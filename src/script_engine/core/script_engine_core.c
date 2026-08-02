/**
 * @file script_engine_core.c
 * @brief Script engine core functionality implementation
 */

#include "script_engine_core.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#define EOS_LOG_TAG "ScriptEngine"
#include "eos_log.h"

#include "lvgl.h"
#include "cJSON.h"

#include "eos_port.h"
#include "eos_core.h"
#include "eos_icon.h"
#include "eos_watchface.h"
#include "eos_app.h"
#include "eos_config.h"
#include "eos_service_storage.h"
#include "eos_mem.h"
#include "eos_pkg_mgr.h"
#include "eos_version.h"
#include "eos_event.h"
#include "eos_cqueue.h"
#include "eos_dispatcher.h"

#include "sni.h"
#include "sni_callback_runtime.h"
#include "spm.h"

/* Macros and Definitions -------------------------------------*/
#define SCRIPT_INIT_FLAGS JERRY_INIT_MEM_STATS
#define SCRIPT_DEFAULT_CQUEUE_CAPACITY 10
#define SCRIPT_ERROR_STACK_BUF_SIZE 256
#define SCRIPT_BACKTRACE_MAX_FRAMES 16
#define SCRIPT_BACKTRACE_SOURCE_SIZE 128
#define SCRIPT_DEFAULT_TIMEOUT_MS 3000
#define TRACKED_MODULES_INIT_CAPACITY 4

/* Module cache for ES module singleton semantics.
 * _module_resolve_cb is called once per import site even for the same
 * file. Without caching each call creates a new module instance with
 * independent variable scopes — breaking ES module guarantees.
 * The cache is keyed by canonical file path and cleared at engine stop. */
#define MODULE_CACHE_MAX 64

typedef struct
{
    char path[512];           /* canonical file path (key) */
    jerry_value_t module;     /* parsed module value (one ref held) */
} _module_cache_entry_t;

static _module_cache_entry_t _module_cache[MODULE_CACHE_MAX];
static int _module_cache_count = 0;

typedef struct
{
    jerry_value_t specifier;
    jerry_value_t user_value;
    jerry_value_t promise;
} _module_task_t;

/* Variables --------------------------------------------------*/

/**
 * @brief Script Engine Runtime — global engine singleton
 */
typedef struct
{
    script_engine_state_t state;
    bool initialized;
    char *error_info;
    script_error_location_t error_location;
    script_error_location_t backtrace[SCRIPT_BACKTRACE_MAX_FRAMES];
    uint32_t backtrace_count;
    uint32_t script_start_time;
    uint32_t script_timeout_ms;
    bool stop_is_timeout;
    bool pending_stop;
    jerry_value_t old_realm;
    script_program_t *current_program;
    script_pkg_t owned_script;

    jmp_buf fatal_jmp_buf;
    bool fatal_recovering;
    bool fatal_scope_active;
    uint32_t engine_gen;
} script_engine_runtime_t;

static script_engine_runtime_t engine_rt = {
    .state = SCRIPT_ENGINE_STATE_UNINITIALIZED,
    .initialized = false,
    .script_start_time = 0,
    .script_timeout_ms = SCRIPT_DEFAULT_TIMEOUT_MS,
    .stop_is_timeout = false,
    .pending_stop = false,
    .old_realm = 0,
    .current_program = NULL,
    .fatal_recovering = false,
    .fatal_scope_active = false,
    .engine_gen = 0,
};

static eos_cqueue_t *_module_queue = NULL;

static jerry_value_t *_tracked_modules = NULL;
static int _tracked_module_count = 0;
static int _tracked_module_capacity = 0;

/*
 * Track only the main module (entry point).
 * Uses jerry_value_copy to maintain a separate reference so that the
 * original parsed_code can be freed independently without causing
 * use-after-free in _release_all_tracked_modules().
 * Dependency modules are NOT tracked here. Their lifetime is managed by
 * JerryScript's module system. When the main module is freed via
 * jerry_value_free, JerryScript automatically releases dependencies.
 * See: JerryScript docs/02.API-REFERENCE.md jerry_module_link example.
 */
static void _track_main_module(jerry_value_t module)
{
    if (jerry_value_is_exception(module))
        return;

    if (_tracked_module_count >= _tracked_module_capacity)
    {
        int new_cap = _tracked_module_capacity > 0 ? _tracked_module_capacity * 2 : TRACKED_MODULES_INIT_CAPACITY;
        jerry_value_t *new_arr = eos_realloc(_tracked_modules, new_cap * sizeof(jerry_value_t));
        if (!new_arr)
            return;
        _tracked_modules = new_arr;
        _tracked_module_capacity = new_cap;
    }
    _tracked_modules[_tracked_module_count++] = jerry_value_copy(module);
}

/*
 * Release all tracked main modules.
 * Since we only track main modules (not dependencies), this is safe.
 * JerryScript's module system handles dependency lifetime automatically.
 */
static void _release_all_tracked_modules(void)
{
    EOS_LOG_D("RELEASE_TRACKED: freeing %d modules", _tracked_module_count);
    for (int i = 0; i < _tracked_module_count; i++)
    {
        jerry_value_free(_tracked_modules[i]);
    }
    _tracked_module_count = 0;

    if (_tracked_modules)
    {
        eos_free(_tracked_modules);
        _tracked_modules = NULL;
        _tracked_module_capacity = 0;
    }

    jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);
}

static script_program_t *_get_prog(void)
{
    return engine_rt.current_program;
}

static const script_pkg_t *_get_pkg(void)
{
    return _get_prog() ? &_get_prog()->script : NULL;
}

static const char *_get_base_path(void)
{
    const script_pkg_t *p = _get_pkg();
    return (p && p->base_path) ? p->base_path : "/";
}

static void _cleanup_module_task(_module_task_t *task)
{
    if (!task)
        return;
    jerry_value_free(task->specifier);
    jerry_value_free(task->user_value);
    jerry_value_free(task->promise);
    eos_free(task);
}

static void _pkg_free(script_pkg_t *p)
{
    if (!p)
        return;
    if (p->base_path)
    {
        eos_free((void *)p->base_path);
        p->base_path = NULL;
    }
    if (p->script_str)
    {
        eos_free((void *)p->script_str);
        p->script_str = NULL;
    }
}

static void _pkg_clone_into(script_pkg_t *dst, const script_pkg_t *src)
{
    memset(dst, 0, sizeof(*dst));
    dst->type = src->type;
    dst->id = src->id ? eos_strdup(src->id) : NULL;
    dst->name = src->name ? eos_strdup(src->name) : NULL;
    dst->version = src->version ? eos_strdup(src->version) : NULL;
    dst->author = src->author ? eos_strdup(src->author) : NULL;
    dst->description = src->description ? eos_strdup(src->description) : NULL;
    dst->script_str = src->script_str ? eos_strdup(src->script_str) : NULL;
    dst->base_path = src->base_path ? eos_strdup(src->base_path) : NULL;

    /* Clone permissions array */
    if (src->permissions && src->permission_count > 0)
    {
        dst->permissions = (const char **)eos_malloc(sizeof(const char *) * (src->permission_count + 1));
        if (dst->permissions)
        {
            for (uint8_t i = 0; i < src->permission_count; i++)
            {
                dst->permissions[i] = src->permissions[i] ? eos_strdup(src->permissions[i]) : NULL;
            }
            dst->permissions[src->permission_count] = NULL;
            dst->permission_count = src->permission_count;
        }
    }

    dst->min_api_level = src->min_api_level;
    dst->target_api_level = src->target_api_level;
}

