/**
 * @file spm.c
 * @brief Script Program Manager implementation
 */

#include "spm.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#define EOS_LOG_TAG "SPM"
#include "eos_log.h"
#include "eos_mem.h"
#include "eos_event.h"
#include "eos_dispatcher.h"
#include "eos_recent_apps.h"
#include "sni_context.h"
#include "sni_callback_runtime.h"
#include "jerryscript.h"
#include "lvgl.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

static script_program_t *s_program_list = NULL;
static bool s_initialized = false;
static spm_error_t s_last_error = {0};
static bool s_has_last_error = false;
static spm_crash_state_t s_crash_state = {0};

/* Function Implementations -----------------------------------*/
static void _lvgl_view_clean(void *view)
{
    if (view)
        lv_obj_clean((lv_obj_t *)view);
}

static void _program_list_add(script_program_t *prog);
static void _program_list_remove(script_program_t *prog);
static void _program_destroy(script_program_t *prog);
static eos_result_t _error_copy_from_core(script_program_t *prog);

static void _program_list_add(script_program_t *prog)
{
    if (!prog)
        return;
    prog->next = s_program_list;
    prog->prev = NULL;
    if (s_program_list)
        s_program_list->prev = prog;
    s_program_list = prog;
}

static void _program_list_remove(script_program_t *prog)
{
    if (!prog)
        return;
    if (prog->prev)
        prog->prev->next = prog->next;
    else
        s_program_list = prog->next;
    if (prog->next)
        prog->next->prev = prog->prev;
    prog->prev = NULL;
    prog->next = NULL;
}

