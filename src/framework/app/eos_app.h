/**
 * @file eos_app.h
 * @brief Application system
 */

#ifndef EOS_APP_H
#define EOS_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "eos_core.h"
#include "eos_service_config.h"
#include "eos_storage_paths.h"
#include "lvgl.h"
#include "script_engine_core.h"

/* Public macros ----------------------------------------------*/
#define EOS_APP_ICON_FILE_NAME "icon.bin"
#define EOS_APP_MANIFEST_FILE_NAME "manifest.json"
#define EOS_APP_SCRIPT_ENTRY_FILE_NAME "main.js"
/* Public typedefs --------------------------------------------*/

/**
 * @brief Script error handler configuration
 */
typedef struct
{
    const char *title_text; /**< Error title text (NULL for default) */
    lang_string_id_t title_id; /**< Error title string ID (0 for default) */
    const char *button_text; /**< Button text (NULL for default) */
    lang_string_id_t button_id; /**< Button string ID (0 for default) */
    lv_event_cb_t button_callback; /**< Button click callback (NULL for default back) */
} eos_script_error_handler_cfg_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Handle script execution error
 * @param error_type Type of script error
 * @param error_code Error code from script engine
 * @param app_id Application ID that caused the error
 * @param cfg Optional configuration for customizing error page
 */
void eos_app_handle_script_error(eos_script_error_type_t error_type,
                                 eos_result_t error_code,
                                 const char *app_id,
                                 const eos_script_error_handler_cfg_t *cfg);

/**
 * @brief Move app with target ID to specified position for app_list sorting
 * @param app_id Target ID
 * @param new_index New index value
 * @return eos_result_t
 */
eos_result_t eos_app_order_move(const char *app_id, size_t new_index);
/**
 * @brief Get the number of currently installed apps
 */
uint32_t eos_app_get_installed(void);
/**
 * @brief Get app id by index
 * @param index Index value (0-based)
 * @return const char* id string
 */
const char *eos_app_list_get_id(size_t index);
/**
 * @brief Check if app with specified id exists in the list
 * @param app_id id string
 * @return true
 * @return false
 */
bool eos_app_list_contains(const char *app_id);
/**
 * @brief Get existing ID from app list that matches input string (avoid duplicate memory allocation)
 * @param id Original ID to find (e.g., header.pkg_id)
 * @return Existing string pointer in the list (lifecycle managed by the list), returns NULL if not found
 */
const char *eos_app_list_get_existing_id(const char *id);
/**
 * @brief Install app
 * @param eapk_path eapk package path
 * @return eos_result_t Installation result
 */
eos_result_t eos_app_install(const char *eapk_path);
/**
 * @brief Uninstall app
 * @param app_id App id
 * @return eos_result_t Uninstallation result
 */
eos_result_t eos_app_uninstall(const char *app_id);
/**
 * @brief Automatically delete specified object when app is deleted
 * @param obj Target object
 * @param app_id Target app ID
 */
void eos_app_obj_auto_delete(lv_obj_t *obj, const char *app_id);
/**
 * @brief Initialize app system
 * @return eos_result_t Initialization result
 */
eos_result_t eos_app_init(void);
#ifdef __cplusplus
}
#endif

#endif /* EOS_APP_H */
