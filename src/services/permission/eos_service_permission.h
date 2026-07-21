/**
 * @file eos_service_permission.h
 * @brief Permission service - Android-like runtime permission model
 */
#ifndef EOS_SERVICE_PERMISSION_H
#define EOS_SERVICE_PERMISSION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_lang.h"

/* Public typedefs --------------------------------------------*/

/**
 * @brief Permission categories
 */
typedef enum
{
    EOS_PERM_CATEGORY_LOCATION = 0, /**< Location information */
    EOS_PERM_CATEGORY_SENSOR, /**< Sensor */
    EOS_PERM_CATEGORY_NOTIFICATION, /**< Notification */
    EOS_PERM_CATEGORY_STORAGE, /**< Storage */
    EOS_PERM_CATEGORY_BLUETOOTH, /**< Bluetooth */
    EOS_PERM_CATEGORY_AUDIO, /**< Audio/Mic */
    EOS_PERM_CATEGORY_HEALTH, /**< Health */
    EOS_PERM_CATEGORY_CONTACTS, /**< Contacts */
    EOS_PERM_CATEGORY_CALENDAR, /**< Calendar */
    EOS_PERM_CATEGORY_COUNT /**< Sentinel - must be last */
} eos_perm_category_t;

/**
 * @brief Permission grant states (Android-like)
 */
typedef enum
{
    EOS_PERM_STATE_DENIED = 0, /**< Not allowed */
    EOS_PERM_STATE_ALLOW_ONCE, /**< Allow once */
    EOS_PERM_STATE_ALLOW_FOREGROUND, /**< Allow while using the app */
    EOS_PERM_STATE_ALLOW_ALWAYS, /**< Always allow */
} eos_perm_state_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize the permission service
 * @note Must be called during system init after config service is ready
 */
void eos_service_permission_init(void);

/**
 * @brief Get current grant state for an app's permission
 * @param app_id Application ID (e.g., "cn.sab1e.calculator")
 * @param cat Permission category
 * @return eos_perm_state_t Current grant state (defaults to DENIED if never set)
 */
eos_perm_state_t eos_permission_get(const char *app_id, eos_perm_category_t cat);

/**
 * @brief Set grant state for an app's permission
 * @param app_id Application ID
 * @param cat Permission category
 * @param state New grant state
 * @return true on success, false on failure
 */
bool eos_permission_set(const char *app_id, eos_perm_category_t cat, eos_perm_state_t state);

/**
 * @brief Revoke all permissions for an app (used on uninstall)
 * @param app_id Application ID
 */
void eos_permission_revoke_all(const char *app_id);

/**
 * @brief Get the human-readable display name for a permission category
 * @param cat Permission category
 * @return const char* Localized name string
 */
const char *eos_permission_category_name(eos_perm_category_t cat);

/**
 * @brief Get the description text for a permission category
 *        (used as body text in the permission request panel)
 * @param cat Permission category
 * @return const char* Localized description string
 */
const char *eos_permission_category_desc(eos_perm_category_t cat);

/**
 * @brief Convert a permission name string (from manifest) to category enum
 * @param name Permission name string (e.g., "location", "sensor")
 * @return eos_perm_category_t Category enum, or EOS_PERM_CATEGORY_COUNT if unknown
 */
eos_perm_category_t eos_permission_name_to_category(const char *name);

/**
 * @brief Get the string key name for a permission category
 * @param cat Permission category
 * @return const char* Key name (e.g., "location") or NULL if invalid
 */
const char *eos_permission_category_key(eos_perm_category_t cat);

/**
 * @brief Get the i18n string ID for a permission category name
 * @param cat Permission category
 * @return lang_string_id_t String ID
 */
lang_string_id_t eos_permission_category_name_id(eos_perm_category_t cat);

/**
 * @brief Get the i18n string ID for a permission category description
 * @param cat Permission category
 * @return lang_string_id_t String ID
 */
lang_string_id_t eos_permission_category_desc_id(eos_perm_category_t cat);

/**
 * @brief Get the i18n string ID for a grant state label
 * @param state Grant state
 * @return lang_string_id_t String ID
 */
lang_string_id_t eos_permission_state_label_id(eos_perm_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SERVICE_PERMISSION_H */
