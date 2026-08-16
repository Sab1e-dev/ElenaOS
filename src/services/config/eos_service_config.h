/**
 * @file eos_service_config.h
 * @brief System configuration service
 */

#ifndef EOS_SERVICE_CONFIG_H
#define EOS_SERVICE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_core.h"
#include "eos_config.h"
#include "eos_storage_paths.h"
#include "cJSON.h"
#include "eos_lang.h"
/* Public macros ----------------------------------------------*/
/* Configuration Keys */
/* Keys for system configuration information ------------------*/
#define EOS_CONFIG_KEY_DEVICE_NAME_STR "device_name"
#define EOS_CONFIG_KEY_LANGUAGE_STR "language"
#define EOS_CONFIG_KEY_WATCHFACE_ID_STR "wf_id"
#define EOS_CONFIG_KEY_BLUETOOTH_BOOL "bluetooth"
#define EOS_CONFIG_KEY_DISPLAY_BRIGHTNESS_NUMBER "display_brightness"
#define EOS_CONFIG_KEY_SPEAKER_VOLUME_NUMBER "speaker_volume"
#define EOS_CONFIG_KEY_MUTE_BOOL "mute"
#define EOS_CONFIG_KEY_SLEEP_TIMEOUT_SEC_NUMBER "sleep_timeout_sec"
#define EOS_CONFIG_KEY_AOD_MODE_BOOL "aod_mode"
#define EOS_CONFIG_KEY_WAKE_ON_RAISE_BOOL "wake_on_raise"
#define EOS_CONFIG_KEY_VIBRATOR_STRENGTH_NUMBER "vibrator_strength"
#define EOS_CONFIG_KEY_APP_ORDER_ARRAY "app_order"
#define EOS_CONFIG_KEY_PASSWORD_HASH_STR "password_hash"
#define EOS_CONFIG_KEY_PASSWORD_ENABLED_BOOL "password_enabled"
#define EOS_CONFIG_KEY_PASSWORD_SIMPLE_BOOL "password_simple"
/* Developer options ------------------------------------------*/
#define EOS_CONFIG_KEY_DEV_FPS_BOOL "dev_fps"
#define EOS_CONFIG_KEY_DEV_OBJS_BOOL "dev_objs"
#define EOS_CONFIG_KEY_DEV_TOUCH_BOOL "dev_touch"
/* Default values ---------------------------------------------*/
#define EOS_CONFIG_DEFAULT_DEVICE_NAME "Elenix Watch"
/* Default language string is determined by EOS_CONFIG_DEFAULT_LANGUAGE in eos_config.h */
#define EOS_CONFIG_DEFAULT_LANG_STR (EOS_CONFIG_DEFAULT_LANGUAGE == 1 ? "简体中文" : "English")
#define EOS_CONFIG_DEFAULT_WATCHFACE_ID_STR "cn.sab1e.clock"
/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize on first system run
 */
void eos_service_config_init(void);
/**
 * @brief Set boolean type configuration item
 * @param key Configuration item key
 * @param value Boolean value
 * @return Operation result
 */
eos_result_t eos_config_set_bool(const char *key, bool value);
/**
 * @brief Set string type configuration item
 * @param key Configuration item key
 * @param value String value
 * @return Operation result
 */
eos_result_t eos_config_set_string(const char *key, const char *value);
/**
 * @brief Set number type configuration item
 * @param key Configuration item key
 * @param value Number value
 * @return Operation result
 */
eos_result_t eos_config_set_number(const char *key, double value);
/**
 * @brief Get boolean type configuration item
 * @param key Configuration item key
 * @param default_value Default value (returned when configuration item does not exist or type mismatch)
 * @return Retrieved boolean value or default value
 */
bool eos_config_get_bool(const char *key, bool default_value);
/**
 * @brief Get string type configuration item
 * @param key Configuration item key
 * @param default_value Default value (returned when configuration item does not exist or type mismatch)
 * @return Retrieved string value or default value
 * @warning The returned string needs to be freed using `eos_malloc(str)` when no longer needed
 */
char *eos_config_get_string(const char *key, const char *default_value);
/**
 * @brief Get number type configuration item
 * @param key Configuration item key
 * @param default_value Default value (returned when configuration item does not exist or type mismatch)
 * @return Retrieved number value or default value
 */
double eos_config_get_number(const char *key, double default_value);
/**
 * @brief Add new configuration item to system configuration file
 * @param key Configuration item key to add (string)
 * @param value Configuration item value to add (string)
 * @return Returns result
 */
eos_result_t eos_config_add_item(const char *key, const char *value);

/**
 * @brief Get configuration value as JSON object
 * @param key Configuration item key
 * @return cJSON object or NULL if not found or not an object/array
 * @warning Caller must call cJSON_Delete() on the returned object
 */
cJSON *eos_config_get_json(const char *key);

/**
 * @brief Set configuration value from JSON object
 * @param key Configuration item key
 * @param json_value JSON object or array to set
 * @return Operation result
 */
eos_result_t eos_config_set_json(const char *key, cJSON *json_value);
#ifdef __cplusplus
}
#endif

#endif /* EOS_SERVICE_CONFIG_H */