static void _pkg_free_fields(script_pkg_t *p)
{
    if (!p)
        return;
    if (p->id)
    {
        eos_free((void *)p->id);
        p->id = NULL;
    }
    if (p->name)
    {
        eos_free((void *)p->name);
        p->name = NULL;
    }
    if (p->version)
    {
        eos_free((void *)p->version);
        p->version = NULL;
    }
    if (p->author)
    {
        eos_free((void *)p->author);
        p->author = NULL;
    }
    if (p->description)
    {
        eos_free((void *)p->description);
        p->description = NULL;
    }
    if (p->script_str)
    {
        eos_free((void *)p->script_str);
        p->script_str = NULL;
    }
    if (p->base_path)
    {
        eos_free((void *)p->base_path);
        p->base_path = NULL;
    }
    if (p->permissions)
    {
        for (uint8_t i = 0; i < p->permission_count; i++)
        {
            if (p->permissions[i])
                eos_free((void *)p->permissions[i]);
        }
        eos_free(p->permissions);
        p->permissions = NULL;
        p->permission_count = 0;
    }
}

static jerry_value_t _module_import_cb(const jerry_value_t specifier, const jerry_value_t user_value, void *user_p);
static jerry_value_t _module_resolve_cb(const jerry_value_t specifier, const jerry_value_t referrer, void *user_p);
static jerry_value_t _read_and_parse_module(const char *file_path);
static void _process_module_queue(void);
static void _cleanup_module_queue(void);
static void _set_error_info(const char *msg);
static void _clear_error_info(void);
static void _clear_error_location(void);
static bool _capture_error_backtrace(void);
static void _parse_backtrace_from_js_array(jerry_value_t backtrace_array);
static void _extract_error_location_from_exception(jerry_value_t exception_value);
static void _script_engine_exception_handler(const char *tag, jerry_value_t result);
static jerry_value_t _vm_exec_stop_callback(void *user_p);
static eos_result_t _change_state(script_engine_state_t new_state);
static void _collect_script_garbage(void);
static void _check_mem(void);
static void _engine_cleanup(void);
static eos_result_t _script_engine_stop_and_cleanup(void);
static jerry_value_t _script_engine_create_info(const script_pkg_t *script_package);

/* Realm Management (Encapsulated) ----------------------------*/

static jerry_value_t _realm_create(void)
{
    jerry_value_t realm = jerry_realm();
    EOS_LOG_D("_realm_create: created realm=%p", (void *)realm);
    return realm;
}

static void _realm_save_and_switch(jerry_value_t new_realm)
{
    engine_rt.old_realm = jerry_set_realm(new_realm);
    EOS_LOG_D("_realm_save_and_switch: old_realm=%p, new_realm=%p", (void *)engine_rt.old_realm, (void *)new_realm);
}

static void _realm_assign_to_program(script_program_t *prog, jerry_value_t realm)
{
    if (!prog)
        return;

    if (jerry_value_is_object(prog->realm))
    {
        EOS_LOG_W("_realm_assign_to_program: releasing existing realm=%p", (void *)prog->realm);
        jerry_value_free(prog->realm);
    }

    prog->realm = jerry_value_copy(realm);
    EOS_LOG_D("_realm_assign_to_program: prog=%p, realm=%p (ref copied)", (void *)prog, (void *)prog->realm);
}

static void _realm_release_program(script_program_t *prog)
{
    if (!prog)
        return;

    if (jerry_value_is_object(prog->realm))
    {
        EOS_LOG_D("_realm_release_program: freeing realm=%p for prog=%p", (void *)prog->realm, (void *)prog);
        jerry_value_free(prog->realm);
        prog->realm = jerry_undefined();
    }
}

static void _realm_restore_and_cleanup(void)
{
    if (jerry_value_is_object(engine_rt.old_realm))
    {
        jerry_value_t current = jerry_set_realm(engine_rt.old_realm);
        if (jerry_value_is_object(current))
        {
            EOS_LOG_D("_realm_restore_and_cleanup: restoring old_realm=%p, freed current=%p",
                      (void *)engine_rt.old_realm,
                      (void *)current);
            jerry_value_free(current);
        }
        engine_rt.old_realm = jerry_undefined();
    }
}

/* Function Implementations -----------------------------------*/

script_engine_state_t script_engine_get_state(void)
{
    return engine_rt.state;
}

void script_engine_set_current_program(script_program_t *prog)
{
    engine_rt.current_program = prog;
}

script_program_t *script_engine_get_current_program(void)
{
    return engine_rt.current_program;
}

static eos_result_t _change_state(script_engine_state_t new_state)
{
    switch (engine_rt.state)
    {
        case SCRIPT_ENGINE_STATE_UNINITIALIZED:
            if (new_state != SCRIPT_ENGINE_STATE_IDLE && new_state != SCRIPT_ENGINE_STATE_RUNNING)
            {
                EOS_LOG_E("Invalid transition UNINITIALIZED->%d", new_state);
                return EOS_ERR_INVALID_STATE;
            }
            break;
        case SCRIPT_ENGINE_STATE_RUNNING:
            if (new_state != SCRIPT_ENGINE_STATE_IDLE && new_state != SCRIPT_ENGINE_STATE_EXCEPTION
                && new_state != SCRIPT_ENGINE_STATE_UNINITIALIZED)
            {
                EOS_LOG_E("Invalid transition RUNNING->%d", new_state);
                return EOS_ERR_INVALID_STATE;
            }
            break;
        case SCRIPT_ENGINE_STATE_IDLE:
            if (new_state != SCRIPT_ENGINE_STATE_RUNNING && new_state != SCRIPT_ENGINE_STATE_EXCEPTION
                && new_state != SCRIPT_ENGINE_STATE_UNINITIALIZED)
            {
                EOS_LOG_E("Invalid transition IDLE->%d", new_state);
                return EOS_ERR_INVALID_STATE;
            }
            break;
        case SCRIPT_ENGINE_STATE_EXCEPTION:
            if (new_state != SCRIPT_ENGINE_STATE_IDLE && new_state != SCRIPT_ENGINE_STATE_UNINITIALIZED)
            {
                EOS_LOG_E("Invalid transition EXCEPTION->%d", new_state);
                return EOS_ERR_INVALID_STATE;
            }
            break;
        default:
            return EOS_ERR_INVALID_STATE;
    }
    engine_rt.state = new_state;
    return EOS_OK;
}

static void _check_mem(void)
{
    if (!jerry_feature_enabled(JERRY_FEATURE_HEAP_STATS))
        return;
    jerry_heap_stats_t stats = {0};
    if (jerry_heap_stats(&stats))
        EOS_LOG_D("Heap: size=%d alloc=%d peak=%d", stats.size, stats.allocated_bytes, stats.peak_allocated_bytes);
}

static void _collect_script_garbage(void)
{
    jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);
}

/* Error handling ---------------------------------------------*/

static void _set_error_info(const char *msg)
{
    if (engine_rt.error_info)
    {
        eos_free(engine_rt.error_info);
        engine_rt.error_info = NULL;
    }
    if (!msg)
        return;
    size_t len = strlen(msg);
    if (len > 4096)
        len = 4096;
    engine_rt.error_info = eos_malloc(len + 1);
    if (engine_rt.error_info)
    {
        memcpy(engine_rt.error_info, msg, len);
        engine_rt.error_info[len] = '\0';
    }
}

static void _clear_error_info(void)
{
    if (engine_rt.error_info)
    {
        eos_free(engine_rt.error_info);
        engine_rt.error_info = NULL;
    }
    _clear_error_location();
}

static void _clear_error_location(void)
{
    memset(&engine_rt.error_location, 0, sizeof(script_error_location_t));
    memset(engine_rt.backtrace, 0, sizeof(engine_rt.backtrace));
    engine_rt.backtrace_count = 0;
}

