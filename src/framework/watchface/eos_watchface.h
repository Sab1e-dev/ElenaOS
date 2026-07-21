/**
 * @file eos_watchface.h
 * @brief Watchface
 */

#ifndef EOS_WATCHFACE_H
#define EOS_WATCHFACE_H

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
#include "eos_activity.h"
#include "script_engine_core.h"
/* Public macros ----------------------------------------------*/
#define EOS_WATCHFACE_DATA_DIR EOS_WATCHFACE_DIR "wf_data/"
#define EOS_WATCHFACE_BUILTIN_FALLBACK_ID "builtin.fallback"
#define EOS_WATCHFACE_MANIFEST_FILE_NAME "manifest.json"
#define EOS_WATCHFACE_SNAPSHOT_FILE_NAME "snapshot.bin"
#define EOS_WATCHFACE_SCRIPT_ENTRY_FILE_NAME "main.js"
#define EOS_WATCHFACE_ID_LEN_MAX 256 /*< Watchface ID maximum length */
/* Public typedefs --------------------------------------------*/

/**
 * @brief Watchface type enumeration
 */
typedef enum
{
    EOS_WATCHFACE_TYPE_BUILTIN, /**< Built-in fallback watchface */
    EOS_WATCHFACE_TYPE_JS, /**< JavaScript script watchface */
} eos_watchface_type_t;

/**
 * @brief Watchface instance structure
 *
 * Each watchface instance owns its own Activity and manages its lifecycle.
 * Switching watchfaces = destroy old instance + create new instance.
 */
typedef struct eos_watchface_instance
{
    eos_watchface_type_t type; /**< Watchface type */
    char id[EOS_WATCHFACE_ID_LEN_MAX]; /**< Watchface ID */

    eos_activity_t *activity; /**< Owned Activity (created and managed by instance) */
    const eos_activity_lifecycle_t *lifecycle; /**< Activity lifecycle callbacks */

    union
    {
        struct
        {
            lv_timer_t *time_update_timer; /**< Time update timer for builtin watchface */
        } builtin;

        struct
        {
            script_pkg_t pkg; /**< Script package information for JS watchface */
        } js;
    } data;
} eos_watchface_instance_t;
/* Public function prototypes ---------------------------------*/

/**
 * @brief Get the number of currently installed watchfaces, i.e., list size
 * @return size_t List size
 */
size_t eos_watchface_list_size(void);
/**
 * @brief Get watchface id by index
 * @param index Index value (0-based)
 * @return const char* id string
 */
const char *eos_watchface_list_get_id(size_t index);
/**
 * @brief Check if watchface with specified id exists in the list
 * @param watchface_id id string
 * @return true
 * @return false
 */
bool eos_watchface_list_contains(const char *watchface_id);
/**
 * @brief Install watchface
 * @param eapk_path eapk package path
 * @return eos_result_t Installation result
 */
eos_result_t eos_watchface_install(const char *eapk_path);
/**
 * @brief Uninstall watchface
 * @param watchface_id Watchface id
 * @return eos_result_t Uninstallation result
 */
eos_result_t eos_watchface_uninstall(const char *watchface_id);
/**
 * @brief Initialize watchface system
 * @return eos_result_t Initialization result
 */
eos_result_t eos_watchface_init(void);
/**
 * @brief Get watchface Activity created during initialization
 * @return eos_activity_t* Watchface Activity pointer, returns NULL on failure
 */
eos_activity_t *eos_watchface_get_activity(void);

/**
 * @brief Check if watchface configuration changed and reload if needed
 * @note This function should be called when returning to root activity (e.g., in on_resume)
 */
void eos_watchface_check_and_reload(void);

/**
 * @brief Switch to a specific watchface by ID
 * @param watchface_id Target watchface ID to switch to
 * @note This function will replace the root activity with the new watchface instance
 */
void eos_watchface_switch_to(const char *watchface_id);

/**
 * @brief Long press handler for watchface (enter watchface list)
 * @param e Event object
 */
void eos_watchface_long_pressed_handler(lv_event_t *e);
#ifdef __cplusplus
}
#endif

#endif /* EOS_WATCHFACE_H */
