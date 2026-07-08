/**
 * @file spm.h
 * @brief Script Program Manager - program lifecycle and callback gate
 *
 * SPM is the sole owner of script program lifecycle. It manages the
 * program_list (doubly-linked list of script_program_t), validates all
 * JS callback entries via spm_call(), and delegates JS execution to
 * Script Engine Core (SEC).
 *
 * Architecture constraint:
 *   Upper layers (eos_watchface_js, eos_app) must ONLY use spm_watchface_*
 *   and spm_app_* convenience APIs. Direct Core access is forbidden.
 */

#ifndef SPM_H
#define SPM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "jerryscript.h"
#include "script_engine_core.h"
#include "sni_context.h"

/* Public macros ----------------------------------------------*/

#define SPM_ERROR_INFO_MAX 256
#define SPM_BACKTRACE_MAX_FRAMES 16
#define SPM_BACKTRACE_SOURCE_SIZE 128

/* Public typedefs --------------------------------------------*/

/**
 * @brief Script program state
 */
typedef enum
{
    SCRIPT_PROGRAM_STATE_TERMINATED, /* Fully terminated, all resources freed */
    SCRIPT_PROGRAM_STATE_STOPPING, /* Stopping, no callbacks allowed, cleaning up */
    SCRIPT_PROGRAM_STATE_ACTIVE, /* Active and healthy */
    SCRIPT_PROGRAM_STATE_SUSPENDED, /* Suspended, no code execution, resumable */
} script_program_state_t;

/**
 * @brief Script error record (copied from Core on failure)
 */
typedef struct
{
    char error_info[SPM_ERROR_INFO_MAX];
    eos_script_error_type_t error_type;
    script_error_location_t error_location;
    script_error_location_t backtrace[SPM_BACKTRACE_MAX_FRAMES];
    uint32_t backtrace_count;
} spm_error_t;

/**
 * @brief Script program - the central managed entity
 *
 * Each running or suspended script program is represented by one
 * script_program_t node in SPM's program_list doubly-linked list.
 */
typedef struct script_program
{
    struct script_program *next;
    struct script_program *prev;

    script_pkg_type_t type;
    script_program_state_t state;
    sni_context_t *sni_ctx;
    script_pkg_t script;
    jerry_value_t realm;

    bool has_error;
    spm_error_t error;

    void (*cleanup_view)(void *user_data);
    void *cleanup_user_data;
} script_program_t;

/* Public function prototypes --------------------------------*/

/** @name SPM Lifecycle */
/**@{*/
/**
 * @brief Initialize the Script Program Manager
 * @return EOS_OK on success
 * @note Must be called after script_engine_init()
 */
eos_result_t spm_init(void);

/**
 * @brief Emergency destroy all programs during engine fatal recovery
 *
 * Called from script_engine_run()'s setjmp/longjmp recovery block
 * BEFORE jerry_init() wipes the heap. Walks the entire program list,
 * frees all JS handles (realm, sni_ctx callbacks) on the still-valid
 * old heap, destroys all C-level resources, and clears s_wf_program.
 */
void spm_handle_engine_reset(void);
/**@}*/

/** @name Program Lifecycle (low-level) */
/**@{*/
/**
 * @brief Start a script program
 * @param pkg Script package (deep-copied internally)
 * @return Program handle, or NULL on failure
 *
 * Full flow:
 *   1. Allocate script_program_t, link into program_list
 *   2. Delegate to Core for parse+execute (Core: IDLE -> RUNNING -> IDLE)
 *   3. On success: state = ACTIVE
 *   4. On failure: state = TERMINATED, removed from list, error copied
 */
script_program_t *spm_start_program(const script_pkg_t *pkg);

/**
 * @brief Suspend a script program (WatchFace only)
 * @param prog Program handle
 * @return EOS_OK on success
 *
 * Preconditions: prog->type == SCRIPT_TYPE_WATCHFACE, prog->state == ACTIVE, Core == IDLE
 * Operations: save realm to prog, pause SNI callbacks, prog->state = SUSPENDED
 */
