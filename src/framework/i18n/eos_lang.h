/**
 * @file eos_lang.h
 * @brief Multi-language system
 */

#ifndef EOS_LANG_H
#define EOS_LANG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/**
 * @brief Language type
 */
typedef enum
{
    LANG_EN = 0,
    LANG_ZH,
    LANG_MAX_NUMBER
} language_id_t;

/**
 * @brief String index
 * @note New string IDs can be added here
 */
typedef enum
{
    STR_ID_LANGUAGE,
    STR_ID_ERROR,
    STR_ID_OK,
    STR_ID_DONE,
    STR_ID_CANCEL,
    STR_ID_OFF,
    STR_ID_NORMAL,
    STR_ID_INTENSE,
    STR_ID_MSG_LIST_CLEAR_ALL,
    STR_ID_MSG_LIST_NO_MSG,
    STR_ID_MSG_LIST_ITEM_MARK_AS_READ,
    STR_ID_BACK,
    STR_ID_TEST_LANG_STR,
    STR_ID_APP_RUN_ERR_TITLE,
    STR_ID_APP_RUN_ERR,
    STR_ID_APP_RUN_ERR_BACKTRACE,
    STR_ID_WATCHFACE_RUN_ERR_TITLE,
    STR_ID_WATCHFACE_RUN_ERR,
    STR_ID_WATCHFACE_SWITCH,
    STR_ID_SETTINGS,
    STR_ID_SETTINGS_BLUETOOTH,
    STR_ID_SETTINGS_BLUETOOTH_ENABLE,
    STR_ID_SETTINGS_DISPLAY,
    STR_ID_SETTINGS_DISPLAY_BRIGHTNESS,
    STR_ID_SETTINGS_DISPLAY_AOD,
    STR_ID_SETTINGS_DISPLAY_AOD_COMMENT,
    STR_ID_SETTINGS_WAKE,
    STR_ID_SETTINGS_WAKE_DURATION,
    STR_ID_SETTINGS_WAKE_FOR_N_SECONDS,
    STR_ID_SETTINGS_WAKE_ON_TAP,
    STR_ID_SETTINGS_WAKE_ON_TAP_COMMENT,
    STR_ID_SETTINGS_WAKE_ON_RAISE,
    STR_ID_SETTINGS_HAPTICS_STRENGTH,
    STR_ID_SETTINGS_HAPTICS,
    STR_ID_SETTINGS_NOTIFICATION,
    STR_ID_SETTINGS_APPS,
    STR_ID_SETTINGS_APPS_DETAILS,
    STR_ID_SETTINGS_APPS_APPID,
    STR_ID_SETTINGS_APPS_AUTHOR,
    STR_ID_SETTINGS_APPS_VERSION,
    STR_ID_SETTINGS_APPS_DESCRIPTON,
    STR_ID_SETTINGS_APPS_UNINSTALL,
    STR_ID_SETTINGS_APPS_UNINSTALL_MSG,
    STR_ID_SETTINGS_APPS_CLEAR_DATA,
    STR_ID_SETTINGS_APPS_SDK,
    STR_ID_SETTINGS_GENERAL,
    STR_ID_SETTINGS_GENERAL_LANGUAGE,
    STR_ID_SETTINGS_GENERAL_DEVICE_INFO,
    STR_ID_SETTINGS_GENERAL_DEVICE_NAME,
    STR_ID_SETTINGS_GENERAL_EOS_VER,
    STR_ID_SETTINGS_GENERAL_MARKETING_NAME,
    STR_ID_SETTINGS_GENERAL_MODEL_NUMBER,
    STR_ID_SETTINGS_GENERAL_LEGAL_INFO,
    STR_ID_SETTINGS_GENERAL_LEGAL_INFO_CONTENT,
    STR_ID_SETTINGS_GENERAL_OPEN_SOURCE,
    STR_ID_SETTINGS_GENERAL_OPEN_SOURCE_CONTENT,
    STR_ID_SETTINGS_SOUNDS_AND_HAPTICS,
    STR_ID_SETTINGS_SOUNDS_AND_ALERTS,
    STR_ID_SETTINGS_SOUNDS_AND_HAPTICS_SILENT_MODE,
    STR_ID_SETTINGS_SOUNDS_AND_HAPTICS_VOLUME,
    STR_ID_SETTINGS_APPS_CLEAR_DATA_SUCCESS,
    STR_ID_APP_FLASH_LIGHT_DISMISS,
    STR_ID_TOAST_SHOW_MUTE,
    STR_ID_TOAST_SHOW_UNMUTE,
    STR_ID_SECOND,
    STR_ID_SYS_INIT_FAILED,
    STR_ID_SYS_INIT_FAILED_CONTENT,
    STR_ID_SENSOR_START,
    STR_ID_SENSOR_ACCE, /**< Accelerometer sensor     */
    STR_ID_SENSOR_GYRO, /**< Gyroscope sensor       */
    STR_ID_SENSOR_MAG, /**< Magnetometer sensor         */
    STR_ID_SENSOR_TEMP, /**< Temperature sensor       */
    STR_ID_SENSOR_HUMI, /**< Relative humidity sensor   */
    STR_ID_SENSOR_BARO, /**< Barometric sensor       */
    STR_ID_SENSOR_LIGHT, /**< Ambient light sensor     */
    STR_ID_SENSOR_PROXIMITY, /**< Proximity sensor       */
    STR_ID_SENSOR_HR, /**< Heart rate sensor       */
    STR_ID_SENSOR_TVOC, /**< TVOC sensor       */
    STR_ID_SENSOR_NOISE, /**< Noise sensor       */
    STR_ID_SENSOR_STEP, /**< Step counter sensor       */
    STR_ID_SENSOR_FORCE, /**< Force sensor         */
    STR_ID_SENSOR_BAT, /**< Battery level sensor    */
    STR_ID_SETTINGS_PASSWORD,
    STR_ID_SETTINGS_PASSWORD_ENABLE,
    STR_ID_SETTINGS_PASSWORD_DISABLE,
    STR_ID_SETTINGS_PASSWORD_CHANGE,
    STR_ID_SETTINGS_PASSWORD_SIMPLE,
    STR_ID_SETTINGS_PASSWORD_SIMPLE_COMMENT,
    STR_ID_LOCK_SCREEN_ENTER_PASSCODE,
    STR_ID_LOCK_SCREEN_WRONG_PASSCODE,
    STR_ID_SETTINGS_PASSWORD_ENTER_NEW,
    STR_ID_SETTINGS_PASSWORD_REENTER,
    STR_ID_SETTINGS_PASSWORD_MISMATCH,
    STR_ID_PERM_NAME_LOCATION,
    STR_ID_PERM_NAME_SENSOR,
    STR_ID_PERM_NAME_NOTIFICATION,
    STR_ID_PERM_NAME_STORAGE,
    STR_ID_PERM_NAME_BLUETOOTH,
    STR_ID_PERM_NAME_AUDIO,
    STR_ID_PERM_NAME_HEALTH,
    STR_ID_PERM_NAME_CONTACTS,
    STR_ID_PERM_NAME_CALENDAR,
    STR_ID_PERM_DESC_LOCATION,
    STR_ID_PERM_DESC_SENSOR,
    STR_ID_PERM_DESC_NOTIFICATION,
    STR_ID_PERM_DESC_STORAGE,
    STR_ID_PERM_DESC_BLUETOOTH,
    STR_ID_PERM_DESC_AUDIO,
    STR_ID_PERM_DESC_HEALTH,
    STR_ID_PERM_DESC_CONTACTS,
    STR_ID_PERM_DESC_CALENDAR,
    STR_ID_PERM_REQUEST_TITLE,
    STR_ID_PERM_ALLOW_ONCE,
    STR_ID_PERM_ALLOW_FOREGROUND,
    STR_ID_PERM_DENY,
    STR_ID_PERM_STATE_DENIED,
    STR_ID_PERM_STATE_ALLOW_ONCE,
    STR_ID_PERM_STATE_ALLOW_FOREGROUND,
    STR_ID_PERM_STATE_ALLOW_ALWAYS,
    STR_ID_PERM_NO_PERMISSIONS,
    STR_ID_PERM_MANAGEMENT,
    STR_ID_PERM_TITLE,
    /* New string IDs can be added here */
    STR_ID_MAX_NUMBER /**< Maximum string ID */
} lang_string_id_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize language system
 */