static void _script_free(script_pkg_t *p)
{
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

static void _program_destroy(script_program_t *prog)
{
    if (!prog)
        return;
    EOS_LOG_D("Destroying program %p type=%d state=%d sni_ctx=%p",
              (void *)prog,
              prog->type,
              prog->state,
              (void *)prog->sni_ctx);

    if (prog->sni_ctx)
    {
        /* sni_cb_context_cleanup_events already called in Phase 2 before
         * script_engine_stop — event list is empty by this point. */
        sni_context_sweep_all(prog->sni_ctx);
        sni_context_destroy(prog->sni_ctx);
        prog->sni_ctx = NULL;
    }
    if (jerry_value_is_object(prog->realm))
    {
        jerry_value_free(prog->realm);
        prog->realm = jerry_undefined();
    }
    _script_free(&prog->script);
    eos_free(prog);
}

static eos_result_t _error_copy_from_core(script_program_t *prog)
{
    if (!prog)
        return EOS_ERR_SCRIPT_NULL_PACKAGE;
    prog->has_error = true;
    const char *err_info = script_engine_get_error_info();
    if (err_info)
        snprintf(prog->error.error_info, SPM_ERROR_INFO_MAX, "%s", err_info);
    else
        prog->error.error_info[0] = '\0';
    const script_error_location_t *loc = script_engine_get_error_location();
    if (loc)
        memcpy(&prog->error.error_location, loc, sizeof(script_error_location_t));
    const script_error_location_t *bt = script_engine_get_error_backtrace(&prog->error.backtrace_count);
    if (bt && prog->error.backtrace_count > 0)
    {
        uint32_t frames = prog->error.backtrace_count;
        if (frames > SPM_BACKTRACE_MAX_FRAMES)
            frames = SPM_BACKTRACE_MAX_FRAMES;
        memcpy(prog->error.backtrace, bt, frames * sizeof(script_error_location_t));
        prog->error.backtrace_count = frames;
    }
    return EOS_OK;
}

static void _pkg_clone(script_pkg_t *dst, const script_pkg_t *src)
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

/* SPM Lifecycle ----------------------------------------------*/

eos_result_t spm_init(void)
{
    if (s_initialized)
        return EOS_OK;
    s_program_list = NULL;
    s_initialized = true;
    EOS_LOG_I("SPM initialized");
    return EOS_OK;
}

/* Program Lifecycle ------------------------------------------*/

script_program_t *spm_start_program(const script_pkg_t *pkg)
{
    if (!pkg || !pkg->script_str)
    {
        EOS_LOG_E("spm_start_program: null package");
        return NULL;
    }
    if (!s_initialized)
    {
        EOS_LOG_E("spm_start_program: SPM not initialized");
        return NULL;
    }

    script_program_t *prog = eos_malloc_zeroed(sizeof(script_program_t));
    if (!prog)
    {
        EOS_LOG_E("spm_start_program: malloc failed");
        return NULL;
    }

    prog->type = pkg->type;
    prog->state = SCRIPT_PROGRAM_STATE_ACTIVE;
    prog->realm = jerry_undefined();
    _pkg_clone(&prog->script, pkg);

    prog->sni_ctx = sni_context_create();
    if (!prog->sni_ctx)
    {
        EOS_LOG_E("spm_start_program: sni_context_create failed");
        _script_free(&prog->script);
        eos_free(prog);
        return NULL;
    }
    prog->sni_ctx->owner = prog;

    _program_list_add(prog);
    EOS_LOG_I("Starting program %p type=%d id=%s", (void *)prog, prog->type, pkg->id ? pkg->id : "unknown");

    script_engine_set_current_program(prog);
    eos_result_t ret = script_engine_run(pkg);

    if (ret != EOS_OK)
    {
        /* If fatal engine recovery happened, spm_handle_engine_reset()
         * already destroyed ALL programs (including this one). The prog
         * pointer is now freed — skip all cleanup. */
        if (ret == EOS_ERR_SCRIPT_EXCEPTION && s_program_list == NULL)
        {
            const char *err = script_engine_get_error_info();
            if (err && err[0])
            {
                s_has_last_error = true;
                memset(&s_last_error, 0, sizeof(spm_error_t));
                snprintf(s_last_error.error_info, SPM_ERROR_INFO_MAX, "%s", err);
                s_last_error.error_type = EOS_SCRIPT_FAULT_ENGINE_CRASH;
            }
            script_engine_set_current_program(NULL);
            return NULL;
        }

        /* Capture error info BEFORE script_engine_stop clears it */
        _error_copy_from_core(prog);
        /* Save a persistent copy before destroying the program so the
         * fault panel can read the backtrace after the program is gone. */
        memcpy(&s_last_error, &prog->error, sizeof(spm_error_t));
        s_has_last_error = true;

        /* Phase 0: JS-Native decouple — clear native_ptrs BEFORE stopping
         * the engine so JerryScript GC doesn't fire native free callbacks
         * on still-linked resource nodes. */
        if (prog->sni_ctx)
        {
            prog->sni_ctx->teardown_phase = SNI_TEARDOWN_PHASE_JS_DECOUPLE;
            sni_context_clear_native_ptrs_all(prog->sni_ctx);
        }

        /* Phase 1: Release JS callback refs (timers, anims) while
         * the JS heap is still alive so jerry_value_free works. */
        if (prog->sni_ctx)
        {
            prog->sni_ctx->teardown_phase = SNI_TEARDOWN_PHASE_JS_REFS;
            sni_context_sweep_js_refs(prog->sni_ctx);
        }

        /* Phase 2: Clean up LVGL event callbacks BEFORE engine stop
         * (see spm_terminate_program for rationale) */
        if (prog->sni_ctx)
        {
            prog->sni_ctx->teardown_phase = SNI_TEARDOWN_PHASE_LVGL_EVENTS;
            sni_cb_context_cleanup_events(prog->sni_ctx);
        }

        script_engine_stop();
        if (prog->sni_ctx)
            prog->sni_ctx->teardown_phase = SNI_TEARDOWN_PHASE_ENGINE_STOPPED;
        EOS_LOG_E("spm_start_program: execution failed ret=%d", ret);
        _program_list_remove(prog);
        _program_destroy(prog);
        script_engine_set_current_program(NULL);
        return NULL;
    }

    EOS_LOG_I("Program %p started successfully realm=%u sni_ctx=%p",
              (void *)prog,
              (unsigned)prog->realm,
              (void *)prog->sni_ctx);
    /* Clear persistent error copy — a new program started, old errors are stale */
    s_has_last_error = false;
    memset(&s_last_error, 0, sizeof(spm_error_t));
    return prog;
}

eos_result_t spm_suspend_program(script_program_t *prog)
{
    if (!prog)
        return EOS_ERR_SCRIPT_NULL_PACKAGE;
    if (prog->type != SCRIPT_TYPE_WATCHFACE && prog->type != SCRIPT_TYPE_APPLICATION)
        return EOS_ERR_SCRIPT_INVALID_TYPE;
    if (prog->state != SCRIPT_PROGRAM_STATE_ACTIVE)
        return EOS_ERR_INVALID_STATE;

    script_engine_state_t core_state = script_engine_get_state();
    if (core_state != SCRIPT_ENGINE_STATE_IDLE)
    {
        EOS_LOG_E("spm_suspend_program: Core state=%d not IDLE", core_state);
        return EOS_ERR_INVALID_STATE;
    }

    if (prog->sni_ctx)
        sni_context_set_paused(prog->sni_ctx, true);
    prog->state = SCRIPT_PROGRAM_STATE_SUSPENDED;
    EOS_LOG_I("Program %p suspended", (void *)prog);
    return EOS_OK;
}

eos_result_t spm_resume_program(script_program_t *prog)
{
    if (!prog)
        return EOS_ERR_SCRIPT_NULL_PACKAGE;
    if (prog->state != SCRIPT_PROGRAM_STATE_SUSPENDED)
        return EOS_ERR_INVALID_STATE;

    script_engine_state_t core_state = script_engine_get_state();
    if (core_state != SCRIPT_ENGINE_STATE_IDLE)
    {
        EOS_LOG_E("spm_resume_program: Core state=%d not IDLE", core_state);
        return EOS_ERR_INVALID_STATE;
    }

    if (prog->sni_ctx)
        sni_context_set_paused(prog->sni_ctx, false);
    prog->state = SCRIPT_PROGRAM_STATE_ACTIVE;
    script_engine_set_current_program(prog);
    EOS_LOG_I("Program %p resumed", (void *)prog);
    return EOS_OK;
}

eos_result_t spm_terminate_program(script_program_t *prog)
{
    if (!prog)
        return EOS_ERR_SCRIPT_NULL_PACKAGE;
    if (prog->state == SCRIPT_PROGRAM_STATE_TERMINATED)
        return EOS_OK;

    EOS_LOG_I("Terminating program %p type=%d state=%d", (void *)prog, prog->type, prog->state);
    prog->state = SCRIPT_PROGRAM_STATE_STOPPING;
    // Pause the program context to ensure no callbacks are triggered
    if (prog->sni_ctx)
        sni_context_set_paused(prog->sni_ctx, true);

    // Clean up LVGL widgets created by this program's JS session
    if (prog->cleanup_view)
    {
        prog->cleanup_view(prog->cleanup_user_data);
        prog->cleanup_view = NULL;
    }

    // -- Teardown Phase 0: JS-Native decouple --
    // Clear JS object native_ptrs BEFORE stopping the JS engine.
    // This prevents JerryScript GC (triggered by _collect_script_garbage
    // inside script_engine_stop) from calling native free callbacks
    // that would free sni_managed_resource_node_t nodes while they are
    // still linked in the context's resource lists.
    if (prog->sni_ctx)
    {
        prog->sni_ctx->teardown_phase = SNI_TEARDOWN_PHASE_JS_DECOUPLE;
        sni_context_clear_native_ptrs_all(prog->sni_ctx);
    }

    // -- Teardown Phase 1: Release JS callback refs --
    // Release all JS callback references BEFORE stopping the engine.
    // Timer/animation callbacks hold JS function references that keep
    // bytecode alive. Freeing them here (with multiple GC passes) ensures
    // bytecode refcounts reach zero before script_engine_stop releases modules.
    if (prog->sni_ctx)
    {
        prog->sni_ctx->teardown_phase = SNI_TEARDOWN_PHASE_JS_REFS;
        sni_context_sweep_js_refs(prog->sni_ctx);
    }

    // -- Teardown Phase 2: Clean up LVGL event callbacks --
    // Must run while the JS heap is still valid (so jerry_value_free works)
    // and while the Activity's view hierarchy is still intact (so
    // lv_obj_remove_event_dsc does not access freed LVGL objects).
    // If we wait until _program_destroy runs after script_engine_stop,
    // both the JS heap AND potentially some LVGL objects have been freed,
    // causing EXC_BAD_ACCESS inside lv_array_size → lv_event_remove_dsc.
    if (prog->sni_ctx)
    {
        prog->sni_ctx->teardown_phase = SNI_TEARDOWN_PHASE_LVGL_EVENTS;
        sni_cb_context_cleanup_events(prog->sni_ctx);
    }

    // -- Teardown Phase 3: Stop the JS engine --
    // Let Core finish its stop/cleanup while the program object is still valid.
    script_engine_stop();
    if (prog->sni_ctx)
        prog->sni_ctx->teardown_phase = SNI_TEARDOWN_PHASE_ENGINE_STOPPED;
    // Remove the program from the list first so it can no longer be discovered
    _program_list_remove(prog);

    // Drop the Core's current-program pointer before freeing the program itself.
    script_engine_set_current_program(NULL);

    // Destroy the program resources after Core cleanup has finished.
    prog->state = SCRIPT_PROGRAM_STATE_TERMINATED;
    bool had_error = prog->has_error;
    _program_destroy(prog);

    /* If this program was healthy, any persistent error copy is stale */
    if (!had_error)
    {
        s_has_last_error = false;
        memset(&s_last_error, 0, sizeof(spm_error_t));
    }

    EOS_LOG_I("Program terminated");
    return EOS_OK;
}

void spm_terminate_programs_by_type(script_pkg_type_t type)
{
    script_program_t *prog = s_program_list;
    while (prog)
    {
        script_program_t *next = prog->next;
        if (type == SCRIPT_TYPE_UNKNOWN || prog->type == type)
            spm_terminate_program(prog);
        prog = next;
    }
}

/* JS Callback Gate -------------------------------------------*/

jerry_value_t spm_call(script_program_t *prog,
                       jerry_value_t func,
                       jerry_value_t this_val,
                       const jerry_value_t args_p[],
                       jerry_length_t args_count)
{
    if (!prog || prog->state != SCRIPT_PROGRAM_STATE_ACTIVE)
    {
        EOS_LOG_W("spm_call: program not ACTIVE (state=%d), rejecting", prog ? prog->state : -1);
        return jerry_undefined();
    }
    return script_engine_call_raw(func, this_val, args_p, args_count);
}

/* Query APIs -------------------------------------------------*/

script_program_t *spm_get_active_program(void)
{
    script_program_t *prog = s_program_list;
    while (prog)
    {
        if (prog->state == SCRIPT_PROGRAM_STATE_ACTIVE)
            return prog;
        prog = prog->next;
    }
    return NULL;
}

script_program_t *spm_get_program_by_type(script_pkg_type_t type)
{
    script_program_t *prog = s_program_list;
    while (prog)
    {
        if (prog->type == type && prog->state != SCRIPT_PROGRAM_STATE_TERMINATED)
            return prog;
        prog = prog->next;
    }
    return NULL;
}

script_program_t *spm_get_program_by_id(const char *id)
{
    if (!id)
        return NULL;
    script_program_t *prog = s_program_list;
    while (prog)
    {
        if (prog->script.id && strcmp(prog->script.id, id) == 0 && prog->state == SCRIPT_PROGRAM_STATE_ACTIVE)
        {
            return prog;
        }
        prog = prog->next;
    }
    return NULL;
}

const char *spm_get_program_error_info(script_program_t *prog)
{
    if (!prog || !prog->has_error)
        return "";
    return prog->error.error_info;
}

eos_script_error_type_t spm_get_program_error_type(script_program_t *prog)
{
    if (!prog || !prog->has_error)
        return EOS_SCRIPT_FAULT_ERROR_UNKNOWN;
    return prog->error.error_type;
}

const script_error_location_t *spm_get_program_error_location(script_program_t *prog)
{
    if (!prog || !prog->has_error)
        return NULL;
    return &prog->error.error_location;
}

/* Simplified WatchFace APIs ----------------------------------*/

static script_program_t *s_wf_program = NULL;

eos_result_t spm_watchface_start(const script_pkg_t *pkg, void *view)
{
    if (!pkg || !pkg->script_str)
        return EOS_ERR_SCRIPT_NULL_PACKAGE;
    if (s_wf_program)
    {
        spm_terminate_program(s_wf_program);
        s_wf_program = NULL;
    }
    script_engine_stop();
    s_wf_program = spm_start_program(pkg);
    if (s_wf_program && view)
    {
        s_wf_program->cleanup_view = _lvgl_view_clean;
        s_wf_program->cleanup_user_data = view;
    }
    return s_wf_program ? EOS_OK : EOS_FAILED;
}

eos_result_t spm_watchface_pause(void)
{
    if (!s_wf_program)
        return EOS_ERR_INVALID_STATE;
    script_engine_state_t state = script_engine_get_state();
    if (state == SCRIPT_ENGINE_STATE_RUNNING)
        script_engine_request_stop();
    eos_result_t ret = spm_suspend_program(s_wf_program);
    return ret;
}

eos_result_t spm_watchface_resume(void)
{
    if (!s_wf_program)
        return EOS_ERR_INVALID_STATE;
    eos_result_t ret = spm_resume_program(s_wf_program);
    return ret;
}

eos_result_t spm_watchface_destroy(void)
{
    if (s_wf_program)
    {
        spm_terminate_program(s_wf_program);
        s_wf_program = NULL;
    }
    script_engine_stop();
    return EOS_OK;
}

void spm_watchface_set_view_cleanup(void *view)
{
    if (s_wf_program && view)
    {
        s_wf_program->cleanup_view = _lvgl_view_clean;
        s_wf_program->cleanup_user_data = view;
    }
}

bool spm_watchface_has_context(void)
{
    return s_wf_program != NULL && s_wf_program->state != SCRIPT_PROGRAM_STATE_TERMINATED;
}

eos_result_t spm_app_run(const script_pkg_t *pkg)
{
    if (!pkg || !pkg->script_str)
        return EOS_ERR_SCRIPT_NULL_PACKAGE;
    return spm_start_program(pkg) ? EOS_OK : EOS_FAILED;
}

eos_result_t spm_app_stop(void)
{
    script_program_t *prog = spm_get_program_by_type(SCRIPT_TYPE_APPLICATION);
    if (!prog)
        return EOS_OK;
    eos_result_t ret = spm_terminate_program(prog);
    return ret;
}

eos_result_t spm_app_suspend(void)
{
    script_program_t *prog = spm_get_active_program();
    if (!prog || prog->type != SCRIPT_TYPE_APPLICATION)
        return EOS_ERR_INVALID_STATE;
    return spm_suspend_program(prog);
}

eos_result_t spm_app_resume(const char *app_id)
{
    if (!app_id)
        return EOS_ERR_SCRIPT_NULL_PACKAGE;
    script_program_t *prog = spm_get_program_by_id_any_state(app_id);
    if (!prog || prog->type != SCRIPT_TYPE_APPLICATION)
        return EOS_ERR_INVALID_STATE;
    if (prog->state != SCRIPT_PROGRAM_STATE_SUSPENDED)
        return EOS_ERR_INVALID_STATE;
    return spm_resume_program(prog);
}

eos_result_t spm_app_stop_by_id(const char *app_id)
{
    if (!app_id)
        return EOS_ERR_SCRIPT_NULL_PACKAGE;
    script_program_t *prog = spm_get_program_by_id_any_state(app_id);
    if (!prog || prog->type != SCRIPT_TYPE_APPLICATION)
        return EOS_OK; /* Already gone or never existed */
    return spm_terminate_program(prog);
}

script_program_t *spm_get_program_by_id_any_state(const char *id)
{
    if (!id)
        return NULL;
    script_program_t *prog = s_program_list;
    while (prog)
    {
        if (prog->script.id && strcmp(prog->script.id, id) == 0 && prog->state != SCRIPT_PROGRAM_STATE_TERMINATED)
        {
            return prog;
        }
        prog = prog->next;
    }
    return NULL;
}

const spm_error_t *spm_get_last_error(void)
{
    script_program_t *prog = s_program_list;
    while (prog)
    {
        if (prog->has_error)
        {
            return &prog->error;
        }
        prog = prog->next;
    }
    /* Fallback: return the persistent copy saved before program destruction. */
    if (s_has_last_error)
    {
        return &s_last_error;
    }
    return NULL;
}

/* Crash Context ----------------------------------------------*/

void spm_save_crash_context(const char *id, script_pkg_type_t type, const char *error_info)
{
    memset(&s_crash_state, 0, sizeof(s_crash_state));
    if (id)
    {
        size_t len = strlen(id);
        if (len >= SPM_CRASH_ID_MAX)
            len = SPM_CRASH_ID_MAX - 1;
        memcpy(s_crash_state.script_id, id, len);
        s_crash_state.script_id[len] = '\0';
    }
    s_crash_state.script_type = type;
    if (error_info)
    {
        size_t len = strlen(error_info);
        if (len >= SPM_ERROR_INFO_MAX)
            len = SPM_ERROR_INFO_MAX - 1;
        memcpy(s_crash_state.error_info, error_info, len);
        s_crash_state.error_info[len] = '\0';
    }
    s_crash_state.has_crash = true;
    EOS_LOG_W("SPM: crash context saved — id=%s type=%d info=%s",
              s_crash_state.script_id,
              s_crash_state.script_type,
              s_crash_state.error_info);
}

const spm_crash_state_t *spm_get_crash_state(void)
{
    return s_crash_state.has_crash ? &s_crash_state : NULL;
}

void spm_clear_crash_state(void)
{
    EOS_LOG_I("SPM: crash context cleared");
    memset(&s_crash_state, 0, sizeof(s_crash_state));
}

/* Deferred crash notification callback — posts EOS_EVENT_SCRIPT_FATAL */
static void _spm_crash_event_cb(void *user_data)
{
    (void)user_data;
    EOS_LOG_W("SPM: dispatching deferred crash notification");
    eos_event_post(EOS_EVENT_SCRIPT_FATAL, NULL, NULL);
}

void spm_schedule_crash_notification(void)
{
    EOS_LOG_W("SPM: scheduling deferred crash notification");
    eos_dispatcher_call(_spm_crash_event_cb, NULL);
}

void spm_handle_engine_reset(void)
{
    EOS_LOG_W("SPM: emergency reset — destroying all programs");

    /* Pause LVGL timer dispatch to prevent any pending callbacks from
     * firing during or after the engine reset. Callers re-enable timers
     * (lv_timer_enable(true)) after jerry_init() + sni_init() complete. */
    lv_timer_enable(false);

    script_program_t *prog = s_program_list;
    while (prog)
    {
        script_program_t *next = prog->next;

        if (prog->sni_ctx)
        {
            /* ORDER IS CRITICAL: neutralize SNI state BEFORE lv_obj_clean.
             * lv_obj_clean triggers LV_EVENT_DELETE → sni_cb_event_dispatch
             * → sni_cb_event_free_ctx → jerry_value_free, which will crash
             * on the corrupted JerryScript heap.  We unregister event
             * descriptors first so EVENT_DELETE is a no-op. */
            sni_cb_context_cleanup_events(prog->sni_ctx);

            /* Null out LVGL timer/animation callbacks and user_data so they
             * become harmless zombies.  Do NOT delete timers or free callback
             * contexts — the current callback may be on the stack, and
             * lv_timer_delete on the executing timer corrupts LVGL internals.
             * After jerry_init() wipes the heap, these zombies will be skipped
             * (NULL cb) or rejected by sni_cb_detect_recovery (gen mismatch). */
            sni_context_neutralize_timers(prog->sni_ctx);
            sni_context_neutralize_anims(prog->sni_ctx);
        }

        /* Clean LVGL view tree — safe now because event descriptors were
         * already unregistered above. */
        if (prog->cleanup_view)
        {
            prog->cleanup_view(prog->cleanup_user_data);
            prog->cleanup_view = NULL;
        }

        if (prog->sni_ctx)
        {
            /* Free the SNI context shell.  We intentionally LEAK the
             * managed-resource nodes and sni_*_callback_ctx_t blocks:
             *   - The current timer/anim ctx may still be on the C stack.
             *   - All jerry_* calls are unsafe after a fatal assertion.
             *   - jerry_init() will zero the JerryScript heap.
             * Engine crashes are rare; these small leaks are acceptable
             * compared to the risk of use-after-free or double-free. */
            sni_context_t *ctx = prog->sni_ctx;
            prog->sni_ctx = NULL;
            eos_free(ctx);
        }

        /* Do NOT call jerry_value_is_object / jerry_value_free here.
         * After a fatal assertion the JerryScript heap may be internally
         * inconsistent, and even read-only queries can crash (EXC_BAD_ACCESS).
         * jerry_init() will memset the entire context + heap to zero,
         * so all old handles are implicitly discarded. */

        _script_free(&prog->script);
        eos_free(prog);
        prog = next;
    }

    s_program_list = NULL;
    s_wf_program = NULL;
    s_has_last_error = false;
    memset(&s_last_error, 0, sizeof(spm_error_t));

    /* Notify recents registry that all programs were destroyed */
#if EOS_RECENT_APPS_ENABLE
    eos_recent_apps_on_engine_reset();
#endif

    EOS_LOG_I("SPM: all programs destroyed, engine reset ready");
}
