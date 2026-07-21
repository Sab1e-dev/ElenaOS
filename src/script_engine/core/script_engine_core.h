/**
 * @file script_engine_core.h
 * @brief Script Engine Core — "how to run JS"
 *
 * Core is responsible ONLY for: JerryScript VM lifecycle, Realm
 * create/destroy/switch, JS bytecode parse/execute, module import,
 * timeout detection. It knows NOTHING about Activity, View, WatchFace,
 * Application, or program lifecycle. Those belong to SPM.
 */

#ifndef SCRIPT_ENGINE_CORE_H
#define SCRIPT_ENGINE_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "jerryscript.h"
#include "eos_error.h"

/* Public macros ----------------------------------------------*/

#ifndef SNI_CONTEXT_MAX_COUNT
#define SNI_CONTEXT_MAX_COUNT 2
#endif

/* Public typedefs --------------------------------------------*/

/**
 * @brief Engine execution-window state — NOT program lifecycle
 */
typedef enum
{
    SCRIPT_ENGINE_STATE_UNINITIALIZED,
    SCRIPT_ENGINE_STATE_IDLE,
    SCRIPT_ENGINE_STATE_RUNNING,
    SCRIPT_ENGINE_STATE_EXCEPTION,
} script_engine_state_t;

typedef enum
{
    SCRIPT_TYPE_UNKNOWN = 0,
    SCRIPT_TYPE_APPLICATION = 1,
    SCRIPT_TYPE_WATCHFACE = 2,
} script_pkg_type_t;

typedef struct
{
    const char *class_name;
    const char *method_name;
    jerry_external_handler_t handler;
} script_engine_func_entry_t;

typedef struct
{
    const char *id;
    const char *name;
    script_pkg_type_t type;
    const char *version;
    const char *author;
    const char *description;
    const char *script_str;
    const char *base_path;
    const char **permissions; /**< Array of permission name strings */
    uint8_t permission_count; /**< Number of permissions declared */
    uint16_t min_api_level; /**< Minimum required API level */
    uint16_t target_api_level; /**< Target API level the package was built for */
} script_pkg_t;

typedef struct
{
    char source_name[128];
    uint32_t line;
    uint32_t column;
} script_error_location_t;

typedef enum
{
    EOS_SCRIPT_FAULT_ERROR_UNKNOWN = 0,
    EOS_SCRIPT_FAULT_ERROR_EXCEPTION,
    EOS_SCRIPT_FAULT_UNRESPONSIVE,
    EOS_SCRIPT_FAULT_ERROR_PARSE,
    EOS_SCRIPT_FAULT_ERROR_MODULE_LINK,
    EOS_SCRIPT_FAULT_ENGINE_CRASH,
} eos_script_error_type_t;

/**
 * @brief SPM program handle — opaque to Core, owned by SPM
 */
struct script_program;
typedef struct script_program script_program_t;

/* Public function prototypes ---------------------------------*/

/** @name Core Lifecycle */
/**@{*/
eos_result_t script_engine_init(void);
eos_result_t script_engine_stop(void);
eos_result_t script_engine_request_stop(void);
eos_result_t script_engine_clean_up(void);
/**@}*/

/** @name Run — SPM sets current_program before calling this */
/**@{*/
/**
 * @brief Execute script for the given SPM program
 *
 * SPM MUST call script_engine_set_current_program() before this call.
 * Core copies the program's script_pkg_t internally and writes back
 * creado sni_ctx and realm into the program.
 */
eos_result_t script_engine_run(const script_pkg_t *script_package);

/**
 * @brief Set/clear the SPM program that Core is working for
 * @param prog Program pointer (SPM-owned, not freed by Core), NULL to clear
 */
void script_engine_set_current_program(script_program_t *prog);
script_program_t *script_engine_get_current_program(void);
/**@}*/

/** @name State & Error */
/**@{*/
script_engine_state_t script_engine_get_state(void);
const char *script_engine_get_error_info(void);
const script_error_location_t *script_engine_get_error_location(void);
const script_error_location_t *script_engine_get_error_backtrace(uint32_t *count);
uint32_t script_engine_get_backtrace_count(void);
char *script_engine_get_current_script_id(void);
char *script_engine_get_current_script_name(void);
script_pkg_type_t script_engine_get_current_script_type(void);
bool script_engine_has_permission(const char *perm_name);
uint32_t script_engine_get_timeout(void);
void script_engine_set_timeout(uint32_t timeout_ms);
/**@}*/

/** @name JS Interaction */
/**@{*/
jerry_value_t script_engine_throw_error(const char *message);
void script_engine_register_functions(jerry_value_t parent,
                                      const script_engine_func_entry_t *entry,
                                      const size_t funcs_count);

/**
 * @brief Raw jerry_call — for SPM's spm_call() only.
 * SPM must validate program state before calling.
 */
jerry_value_t script_engine_call_raw(jerry_value_t func,
                                     jerry_value_t this_val,
                                     const jerry_value_t args_p[],
                                     const jerry_length_t args_count);
/**@}*/

/** @name Property Helpers */
/**@{*/
static inline void script_engine_set_prop_number(jerry_value_t obj, const char *prop_name, double value)
{
    jerry_value_t prop = jerry_string_sz(prop_name);
    jerry_value_t val = jerry_number(value);
    jerry_value_free(jerry_object_set(obj, prop, val));
    jerry_value_free(val);
    jerry_value_free(prop);
}

static inline void script_engine_set_prop_bool(jerry_value_t obj, const char *prop_name, bool value)
{
    jerry_value_t prop = jerry_string_sz(prop_name);
    jerry_value_t val = jerry_boolean(value);
    jerry_value_free(jerry_object_set(obj, prop, val));
    jerry_value_free(val);
    jerry_value_free(prop);
}

static inline void script_engine_set_prop_string(jerry_value_t obj, const char *prop_name, const char *value)
{
    jerry_value_t prop = jerry_string_sz(prop_name);
    jerry_value_t val = jerry_string_sz(value);
    jerry_value_free(jerry_object_set(obj, prop, val));
    jerry_value_free(val);
    jerry_value_free(prop);
}
/**@}*/

/** @name Fatal Recovery */
/**@{*/
bool script_engine_is_fatal_scope_active(void);
bool script_engine_is_fatal_recovering(void);
void script_engine_set_fatal_recovering(bool recovering);
void JERRY_ATTR_NORETURN script_engine_fatal_longjmp(int code);
/**@}*/

/** @name Engine Generation */
/**@{*/
/**
 * @brief Get the current engine generation counter.
 * Incremented after each jerry_init() during recovery.
 * SNI control blocks store the generation at creation time to detect stale handles.
 */
uint32_t script_engine_get_gen(void);
/**@}*/

/** @name Reload */
/**@{*/
eos_result_t script_engine_reload_current_script(void);
eos_result_t script_engine_reload_current_app(void);
/**@}*/

/** @name Manifest */
/**@{*/
eos_result_t script_engine_get_manifest(const char *manifest_path, script_pkg_t *pkg);
/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* SCRIPT_ENGINE_CORE_H */