static void _parse_backtrace_from_js_array(jerry_value_t backtrace_array)
{
    if (!jerry_value_is_array(backtrace_array))
        return;
    uint32_t array_len = jerry_array_length(backtrace_array);
    if (array_len == 0)
        return;

    /* Only clear the existing backtrace once we're sure we have
     * data to replace it with. Otherwise _extract_error_location_from_exception
     * may have already populated the backtrace from the exception.stack
     * property, and we'd lose it. */
    _clear_error_location();

    uint32_t max_frames = (array_len > SCRIPT_BACKTRACE_MAX_FRAMES) ? SCRIPT_BACKTRACE_MAX_FRAMES : array_len;
    for (uint32_t i = 0; i < max_frames; i++)
    {
        jerry_value_t element = jerry_object_get_index(backtrace_array, i);
        if (!jerry_value_is_string(element))
        {
            jerry_value_free(element);
            continue;
        }
        jerry_char_t buffer[SCRIPT_ERROR_STACK_BUF_SIZE];
        jerry_size_t copied =
            jerry_string_to_buffer(element, JERRY_ENCODING_UTF8, buffer, SCRIPT_ERROR_STACK_BUF_SIZE - 1);
        buffer[copied] = '\0';
        jerry_value_free(element);
        const char *str = (const char *)buffer;
        const char *file_start = strstr(str, " ");
        file_start = file_start ? file_start + 1 : str;
        const char *colon1 = strchr(file_start, ':');
        if (!colon1)
            continue;
        const char *colon2 = strchr(colon1 + 1, ':');
        if (!colon2)
            continue;
        script_error_location_t *loc = &engine_rt.backtrace[i];
        size_t file_len = colon1 - file_start;
        if (file_len > SCRIPT_BACKTRACE_SOURCE_SIZE - 1)
            file_len = SCRIPT_BACKTRACE_SOURCE_SIZE - 1;
        memcpy(loc->source_name, file_start, file_len);
        loc->source_name[file_len] = '\0';
        loc->line = (uint32_t)atoi(colon1 + 1);
        loc->column = (uint32_t)atoi(colon2 + 1);
        engine_rt.backtrace_count++;
    }
    if (engine_rt.backtrace_count > 0)
        engine_rt.error_location = engine_rt.backtrace[0];
}

static bool _capture_error_backtrace(void)
{
    if (!jerry_feature_enabled(JERRY_FEATURE_LINE_INFO))
    {
        return false;
    }
    jerry_value_t backtrace_array = jerry_backtrace(SCRIPT_BACKTRACE_MAX_FRAMES);
    if (jerry_value_is_exception(backtrace_array))
    {
        jerry_value_free(backtrace_array);
        return false;
    }
    _parse_backtrace_from_js_array(backtrace_array);
    jerry_value_free(backtrace_array);
    return true;
}

static void _extract_error_location_from_exception(jerry_value_t exception_value)
{
    if (!jerry_value_is_object(exception_value))
    {
        return;
    }
    _clear_error_location();
    jerry_value_t stack_prop = jerry_object_get_sz(exception_value, "stack");
    if (jerry_value_is_object(stack_prop) && jerry_value_is_array(stack_prop))
    {
        uint32_t array_len = jerry_array_length(stack_prop);
        for (uint32_t i = 0; i < array_len && engine_rt.backtrace_count < SCRIPT_BACKTRACE_MAX_FRAMES; i++)
        {
            jerry_value_t element = jerry_object_get_index(stack_prop, i);
            if (jerry_value_is_string(element))
            {
                jerry_char_t buffer[SCRIPT_ERROR_STACK_BUF_SIZE];
                jerry_size_t copied = jerry_string_to_buffer(element, JERRY_ENCODING_UTF8, buffer, sizeof(buffer) - 1);
                buffer[copied] = '\0';
                script_error_location_t *loc = &engine_rt.backtrace[engine_rt.backtrace_count];
                const char *line_start = (const char *)buffer;
                const char *colon1 = strchr(line_start, ':');
                const char *colon2 = colon1 ? strchr(colon1 + 1, ':') : NULL;
                if (colon1 && colon2 && colon2 > colon1)
                {
                    size_t name_len = colon1 - line_start;
                    if (name_len > SCRIPT_BACKTRACE_SOURCE_SIZE - 1)
                        name_len = SCRIPT_BACKTRACE_SOURCE_SIZE - 1;
                    memcpy(loc->source_name, line_start, name_len);
                    loc->source_name[name_len] = '\0';
                    loc->line = (uint32_t)atoi(colon1 + 1);
                    loc->column = (uint32_t)atoi(colon2 + 1);
                    if (engine_rt.backtrace_count == 0)
                        engine_rt.error_location = *loc;
                    engine_rt.backtrace_count++;
                }
            }
            jerry_value_free(element);
        }
        jerry_value_free(stack_prop);
        return;
    }
    jerry_value_free(stack_prop);
    jerry_value_t line_prop = jerry_object_get_sz(exception_value, "lineNumber");
    if (jerry_value_is_number(line_prop))
        engine_rt.error_location.line = (uint32_t)jerry_value_as_number(line_prop);
    jerry_value_free(line_prop);
    jerry_value_t col_prop = jerry_object_get_sz(exception_value, "columnNumber");
    if (jerry_value_is_number(col_prop))
        engine_rt.error_location.column = (uint32_t)jerry_value_as_number(col_prop);
    jerry_value_free(col_prop);
}

static void _script_engine_exception_handler(const char *tag, jerry_value_t result)
{
    EOS_LOG_E("===================================");
    jerry_value_t value = jerry_exception_value(result, false);
    jerry_value_t final_str_val = value;
    char stack_buf[SCRIPT_ERROR_STACK_BUF_SIZE];
    char *buf = stack_buf;
    bool need_free = false;
    if (!jerry_value_is_string(value))
        final_str_val = jerry_value_to_string(value);
    jerry_size_t req_sz = jerry_string_size(final_str_val, JERRY_ENCODING_CESU8);
    if (req_sz > 0)
    {
        if (req_sz >= sizeof(stack_buf))
        {
            buf = eos_malloc(req_sz + 1);
            need_free = (buf != NULL);
        }
        if (buf)
        {
            jerry_string_to_buffer(final_str_val, JERRY_ENCODING_CESU8, (jerry_char_t *)buf, req_sz);
            buf[req_sz] = '\0';
            EOS_LOG_E("%s Error: %s", tag, buf);
            _set_error_info(buf);
            if (need_free)
                eos_free(buf);
        }
    }
    else
    {
        _set_error_info("Unknown error");
    }
    if (final_str_val != value)
        jerry_value_free(final_str_val);
    _extract_error_location_from_exception(value);
    jerry_value_free(value);
    if (!_capture_error_backtrace())
    {
        if (!jerry_feature_enabled(JERRY_FEATURE_LINE_INFO))
            EOS_LOG_E("Backtrace disabled (JERRY_LINE_INFO=OFF), enable for stack traces");
        if (!jerry_feature_enabled(JERRY_FEATURE_ERROR_MESSAGES))
            EOS_LOG_E("Error details limited (JERRY_ERROR_MESSAGES=OFF)");
    }
    if (engine_rt.backtrace_count > 0)
    {
        EOS_LOG_E("Backtrace (%u frames):", engine_rt.backtrace_count);
        for (uint32_t i = 0; i < engine_rt.backtrace_count; i++)
        {
            EOS_LOG_E("  #%u %s:%u:%u",
                      i,
                      engine_rt.backtrace[i].source_name,
                      engine_rt.backtrace[i].line,
                      engine_rt.backtrace[i].column);
        }
    }
    else if (jerry_feature_enabled(JERRY_FEATURE_LINE_INFO))
    {
        EOS_LOG_E("(no backtrace frames captured)");
    }
}

/* Query ------------------------------------------------------*/

const char *script_engine_get_error_info(void)
{
    return engine_rt.error_info ? engine_rt.error_info : "";
}

const script_error_location_t *script_engine_get_error_location(void)
{
    return &engine_rt.error_location;
}
uint32_t script_engine_get_backtrace_count(void)
{
    return engine_rt.backtrace_count;
}