eos_result_t spm_suspend_program(script_program_t *prog);

/**
 * @brief Resume a script program (WatchFace only)
 * @param prog Program handle
 * @return EOS_OK on success
 *
 * Preconditions: prog->state == SUSPENDED, Core == IDLE
 * Operations: restore realm to Core, resume SNI callbacks, prog->state = ACTIVE
 */
eos_result_t spm_resume_program(script_program_t *prog);

/**
 * @brief Terminate a script program (async safe)
 * @param prog Program handle
 * @return EOS_OK on success
 *
 * Terminal entry point:
 *   - ACTIVE -> STOPPING (async wait for Core stop) -> TERMINATED
 *   - SUSPENDED -> STOPPING (direct cleanup) -> TERMINATED
 */
eos_result_t spm_terminate_program(script_program_t *prog);

/**
 * @brief Terminate all programs of a given type
 * @param type Program type, SCRIPT_TYPE_UNKNOWN means all
 */
void spm_terminate_programs_by_type(script_pkg_type_t type);
/**@}*/

/** @name JS Callback Gate (sole legal entry into JS) */
/**@{*/
/**
 * @brief Issue a JS call through SPM validation gate
 * @param prog Target program
 * @param func JS function to call
 * @param this_val 'this' binding
 * @param args_p Argument array
 * @param args_count Number of arguments
 * @return Call result, or jerry_undefined() if gate rejects
 *
 * Gate checks:
 *   1. prog != NULL && prog->state == ACTIVE
 *   2. Core has no active program or active program == prog
 *   3. If Core is IDLE: transition to RUNNING, reset timeout timer
 *   4. Delegate jerry_call() to Core
 *   5. After call: Core back to IDLE
 */
jerry_value_t spm_call(script_program_t *prog,
                       jerry_value_t func,
                       jerry_value_t this_val,
                       const jerry_value_t args_p[],
                       jerry_length_t args_count);
/**@}*/

/** @name Query APIs */
/**@{*/
/**
 * @brief Get the currently active program
 * @return Active program pointer, or NULL if none
 * @note Only returns ACTIVE-state programs
 */
script_program_t *spm_get_active_program(void);

/**
 * @brief Get the first program of a given type
 * @param type Program type
 * @return First matching program, or NULL
 */
script_program_t *spm_get_program_by_type(script_pkg_type_t type);

/**
 * @brief Find a program by its script package ID
 * @param id Script package ID to search for
 * @return Program pointer if found and ACTIVE, or NULL
 */
script_program_t *spm_get_program_by_id(const char *id);

/**
 * @brief Get error info from a program
 * @param prog Program handle
 * @return Error string (lifetime bound to prog)
 */
const char *spm_get_program_error_info(script_program_t *prog);

/**
 * @brief Get error type from a program
 * @param prog Program handle
 * @return Error type
 */
eos_script_error_type_t spm_get_program_error_type(script_program_t *prog);

/**
 * @brief Get error location from a program
 * @param prog Program handle
 * @return Error location pointer
 */
const script_error_location_t *spm_get_program_error_location(script_program_t *prog);

/**
 * @brief Get the most recent copied error snapshot
 * @return Latest error snapshot, or NULL if no failure has been recorded
 */
const spm_error_t *spm_get_last_error(void);
/**@}*/

/** @name Simplified WatchFace APIs */
/**@{*/
eos_result_t spm_watchface_start(const script_pkg_t *pkg, void *view);
void spm_watchface_set_view_cleanup(void *view);
eos_result_t spm_watchface_pause(void);
eos_result_t spm_watchface_resume(void);
eos_result_t spm_watchface_destroy(void);
bool spm_watchface_has_context(void);
/**@}*/

/** @name Simplified Application APIs */
/**@{*/
eos_result_t spm_app_run(const script_pkg_t *pkg);
eos_result_t spm_app_stop(void);
/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* SPM_H */