void eos_lang_init(void);
/**
 * @brief Set current language
 * @param lang Target language type `language_id_t`
 * @warning Language system must be initialized first
 */
void eos_lang_set_current_id(language_id_t lang);
/**
 * @brief Get current language type
 * @return language_id_t Language type
 */
language_id_t eos_lang_get_current_id(void);
/**
 * @brief Parse language ID from language name
 * @param language_name Language name (e.g., "English")
 * @return language_id_t Language type
 */
language_id_t eos_lang_parse_name(const char *language_name);
/**
 * @brief Get language name corresponding to specified language ID
 * @param lang Language type
 * @return const char* Language name
 */
const char *eos_lang_get_name(language_id_t lang);
/**
 * @brief Get current language name
 * @return const char* Language name
 */
const char *eos_lang_get_current_name(void);
/**
 * @brief Get text in current language
 * @param id String ID
 * @return const char* String
 */
const char *eos_lang_get_text(lang_string_id_t id);
/**
 * @brief Compatible old interface: Get language type from language string
 */
language_id_t eos_lang_get_current_id_with_str(const char *language_str);
/**
 * @brief Set label string by string ID
 * @param label Label object
 * @param str_id String ID
 */
void eos_label_set_text_id(lv_obj_t *label, lang_string_id_t str_id);
/**
 * @brief Support formatted strings
 * @param label
 * @param fmt
 * @return eos_label_lang_fmt_t*
 */
void eos_label_set_text_fmt(lv_obj_t *label, const char *fmt, ...);
#ifdef __cplusplus
}
#endif

#endif /* EOS_LANG_H */