const script_error_location_t *script_engine_get_error_backtrace(uint32_t *count)
{
    if (count)
        *count = engine_rt.backtrace_count;
    return engine_rt.backtrace;
}

char *script_engine_get_current_script_id(void)
{
    const script_pkg_t *p = _get_pkg();
    return p ? (char *)p->id : NULL;
}

char *script_engine_get_current_script_name(void)
{
    const script_pkg_t *p = _get_pkg();
    return p ? (char *)p->name : NULL;
}

script_pkg_type_t script_engine_get_current_script_type(void)
{
    const script_pkg_t *p = _get_pkg();
    return p ? p->type : SCRIPT_TYPE_UNKNOWN;
}

bool script_engine_has_permission(const char *perm_name)
{
    if (!perm_name)
        return false;
    const script_pkg_t *p = _get_pkg();
    if (!p || !p->permissions || p->permission_count == 0)
        return false;
    for (uint8_t i = 0; i < p->permission_count; i++)
    {
        if (p->permissions[i] && strcmp(p->permissions[i], perm_name) == 0)
            return true;
    }
    return false;
}

/* Throwing / Registering / Helpers ---------------------------*/

jerry_value_t script_engine_throw_error(const char *message)
{
    jerry_value_t error_obj = jerry_error_sz(JERRY_ERROR_TYPE, (const jerry_char_t *)message);
    return jerry_throw_value(error_obj, true);
}

void script_engine_register_functions(jerry_value_t parent,
                                      const script_engine_func_entry_t *entries,
                                      size_t funcs_count)
{
    for (size_t i = 0; i < funcs_count; ++i)
    {
        jerry_value_t target_obj = parent;
        if (entries[i].class_name)
        {
            jerry_value_t class_key = jerry_string_sz(entries[i].class_name);
            jerry_value_t class_obj = jerry_object_get(parent, class_key);
            if (jerry_value_is_undefined(class_obj))
            {
                jerry_value_free(class_obj);
                class_obj = jerry_object();
                jerry_value_free(jerry_object_set(parent, class_key, class_obj));
            }
            jerry_value_free(class_key);
            target_obj = class_obj;
        }
        jerry_value_t fn = jerry_function_external(entries[i].handler);
        jerry_value_t method_key = jerry_string_sz(entries[i].method_name);
        jerry_value_free(jerry_object_set(target_obj, method_key, fn));
        jerry_value_free(method_key);
        jerry_value_free(fn);
        if (entries[i].class_name)
            jerry_value_free(target_obj);
    }
}

/* JS Call (raw — SPM gates) ----------------------------------*/

jerry_value_t script_engine_call_raw(jerry_value_t func,
                                     jerry_value_t this_val,
                                     const jerry_value_t args_p[],
                                     const jerry_length_t args_count)
{
    script_engine_state_t prev_state = engine_rt.state;
    if (prev_state != SCRIPT_ENGINE_STATE_RUNNING && prev_state != SCRIPT_ENGINE_STATE_IDLE)
    {
        EOS_LOG_W("script_engine_call_raw: rejecting, state=%d", prev_state);
        return jerry_undefined();
    }

    if (prev_state == SCRIPT_ENGINE_STATE_IDLE)
    {
        _change_state(SCRIPT_ENGINE_STATE_RUNNING);
        if (engine_rt.script_timeout_ms > 0)
            engine_rt.script_start_time = eos_tick_get();
    }

    /*
     * If script_engine_run() has already set up a fatal scope, let its
     * setjmp/longjmp handle any assertion failure.  Otherwise, this
     * callback is running "detached" (after script_engine_run returned)
     * and we must guard jerry_call ourselves so that a JS engine crash
     * does not abort() the whole process.
     */
    bool owns_fatal_scope = false;
    int fatal_code = 0;
    if (!engine_rt.fatal_scope_active)
    {
        fatal_code = setjmp(engine_rt.fatal_jmp_buf);
        if (fatal_code == 0)
        {
            owns_fatal_scope = true;
            engine_rt.fatal_scope_active = true;
        }
    }

    jerry_value_t result;

    if (fatal_code != 0)
    {
        /*
         * A JerryScript assertion / fatal error occurred inside
         * jerry_call().  The engine heap is structurally consistent
         * but may contain stale handles.  Perform the same full
         * recovery as script_engine_run does on fatal errors.
         */
        EOS_LOG_E("Callback fatal error (code=%d) — resetting engine", fatal_code);

        /* Build a local error message (matching script_engine_run's pattern)
         * before spm_handle_engine_reset destroys programs.
         * engine_rt.error_info may be NULL here — use the local string. */
        char fatal_msg[64];
        snprintf(fatal_msg, sizeof(fatal_msg), "Engine crash (code=%d)", fatal_code);
        _set_error_info(fatal_msg);

        {
            script_program_t *prog = engine_rt.current_program;
            spm_save_crash_context(prog ? prog->script.id : NULL, prog ? prog->type : SCRIPT_TYPE_UNKNOWN, fatal_msg);
        }

        _cleanup_module_queue();
        _release_all_tracked_modules();
        _engine_cleanup();
        spm_handle_engine_reset();
        script_engine_set_current_program(NULL);

        jerry_init(SCRIPT_INIT_FLAGS);
        sni_init();
        engine_rt.engine_gen++;
        engine_rt.initialized = true;
        engine_rt.fatal_recovering = false;
        engine_rt.fatal_scope_active = false;

        /* Re-enable LVGL timers that were paused by spm_handle_engine_reset. */
        lv_timer_enable(true);

        if (engine_rt.state != SCRIPT_ENGINE_STATE_IDLE)
            _change_state(SCRIPT_ENGINE_STATE_IDLE);
        engine_rt.stop_is_timeout = false;
        engine_rt.pending_stop = false;

        /* Schedule deferred crash notification (after timers re-enabled) */
        spm_schedule_crash_notification();

        result = jerry_undefined();
    }
    else
    {
        result = jerry_call(func, this_val, args_p, args_count);

        if (owns_fatal_scope)
        {
            engine_rt.fatal_scope_active = false;
        }

        if (jerry_value_is_exception(result))
        {
            if (engine_rt.pending_stop)
            {
                if (engine_rt.stop_is_timeout)
                    EOS_LOG_W("Script call timeout");
                else
                    EOS_LOG_D("Script call stopped by request");
            }
            else
            {
                _script_engine_exception_handler("Jerry Call", result);
                _change_state(SCRIPT_ENGINE_STATE_EXCEPTION);
            }
        }

        if (engine_rt.state == SCRIPT_ENGINE_STATE_RUNNING)
            _change_state(SCRIPT_ENGINE_STATE_IDLE);
    }

    return result;
}

/* Timeout ----------------------------------------------------*/

void script_engine_set_timeout(uint32_t timeout_ms)
{
    engine_rt.script_timeout_ms = timeout_ms;
}
uint32_t script_engine_get_timeout(void)
{
    return engine_rt.script_timeout_ms;
}

/* VM halt callback -------------------------------------------*/

static jerry_value_t _vm_exec_stop_callback(void *user_p)
{
    (void)user_p;
    if (engine_rt.pending_stop)
    {
        EOS_LOG_D("Script execution stopped by request");
        return jerry_string_sz("Script terminated by request");
    }
    static uint32_t _halt_tick_skip = 0;
    if (++_halt_tick_skip < 64)
        return jerry_undefined();
    _halt_tick_skip = 0;

    if (engine_rt.script_timeout_ms > 0 && engine_rt.state == SCRIPT_ENGINE_STATE_RUNNING)
    {
        uint32_t elapsed = eos_tick_get() - engine_rt.script_start_time;
        if (elapsed >= engine_rt.script_timeout_ms)
        {
            EOS_LOG_W("Script execution timeout (%u ms)", elapsed);
            engine_rt.stop_is_timeout = true;
            engine_rt.pending_stop = true;
            _change_state(SCRIPT_ENGINE_STATE_EXCEPTION);
            return jerry_string_sz("Script execution timeout");
        }
    }
    return jerry_undefined();
}

