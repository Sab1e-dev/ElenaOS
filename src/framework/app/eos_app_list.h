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
 * @brief Restart an app in-place on its existing activity (no navigation, no animation)
 * @param app_id The app ID (for logging/validation)
 * @param activity The current app activity to restart
 * @return eos_result_t EOS_OK on success, EOS_FAILED on error
 * @note Cleans all child widgets from the activity view and re-runs the app script.
 *       The activity stack is preserved — this does NOT navigate.
 */
eos_result_t eos_app_restart_in_place(const char *app_id, eos_activity_t *activity);
#ifdef __cplusplus
}
#endif

#endif /* EOS_APP_LIST_H */
