/**
 * @file eos_app_list.h
 * @brief App list page
 */

#ifndef EOS_APP_LIST_H
#define EOS_APP_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_core.h"
#include "eos_activity.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef void (*eos_sys_app_entry_t)(void);

enum
{
    EOS_SYS_APP_SETTINGS = 0,
    EOS_SYS_APP_FLASH_LIGHT,
/* New system apps can be added here */
#ifdef EOS_ENABLE_TEST_APP
    EOS_SYS_APP_TEST,
#endif
    EOS_SYS_APP_LAST
};

extern const char *eos_sys_app_id_list[EOS_SYS_APP_LAST];

/* Public function prototypes ---------------------------------*/

/**
 * @brief Immediately launch the target app by id from any page
 * @param app_id Target app id
 * @return eos_result_t Launch result
 */
eos_result_t eos_app_launch_immediately(const char *app_id);
/**
 * @brief Enter app list
 * @return eos_activity_t* App list activity object
 */
void eos_app_list_enter(void);

/**
 * @brief Get the app_id from an APP-type activity's launch context
 * @param activity The activity (must be EOS_ACTIVITY_TYPE_APP)
 * @return const char* App ID string, or NULL if not a script app activity
 */
const char *eos_app_list_get_app_id(eos_activity_t *activity);

/**
 * @brief Restart an app in-place on its existing activity (no navigation, no animation)
 * @param app_id The app ID (for logging/validation)
 * @param activity The current app activity to restart
 * @return eos_result_t EOS_OK on success, EOS_FAILED on error
 * @note Cleans all child widgets from the activity view and re-runs the app script.
 *       The activity stack is preserved — this does NOT navigate.
 */
eos_result_t eos_app_restart_in_place(const char *app_id, eos_activity_t *activity);

#if EOS_COMPILE_MODE == DEBUG
/**
 * @brief Set an artificial delay to simulate slow app loading (debug only).
 *
 * The delay is applied in two phases to mimic real-world slow Flash:
 *   - Phase 1: delay before reading main.js (simulates slow SPI NOR read)
 *   - Phase 2: delay before spm_app_run (simulates slow JS parse/eval)
 *
 * Set to 0 to disable. Default is 0 (no artificial delay).
 *
 * @param io_delay_ms   Delay before reading main.js, in ms
 * @param eval_delay_ms Delay before JS evaluation, in ms
 */
void eos_app_list_set_debug_loading_delay(uint32_t io_delay_ms, uint32_t eval_delay_ms);

/**
 * @brief Get the current debug loading delay settings
 * @param io_delay_ms   [out] Current I/O delay in ms, may be NULL
 * @param eval_delay_ms [out] Current eval delay in ms, may be NULL
 */
void eos_app_list_get_debug_loading_delay(uint32_t *io_delay_ms, uint32_t *eval_delay_ms);
#endif /* EOS_COMPILE_MODE == DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* EOS_APP_LIST_H */