/* Init -------------------------------------------------------*/

eos_result_t script_engine_init(void)
{
    if (engine_rt.initialized)
        return EOS_ERR_ALREADY_INITIALIZED;
    if (!jerry_feature_enabled(JERRY_FEATURE_VM_EXEC_STOP) || !jerry_feature_enabled(JERRY_FEATURE_REALM)
        || !jerry_feature_enabled(JERRY_FEATURE_MODULE))
    {
        EOS_LOG_E("Required JerryScript features not enabled");
        return EOS_ERR_SCRIPT_INIT_FAIL;
    }
    if (!lv_is_initialized())
    {
        EOS_LOG_E("LVGL not initialized");
        return EOS_ERR_NOT_INITIALIZED;
    }
    jerry_init(SCRIPT_INIT_FLAGS);
    sni_init();
    engine_rt.initialized = true;
    EOS_LOG_I("Script engine initialized");
    return EOS_OK;
}

/* Module cache helpers ----------------------------------------*/

/* Return a jerry_value_copy of the cached module, or jerry_undefined(). */
static jerry_value_t _module_cache_lookup(const char *canonical_path)
{
    for (int i = 0; i < _module_cache_count; i++) {
        if (strcmp(_module_cache[i].path, canonical_path) == 0) {
            EOS_LOG_D("MODULE CACHE HIT: %s (slot %d)", canonical_path, i);
            return jerry_value_copy(_module_cache[i].module);
        }
    }
    return jerry_undefined();
}

/* Insert a parsed module into the cache. */
static void _module_cache_insert(const char *canonical_path, jerry_value_t module)
{
    if (_module_cache_count >= MODULE_CACHE_MAX) {
        EOS_LOG_W("MODULE CACHE FULL: cannot cache %s", canonical_path);
        return;
    }
    int idx = _module_cache_count++;
    snprintf(_module_cache[idx].path, sizeof(_module_cache[idx].path),
             "%s", canonical_path);
    _module_cache[idx].module = jerry_value_copy(module);
    EOS_LOG_D("MODULE CACHE INSERT: %s (slot %d)", canonical_path, idx);
}

/* Release all cached module references. */
static void _module_cache_clear(void)
{
    EOS_LOG_D("MODULE CACHE CLEAR: freeing %d entries", _module_cache_count);
    for (int i = 0; i < _module_cache_count; i++) {
        jerry_value_free(_module_cache[i].module);
        _module_cache[i].module = jerry_undefined();
        _module_cache[i].path[0] = '\0';
    }
    _module_cache_count = 0;
}

/* Module system ----------------------------------------------*/

static jerry_value_t _module_import_cb(const jerry_value_t specifier, const jerry_value_t user_value, void *user_p)
{
    (void)user_p;
    if (!_module_queue)
    {
        _module_queue = eos_cqueue_create(SCRIPT_DEFAULT_CQUEUE_CAPACITY);
        if (!_module_queue)
            return jerry_throw_sz(JERRY_ERROR_COMMON, "Failed to create module queue");
    }
    _module_task_t *task = eos_malloc_zeroed(sizeof(_module_task_t));
    if (!task)
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Failed to allocate module task");
    task->specifier = jerry_value_copy(specifier);
    task->user_value = jerry_value_copy(user_value);
    jerry_value_t promise = jerry_promise();
    task->promise = jerry_value_copy(promise);
    if (!eos_cqueue_enqueue(_module_queue, task))
    {
        jerry_value_free(task->specifier);
        jerry_value_free(task->user_value);
        jerry_value_free(task->promise);
        eos_free(task);
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Failed to enqueue module task");
    }
    return promise;
}

static jerry_value_t _module_resolve_cb(const jerry_value_t specifier, const jerry_value_t referrer, void *user_p)
{
    (void)referrer;
    (void)user_p;
    jerry_char_t specifier_buffer[256];
    jerry_size_t copied_bytes =
        jerry_string_to_buffer(specifier, JERRY_ENCODING_UTF8, specifier_buffer, sizeof(specifier_buffer) - 1);
    specifier_buffer[copied_bytes] = '\0';

    char full_path[512];
    if (specifier_buffer[0] == '.' && (specifier_buffer[1] == '/' || specifier_buffer[1] == '\\'))
        snprintf(full_path, sizeof(full_path), "%s%s", _get_base_path(), (const char *)specifier_buffer + 2);
    else
        snprintf(full_path, sizeof(full_path), "%s", (const char *)specifier_buffer);

    /* Check module cache first — ensures ES module singleton semantics.
     * Without this, every import site gets a fresh module instance with
     * independent variable scope, breaking shared state. */
    jerry_value_t cached = _module_cache_lookup(full_path);
    if (!jerry_value_is_undefined(cached))
    {
        return cached; /* already a jerry_value_copy from the cache */
    }

    char *source_str = eos_storage_read_file(full_path);
    if (!source_str)
    {
        EOS_LOG_E("Failed to read dependency: %s", full_path);
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Failed to read dependency");
    }
    jerry_size_t file_size = strlen(source_str);
    jerry_parse_options_t parse_options;
    parse_options.options = JERRY_PARSE_MODULE | JERRY_PARSE_HAS_SOURCE_NAME | JERRY_PARSE_HAS_USER_VALUE;
    parse_options.source_name = jerry_string_sz(full_path);
    parse_options.user_value = jerry_string_sz(_get_base_path());
    jerry_value_t result = jerry_parse((const jerry_char_t *)source_str, file_size, &parse_options);
    jerry_value_free(parse_options.source_name);
    jerry_value_free(parse_options.user_value);
    eos_free(source_str);

    if (jerry_value_is_exception(result))
    {
        EOS_LOG_E("DEP PARSE FAILED: %s", specifier_buffer);
    }
    else
    {
        /* Cache the successfully parsed module so subsequent imports
         * of the same file return the same module instance. */
        _module_cache_insert(full_path, result);
    }
    /* Dependency modules are NOT tracked here. Their lifetime is managed by
     * JerryScript's module system. When the main module is freed via
     * jerry_value_free, JerryScript automatically releases dependencies. */
    return result;
}

static jerry_value_t _read_and_parse_module(const char *file_path)
{
    char *source_str = eos_storage_read_file(file_path);
    if (!source_str)
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Failed to read module file");
    jerry_size_t file_size = strlen(source_str);
    jerry_parse_options_t parse_options;
    parse_options.options = JERRY_PARSE_MODULE | JERRY_PARSE_HAS_SOURCE_NAME | JERRY_PARSE_HAS_USER_VALUE;
    parse_options.source_name = jerry_string_sz(file_path);
    parse_options.user_value = jerry_string_sz(_get_base_path());
    jerry_value_t result = jerry_parse((const jerry_char_t *)source_str, file_size, &parse_options);
    jerry_value_free(parse_options.source_name);
    jerry_value_free(parse_options.user_value);
    eos_free(source_str);
    return result;
}

static void _process_module_queue(void)
{
    if (!_module_queue)
        return;
    int processed = 0;
    while (eos_cqueue_get_size(_module_queue) > 0)
    {
        _module_task_t *task = (_module_task_t *)eos_cqueue_dequeue(_module_queue);
        if (!task)
            continue;
        jerry_char_t specifier_buffer[256];
        jerry_size_t copied_bytes = jerry_string_to_buffer(task->specifier,
                                                           JERRY_ENCODING_UTF8,
                                                           specifier_buffer,
                                                           sizeof(specifier_buffer) - 1);
        specifier_buffer[copied_bytes] = '\0';

        jerry_value_t module_value = _read_and_parse_module((const char *)specifier_buffer);
        if (jerry_value_is_exception(module_value))
        {
            _cleanup_module_task(task);
            continue;
        }

        EOS_LOG_D("MODULE QUEUE: parsed %s", specifier_buffer);

        jerry_value_t link_result = jerry_module_link(module_value, _module_resolve_cb, NULL);
        if (jerry_value_is_exception(link_result))
        {
            jerry_value_t exc = jerry_exception_value(link_result, false);
            jerry_char_t buf[128];
            jerry_size_t sz = jerry_string_to_buffer(exc, JERRY_ENCODING_UTF8, buf, sizeof(buf) - 1);
            buf[sz] = '\0';
            jerry_value_free(exc);
            EOS_LOG_E("MODULE QUEUE LINK FAILED: %s - %s", specifier_buffer, buf);
            jerry_value_free(link_result);
            jerry_value_free(module_value);
            _cleanup_module_task(task);
            continue;
        }
        jerry_value_free(link_result);
        jerry_value_t eval_result = jerry_module_evaluate(module_value);
        if (jerry_value_is_exception(eval_result))
        {
            EOS_LOG_E("Failed to evaluate module: %s", (const char *)specifier_buffer);
            jerry_value_free(eval_result);
            jerry_value_free(module_value);
            _cleanup_module_task(task);
            continue;
        }
        jerry_value_free(eval_result);
        jerry_value_t namespace_value = jerry_module_namespace(module_value);
        jerry_value_t resolve_result = jerry_promise_resolve(task->promise, namespace_value);
        if (jerry_value_is_exception(resolve_result))
            jerry_value_free(resolve_result);
        jerry_value_free(namespace_value);
        jerry_value_free(module_value);
        _cleanup_module_task(task);
        processed++;
    }
    EOS_LOG_D("MODULE QUEUE: processed %d modules", processed);
}

static void _cleanup_module_queue(void)
{
    if (!_module_queue)
        return;
    while (eos_cqueue_get_size(_module_queue) > 0)
    {
        _module_task_t *task = (_module_task_t *)eos_cqueue_dequeue(_module_queue);
        if (task)
        {
            jerry_value_free(task->specifier);
            jerry_value_free(task->user_value);
            jerry_value_free(task->promise);
            eos_free(task);
        }
    }
    eos_cqueue_destroy(_module_queue);
    _module_queue = NULL;

    /* Clear the module resolve cache so the next program / engine
     * restart starts with a clean slate. */
    _module_cache_clear();
}

/* Manifest ---------------------------------------------------*/

eos_result_t script_engine_get_manifest(const char *manifest_path, script_pkg_t *pkg)
{
    if (!manifest_path || !pkg)
        return EOS_ERR_SCRIPT_NULL_PACKAGE;
    char *manifest_json = eos_storage_read_file(manifest_path);
    if (!manifest_json)
    {
        EOS_LOG_E("Read manifest.json failed");
        return EOS_FAILED;
    }
    cJSON *root = cJSON_Parse(manifest_json);
    eos_free(manifest_json);
    if (!root)
        return EOS_FAILED;
    cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    cJSON *author = cJSON_GetObjectItemCaseSensitive(root, "author");
    cJSON *description = cJSON_GetObjectItemCaseSensitive(root, "description");
    if (!cJSON_IsString(id) || !id->valuestring || !cJSON_IsString(name) || !name->valuestring
        || !cJSON_IsString(version) || !version->valuestring || !cJSON_IsString(author) || !author->valuestring
        || !cJSON_IsString(description) || !description->valuestring)
    {
        cJSON_Delete(root);
        return EOS_FAILED;
    }

    cJSON *min_api = cJSON_GetObjectItemCaseSensitive(root, "minApiLevel");
    cJSON *target_api = cJSON_GetObjectItemCaseSensitive(root, "targetApiLevel");
    if (!cJSON_IsNumber(min_api) || !cJSON_IsNumber(target_api))
    {
        EOS_LOG_E("Manifest missing required number fields: minApiLevel, targetApiLevel");
        cJSON_Delete(root);
        return EOS_FAILED;
    }
    if (pkg->id)
        eos_free((void *)pkg->id);
    if (pkg->name)
        eos_free((void *)pkg->name);
    if (pkg->version)
        eos_free((void *)pkg->version);
    if (pkg->author)
        eos_free((void *)pkg->author);
    if (pkg->description)
        eos_free((void *)pkg->description);
    pkg->id = eos_strdup(id->valuestring);
    pkg->name = eos_strdup(name->valuestring);
    pkg->version = eos_strdup(version->valuestring);
    pkg->author = eos_strdup(author->valuestring);
    pkg->description = eos_strdup(description->valuestring);
    pkg->min_api_level = (uint16_t)cJSON_GetNumberValue(min_api);
    pkg->target_api_level = (uint16_t)cJSON_GetNumberValue(target_api);

    /* Parse optional "permissions" array */
    cJSON *permissions = cJSON_GetObjectItemCaseSensitive(root, "permissions");
    if (permissions && cJSON_IsArray(permissions))
    {
        int perm_count = cJSON_GetArraySize(permissions);
        if (perm_count > 0)
        {
            /* Known permission names for validation */
            static const char *known_perms[] = {"location",
                                                "sensor",
                                                "notification",
                                                "storage",
                                                "bluetooth",
                                                "audio",
                                                "health",
                                                "contacts",
                                                "calendar",
                                                NULL};

            pkg->permissions = (const char **)eos_malloc(sizeof(const char *) * (perm_count + 1));
            if (pkg->permissions)
            {
                int valid_idx = 0;
                for (int i = 0; i < perm_count; i++)
                {
                    cJSON *perm_item = cJSON_GetArrayItem(permissions, i);
                    if (cJSON_IsString(perm_item) && perm_item->valuestring)
                    {
                        /* Validate against known permission names */
                        bool known = false;
                        for (int k = 0; known_perms[k] != NULL; k++)
                        {
                            if (strcmp(perm_item->valuestring, known_perms[k]) == 0)
                            {
                                known = true;
                                break;
                            }
                        }
                        if (known)
                        {
                            pkg->permissions[valid_idx] = eos_strdup(perm_item->valuestring);
                            valid_idx++;
                        }
                        else
                        {
                            EOS_LOG_W("Unknown permission in manifest: %s", perm_item->valuestring);
                        }
                    }
                }
                pkg->permissions[valid_idx] = NULL; /* NULL-terminate */
                pkg->permission_count = (uint8_t)valid_idx;
            }
        }
    }
    else
    {
        pkg->permissions = NULL;
        pkg->permission_count = 0;
    }

    cJSON_Delete(root);
    return EOS_OK;
}

/* Run --------------------------------------------------------*/

static jerry_value_t _script_engine_create_info(const script_pkg_t *pkg)
{
    jerry_value_t obj = jerry_object();
    script_engine_set_prop_string(obj, "id", pkg->id);
    script_engine_set_prop_string(obj, "name", pkg->name);
    script_engine_set_prop_string(obj, "version", pkg->version);
    script_engine_set_prop_string(obj, "author", pkg->author);
    script_engine_set_prop_string(obj, "description", pkg->description);
    script_engine_set_prop_number(obj, "minApiLevel", pkg->min_api_level);
    script_engine_set_prop_number(obj, "targetApiLevel", pkg->target_api_level);
    return obj;
}

eos_result_t script_engine_run(const script_pkg_t *script_package)
{
    if (!script_package || !script_package->script_str)
        return EOS_ERR_SCRIPT_NULL_PACKAGE;
    if (!engine_rt.initialized)
    {
        EOS_LOG_E("Not initialized");
        return EOS_ERR_NOT_INITIALIZED;
    }
    if (engine_rt.state != SCRIPT_ENGINE_STATE_UNINITIALIZED && engine_rt.state != SCRIPT_ENGINE_STATE_IDLE)
    {
        EOS_LOG_E("Cannot run in state %d", engine_rt.state);
        return EOS_ERR_INVALID_STATE;
    }

    if (script_package->min_api_level > ELENIX_OS_API_LEVEL)
    {
        EOS_LOG_E("Package '%s' requires API level %d, OS only supports %d",
                  script_package->id ? script_package->id : "unknown",
                  script_package->min_api_level,
                  ELENIX_OS_API_LEVEL);
        return EOS_ERR_SDK_VERSION;
    }

    /* Clear stale error info from previous program runs before starting fresh */
    _clear_error_info();

    _pkg_clone_into(&engine_rt.owned_script, script_package);
    script_program_t *prog = _get_prog();

    /* Fatal error recovery point (setjmp for jerry_port_fatal longjmp) -*/
    int fatal_code = setjmp(engine_rt.fatal_jmp_buf);
    engine_rt.fatal_scope_active = true;
    EOS_LOG_D("ENGINE_RUN: entered fatal_code=%d", fatal_code);
    if (fatal_code != 0)
    {
        EOS_LOG_W("Engine recovered from fatal error (code=%d)", fatal_code);
        const char *fatal_desc;
        switch (fatal_code)
        {
            case 10:
                fatal_desc = "Out of memory";
                break;
            case 12:
                fatal_desc = "Ref count limit";
                break;
            case 13:
                fatal_desc = "Disabled byte code";
                break;
            case 14:
                fatal_desc = "GC loop limit";
                break;
            case 120:
                fatal_desc = "Assertion failed";
                break;
            default:
                fatal_desc = "Unknown";
                break;
        }
        char fatal_msg[64];
        snprintf(fatal_msg, sizeof(fatal_msg), "Engine crash (code=%d, %s)", fatal_code, fatal_desc);
        _set_error_info(fatal_msg);

        /* Save crash context BEFORE spm_handle_engine_reset destroys programs.
         * Use engine_rt.current_program (global) instead of local prog because
         * longjmp may leave local variables indeterminate. */
        {
            script_program_t *crash_prog = engine_rt.current_program;
            spm_save_crash_context(crash_prog ? crash_prog->script.id : NULL,
                                   crash_prog ? crash_prog->type : SCRIPT_TYPE_UNKNOWN,
                                   fatal_msg);
        }

        /*
         * CRITICAL ORDERING: Clean up ALL stale JS handles and C resources
         * BEFORE jerry_init() wipes the heap.
         *
         * jerry_cleanup() is NOT called — the heap is full but structurally
         * consistent, so jerry_value_free() on old handles still works.
         * jerry_init() does memset(&context, 0, ...) + jmem_init() + ecma_init(),
         * which fully resets all engine state.
         * See: jerry-core/api/jerryscript.c:jerry_init()
         */

        /* Release module queue tasks (hold jerry_value_t handles) */
        _cleanup_module_queue();

        /* Release tracked module references (hold jerry_value_t handles) */
        _release_all_tracked_modules();

        /* Clean up engine-level C state (old_realm, owned_script) */
        _engine_cleanup();

        /* Destroy all SPM programs — frees realm & sni ctx handles
         *    on the old heap. After this, no stale JS handles remain. */
        spm_handle_engine_reset();

        /* Clear current_program pointer (the program was already destroyed) */
        script_engine_set_current_program(NULL);

        /* Now safe to reinitialize — heap gets memset'd + reinit */
        jerry_init(SCRIPT_INIT_FLAGS);
        sni_init();
        engine_rt.engine_gen++;
        engine_rt.initialized = true;
        engine_rt.fatal_recovering = false;

        /* Re-enable LVGL timers that were paused by spm_handle_engine_reset. */
        lv_timer_enable(true);

        /* NOTE: Do NOT schedule crash notification here — script_engine_run()
         * returns EOS_ERR_SCRIPT_EXCEPTION, and the caller (_app_on_enter /
         * _js_on_enter) already handles error display. Only script_engine_call_raw
         * needs deferred notification because its error is silently swallowed. */

        /* Reset runtime state */
        if (engine_rt.state != SCRIPT_ENGINE_STATE_IDLE)
            _change_state(SCRIPT_ENGINE_STATE_IDLE);
        engine_rt.stop_is_timeout = false;
        engine_rt.pending_stop = false;
        return EOS_ERR_SCRIPT_EXCEPTION;
    }

    engine_rt.script_start_time = eos_tick_get();
    EOS_LOG_D("ENGINE_RUN: normal flow start");
    _change_state(SCRIPT_ENGINE_STATE_RUNNING);

    jerry_value_t new_realm = _realm_create();
    _realm_save_and_switch(new_realm);

    sni_mount(new_realm);

    jerry_halt_handler(16, _vm_exec_stop_callback, NULL);
    jerry_log_set_level(JERRY_LOG_LEVEL_DEBUG);

    jerry_value_t global = jerry_current_realm();
    jerry_value_t script_info = _script_engine_create_info(script_package);
    jerry_value_t key = jerry_string_sz("scriptInfo");
    jerry_value_free(jerry_object_set(global, key, script_info));
    jerry_value_free(key);
    jerry_value_free(script_info);
    jerry_value_free(global);

    jerry_module_on_import(_module_import_cb, NULL);

    jerry_parse_options_t parse_options;
    parse_options.options = JERRY_PARSE_MODULE | JERRY_PARSE_HAS_SOURCE_NAME | JERRY_PARSE_HAS_USER_VALUE;
    parse_options.source_name = jerry_string_sz("main.js");
    parse_options.user_value =
        jerry_string_sz(engine_rt.owned_script.base_path ? engine_rt.owned_script.base_path : "/");
    jerry_value_t parsed_code = jerry_parse((const jerry_char_t *)script_package->script_str,
                                            strlen(script_package->script_str),
                                            &parse_options);
    jerry_value_free(parse_options.source_name);
    jerry_value_free(parse_options.user_value);
    if (!jerry_value_is_exception(parsed_code))
        _track_main_module(parsed_code);

    eos_free((void *)engine_rt.owned_script.script_str);
    engine_rt.owned_script.script_str = NULL;

    eos_result_t result = EOS_OK;

    if (jerry_value_is_exception(parsed_code))
    {
        _script_engine_exception_handler("Script Parse", parsed_code);
        _change_state(SCRIPT_ENGINE_STATE_EXCEPTION);
        result = EOS_ERR_SCRIPT_INVALID_JS;
    }
    else
    {
        jerry_value_t link_result = jerry_module_link(parsed_code, _module_resolve_cb, NULL);
        if (jerry_value_is_exception(link_result))
        {
            jerry_value_t exc = jerry_exception_value(link_result, false);
            jerry_char_t buf[256];
            jerry_size_t sz = jerry_string_to_buffer(exc, JERRY_ENCODING_UTF8, buf, sizeof(buf) - 1);
            buf[sz] = '\0';
            jerry_value_free(exc);

            EOS_LOG_E("Module Link Exception: %s", buf);

            _script_engine_exception_handler("Module Link", link_result);
            _change_state(SCRIPT_ENGINE_STATE_EXCEPTION);
            jerry_value_free(link_result);
            result = EOS_ERR_SCRIPT_EXCEPTION;
        }
        else
        {
            jerry_value_free(link_result);
            jerry_value_t run_result = jerry_module_evaluate(parsed_code);
            if (jerry_value_is_exception(run_result))
            {
                if (engine_rt.pending_stop)
                {
                    if (engine_rt.stop_is_timeout)
                        result = EOS_ERR_TIMEOUT;
                    else
                        result = EOS_OK;
                }
                else
                {
                    _script_engine_exception_handler("Script Runtime", run_result);
                    _change_state(SCRIPT_ENGINE_STATE_EXCEPTION);
                    result = EOS_ERR_SCRIPT_EXCEPTION;
                }
            }
            else
            {
                if (engine_rt.state == SCRIPT_ENGINE_STATE_RUNNING)
                    _change_state(SCRIPT_ENGINE_STATE_IDLE);
                eos_event_post(EOS_EVENT_SCRIPT_STARTED, NULL, NULL);
            }
            jerry_value_free(run_result);
        }
    }
    _process_module_queue();
    jerry_value_free(jerry_run_jobs());
    _cleanup_module_queue();

    if (prog)
    {
        _realm_assign_to_program(prog, new_realm);
    }

    /*
     * Release the parsed_code reference. The tracked copy (from
     * _track_main_module with jerry_value_copy) is kept for cleanup
     * at stop time.
     */
    jerry_value_free(parsed_code);

    /*
     * Do NOT release tracked modules here. The module must remain alive
     * for the duration of the script's execution, as callbacks (timers,
     * animations, etc.) may still reference functions defined in the module.
     *
     * Modules are released in _script_engine_stop_and_cleanup() when the
     * script is terminated.
     */

    jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);

    /*
     * Restore the boot realm and clean up the local realm reference.
     * The program's own realm reference (assigned via _realm_assign_to_program
     * above) is intentionally kept alive — callbacks (timers, event handlers,
     * etc.) registered by the script may still reference objects in this realm.
     * It will be freed in _program_destroy() when the program terminates.
     */
    _realm_restore_and_cleanup();
    _cleanup_module_queue();

    if (result != EOS_OK || engine_rt.pending_stop)
    {
        if (engine_rt.state != SCRIPT_ENGINE_STATE_IDLE)
            _change_state(SCRIPT_ENGINE_STATE_IDLE);
        engine_rt.stop_is_timeout = false;
        engine_rt.pending_stop = false;
    }

    engine_rt.fatal_scope_active = false;
    EOS_LOG_D("ENGINE_RUN: returning result=%d", result);
    return result;
}

/* Stop / Cleanup ---------------------------------------------*/

static void _engine_cleanup(void)
{
    _realm_restore_and_cleanup();
    _pkg_free(&engine_rt.owned_script);
    memset(&engine_rt.owned_script, 0, sizeof(engine_rt.owned_script));
}

static eos_result_t _script_engine_stop_and_cleanup(void)
{
    EOS_LOG_I("Script stop cleanup: state=%d prog=%p", engine_rt.state, (void *)_get_prog());
    script_program_t *prog = _get_prog();

    _collect_script_garbage();
    _check_mem();

    _engine_cleanup();
    _realm_release_program(prog);

    _cleanup_module_queue();

    _release_all_tracked_modules();

    for (int i = 0; i < 3; i++)
        jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);

    jerry_module_cleanup(jerry_undefined());

    engine_rt.pending_stop = false;
    if (engine_rt.state != SCRIPT_ENGINE_STATE_IDLE)
        _change_state(SCRIPT_ENGINE_STATE_IDLE);

    _collect_script_garbage();

    for (int i = 0; i < 3; i++)
        jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);

    _check_mem();
    /* Clear error state — next program starts with zero error state */
    _clear_error_info();
    EOS_LOG_I("Script terminated");
    return EOS_OK;
}

eos_result_t script_engine_stop(void)
{
    EOS_LOG_I("Stop script (sync) state=%d", engine_rt.state);
    switch (engine_rt.state)
    {
        case SCRIPT_ENGINE_STATE_UNINITIALIZED:
            return EOS_OK;
        case SCRIPT_ENGINE_STATE_RUNNING:
        case SCRIPT_ENGINE_STATE_EXCEPTION:
        case SCRIPT_ENGINE_STATE_IDLE:
            return _script_engine_stop_and_cleanup();
        default:
            return EOS_ERR_INVALID_STATE;
    }
}

eos_result_t script_engine_request_stop(void)
{
    EOS_LOG_I("Request stop script state=%d", engine_rt.state);
    switch (engine_rt.state)
    {
        case SCRIPT_ENGINE_STATE_UNINITIALIZED:
            return EOS_OK;
        case SCRIPT_ENGINE_STATE_RUNNING:
            engine_rt.stop_is_timeout = false;
            engine_rt.pending_stop = true;
            eos_dispatcher_call((eos_dispatcher_cb_t)_script_engine_stop_and_cleanup, NULL);
            return EOS_OK;
        case SCRIPT_ENGINE_STATE_IDLE:
        case SCRIPT_ENGINE_STATE_EXCEPTION:
            return _script_engine_stop_and_cleanup();
        default:
            return EOS_ERR_INVALID_STATE;
    }
}

eos_result_t script_engine_clean_up(void)
{
    if (engine_rt.state != SCRIPT_ENGINE_STATE_UNINITIALIZED)
        script_engine_request_stop();
    _cleanup_module_queue();
    jerry_module_cleanup(jerry_undefined());
    jerry_cleanup();
    engine_rt.initialized = false;
    _change_state(SCRIPT_ENGINE_STATE_UNINITIALIZED);
    return EOS_OK;
}

/* Reload -----------------------------------------------------*/

eos_result_t script_engine_reload_current_script(void)
{
    if (!engine_rt.initialized)
        return EOS_ERR_NOT_INITIALIZED;
    const script_pkg_t *p = _get_pkg();
    if (!p || !p->id || !p->base_path)
        return EOS_ERR_SCRIPT_NOT_RUNNING;
    script_engine_state_t state = engine_rt.state;
    if (state != SCRIPT_ENGINE_STATE_UNINITIALIZED)
    {
        eos_result_t sr = script_engine_request_stop();
        if (sr != EOS_OK && engine_rt.state != SCRIPT_ENGINE_STATE_UNINITIALIZED)
            return sr;
    }

    script_pkg_t pkg = {0};
    pkg.type = p->type;
    const char *mf =
        (pkg.type == SCRIPT_TYPE_APPLICATION) ? EOS_APP_MANIFEST_FILE_NAME : EOS_WATCHFACE_MANIFEST_FILE_NAME;
    const char *ef =
        (pkg.type == SCRIPT_TYPE_APPLICATION) ? EOS_APP_SCRIPT_ENTRY_FILE_NAME : EOS_WATCHFACE_SCRIPT_ENTRY_FILE_NAME;
    char base_path_buf[EOS_FS_PATH_MAX];
    snprintf(base_path_buf, sizeof(base_path_buf), "%s", p->base_path);
    char manifest_path[EOS_FS_PATH_MAX];
    snprintf(manifest_path, sizeof(manifest_path), "%s%s", base_path_buf, mf);
    if (script_engine_get_manifest(manifest_path, &pkg) != EOS_OK)
        return EOS_FAILED;
    char script_path[EOS_FS_PATH_MAX];
    snprintf(script_path, sizeof(script_path), "%s%s", base_path_buf, ef);
    pkg.base_path = eos_strdup(base_path_buf);
    if (!eos_storage_is_file(script_path))
    {
        eos_pkg_free(&pkg);
        return EOS_FAILED;
    }
    pkg.script_str = eos_storage_read_file(script_path);
    if (!pkg.script_str)
    {
        eos_pkg_free(&pkg);
        return EOS_FAILED;
    }
    eos_result_t run_ret = script_engine_run(&pkg);
    eos_pkg_free(&pkg);
    return run_ret;
}

eos_result_t script_engine_reload_current_app(void)
{
    return script_engine_reload_current_script();
}

uint32_t script_engine_get_gen(void)
{
    return engine_rt.engine_gen;
}

bool script_engine_is_fatal_scope_active(void)
{
    return engine_rt.fatal_scope_active;
}

bool script_engine_is_fatal_recovering(void)
{
    return engine_rt.fatal_recovering;
}

void script_engine_set_fatal_recovering(bool recovering)
{
    engine_rt.fatal_recovering = recovering;
}

void JERRY_ATTR_NORETURN script_engine_fatal_longjmp(int code)
{
    longjmp(engine_rt.fatal_jmp_buf, code);
    /* unreachable */
    while (1)
    {
    }
}
