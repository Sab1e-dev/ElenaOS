/**
 * @file eos_service_config.c
 * @brief System configuration service
 */

#include "eos_service_config.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "eos_storage_paths.h"
#include "eos_image.h"
#include "eos_msg_list.h"
#include "eos_lang.h"
#define EOS_LOG_TAG "ConfigService"
#include "eos_log.h"
#include "eos_basic_widgets.h"
#include "eos_event.h"
#include "eos_service_display.h"
#include "eos_service_audio.h"
#include "eos_test.h"
#include "eos_version.h"
#include "eos_port.h"
#include "eos_swipe_panel.h"
#include "eos_app.h"
#include "eos_watchface.h"
#include "eos_theme.h"
#include "eos_pkg_mgr.h"
#include "eos_service_sensor.h"
#include "eos_service_storage.h"
#include "eos_mem.h"
#include "eos_service_haptic.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static eos_result_t _eos_config_save_and_broadcast(cJSON *root)
{
    if (!root)
        return EOS_ERR_JSON_ERROR;

    eos_result_t ret = eos_storage_json_save(EOS_CONFIG_FILE_PATH, root);
    cJSON_Delete(root);

    if (ret == EOS_OK) {
        eos_event_post(EOS_EVENT_SYSTEM_CONFIG_UPDATE, NULL, NULL);
    }

    return ret;
}

eos_result_t eos_config_set_bool(const char *key, bool value)
{
    EOS_CHECK_PTR_RETURN_VAL(key, EOS_ERR_VAR_NULL);

    EOS_LOG_I("Try set \"%s\" = \"%s\"", key, value ? "true" : "false");

    cJSON *root = eos_storage_json_load(EOS_CONFIG_FILE_PATH);
    if (!root) {
        root = cJSON_CreateObject();
        if (!root)
            return EOS_ERR_MEM;
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item)
        cJSON_SetBoolValue(item, value);
    else
        cJSON_AddBoolToObject(root, key, value);

    eos_result_t ret = _eos_config_save_and_broadcast(root);

    if (ret == EOS_OK) {
        EOS_LOG_I("Successfully set config item: %s=%s", key, value ? "true" : "false");
    }

    return ret;
}

eos_result_t eos_config_set_string(const char *key, const char *value)
{
    EOS_CHECK_PTR_RETURN_VAL(key && value, EOS_ERR_VAR_NULL);

    EOS_LOG_I("Try set \"%s\" = \"%s\"", key, value);

    cJSON *root = eos_storage_json_load(EOS_CONFIG_FILE_PATH);
    if (!root) {
        root = cJSON_CreateObject();
        if (!root)
            return EOS_ERR_MEM;
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item)
        cJSON_SetValuestring(item, value);
    else
        cJSON_AddStringToObject(root, key, value);

    eos_result_t ret = _eos_config_save_and_broadcast(root);

    if (ret == EOS_OK) {
        EOS_LOG_I("Successfully set config item: %s=%s", key, value);
    }

    return ret;
}

eos_result_t eos_config_set_number(const char *key, double value)
{
    EOS_CHECK_PTR_RETURN_VAL(key, EOS_ERR_VAR_NULL);

    EOS_LOG_I("Try set \"%s\" = \"%f\"", key, value);

    cJSON *root = eos_storage_json_load(EOS_CONFIG_FILE_PATH);
    if (!root) {
        root = cJSON_CreateObject();
        if (!root)
            return EOS_ERR_MEM;
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item)
        cJSON_SetNumberValue(item, value);
    else
        cJSON_AddNumberToObject(root, key, value);

    eos_result_t ret = _eos_config_save_and_broadcast(root);

    if (ret == EOS_OK) {
        EOS_LOG_I("Successfully set config item: %s=%f", key, value);
    }

    return ret;
}

bool eos_config_get_bool(const char *key, bool default_value)
{
    return eos_storage_json_get_bool(EOS_CONFIG_FILE_PATH, key, default_value);
}

char *eos_config_get_string(const char *key, const char *default_value)
{
    return eos_storage_json_get_string(EOS_CONFIG_FILE_PATH, key, default_value);
}

double eos_config_get_number(const char *key, double default_value)
{
    return eos_storage_json_get_number(EOS_CONFIG_FILE_PATH, key, default_value);
}

eos_result_t _create_default_cfg_json(const char *path)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return EOS_ERR_JSON_ERROR;
    }

    cJSON_AddStringToObject(root, EOS_CONFIG_KEY_DEVICE_NAME_STR, EOS_CONFIG_DEFAULT_DEVICE_NAME);
    cJSON_AddStringToObject(root, EOS_CONFIG_KEY_LANGUAGE_STR, EOS_CONFIG_DEFAULT_LANG_STR);
    cJSON_AddStringToObject(root, EOS_CONFIG_KEY_WATCHFACE_ID_STR, EOS_CONFIG_DEFAULT_WATCHFACE_ID_STR);

    cJSON *app_order_array = cJSON_CreateArray();
    if (app_order_array)
        cJSON_AddItemToObject(root, EOS_CONFIG_KEY_APP_ORDER_ARRAY, app_order_array);

    // 使用 storage 服务的 JSON 接口保存文件
    eos_result_t ret = eos_storage_json_save(path, root);

    cJSON_Delete(root);

    return ret;
}

void eos_service_config_init()
{
    EOS_LOG_D("Init eos_config");
    eos_storage_mkdir_if_not_exist(EOS_SYS_DIR);
    eos_storage_mkdir_if_not_exist(EOS_CONFIG_DIR);

    eos_storage_mkdir_if_not_exist(EOS_APP_DIR);
    eos_storage_mkdir_if_not_exist(EOS_APP_INSTALLED_DIR);
    eos_storage_mkdir_if_not_exist(EOS_APP_DATA_DIR);

    eos_storage_mkdir_if_not_exist(EOS_WATCHFACE_DIR);
    eos_storage_mkdir_if_not_exist(EOS_WATCHFACE_INSTALLED_DIR);
    eos_storage_mkdir_if_not_exist(EOS_WATCHFACE_DATA_DIR);

    eos_storage_mkdir_if_not_exist(EOS_SYS_RES_DIR);
    eos_storage_mkdir_if_not_exist(EOS_SYS_RES_IMG_DIR);
    eos_storage_mkdir_if_not_exist(EOS_SYS_RES_FONT_DIR);

    // 使用 storage 服务的 JSON 接口检查并创建默认配置文件
    if (!eos_storage_is_file(EOS_CONFIG_FILE_PATH))
    {
        if (_create_default_cfg_json(EOS_CONFIG_FILE_PATH) != EOS_OK)
        {
            EOS_LOG_E("Create default config json failed");
            EOS_ASSERT(0);
        }
    }
    else
    {
        // 检查配置文件是否有效
        cJSON *root = eos_storage_json_load(EOS_CONFIG_FILE_PATH);
        if (!root)
        {
            if (_create_default_cfg_json(EOS_CONFIG_FILE_PATH) != EOS_OK)
            {
                EOS_LOG_E("Create default config json failed");
                EOS_ASSERT(0);
            }
        }
        else
        {
            cJSON_Delete(root);
        }
    }

    /************************** Load system settings **************************/
    if (eos_config_get_bool(EOS_CONFIG_KEY_BLUETOOTH_BOOL, false))
    {
        eos_bluetooth_enable();
        EOS_LOG_I("Bluetooth enable");
    }
    else
    {
        eos_bluetooth_disable();
        EOS_LOG_I("Bluetooth disable");
    }

    uint8_t brightness = (uint8_t)eos_config_get_number(EOS_CONFIG_KEY_DISPLAY_BRIGHTNESS_NUMBER, 50);
    if (brightness < EOS_DISPLAY_BRIGHTNESS_MIN || brightness > EOS_DISPLAY_BRIGHTNESS_MAX)
        brightness = 50;
    eos_display_set_brightness(brightness, EOS_DISPLAY_DURATION_OFF, false);
    EOS_LOG_I("Display brightness set: %d", brightness);

    bool mute = eos_config_get_bool(EOS_CONFIG_KEY_MUTE_BOOL, false);
    if (mute)
    {
        eos_service_audio_set_mute(true);
        EOS_LOG_I("Silent mode ON");
    }
    else
    {
        uint8_t volume = (uint8_t)eos_config_get_number(EOS_CONFIG_KEY_SPEAKER_VOLUME_NUMBER, 20);
        eos_service_audio_set_volume(volume);
        EOS_LOG_I("Volume: %d", volume);
    }

    uint8_t strength = (uint8_t)eos_config_get_number(EOS_CONFIG_KEY_VIBRATOR_STRENGTH_NUMBER, EOS_HAPTIC_STRENGTH_NORMAL);
    eos_haptic_set_strength(strength);
    EOS_LOG_I("Vibrator strength: %d", strength);
}

eos_result_t eos_config_add_item(const char *key, const char *value)
{
    if (!key || !value)
    {
        EOS_LOG_E("Invalid parameters: key or value is NULL");
        return EOS_ERR_VAR_NULL;
    }

    cJSON *root = eos_storage_json_load(EOS_CONFIG_FILE_PATH);
    if (!root)
        return EOS_ERR_FILE_ERROR;

    if (cJSON_HasObjectItem(root, key))
    {
        EOS_LOG_W("Key '%s' already exists in config", key);
        cJSON_Delete(root);
        return EOS_ERR_JSON_ERROR;
    }

    cJSON_AddStringToObject(root, key, value);

    return _eos_config_save_and_broadcast(root);
}

cJSON *eos_config_get_json(const char *key)
{
    return eos_storage_json_get_json(EOS_CONFIG_FILE_PATH, key);
}

eos_result_t eos_config_set_json(const char *key, cJSON *json_value)
{
    EOS_CHECK_PTR_RETURN_VAL(key && json_value, EOS_ERR_VAR_NULL);

    EOS_LOG_I("Try set JSON \"%s\"", key);

    cJSON *root = eos_storage_json_load(EOS_CONFIG_FILE_PATH);
    if (!root) {
        root = cJSON_CreateObject();
        if (!root)
            return EOS_ERR_MEM;
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item)
        cJSON_ReplaceItemInObject(root, key, json_value);
    else
        cJSON_AddItemToObject(root, key, json_value);

    eos_result_t ret = _eos_config_save_and_broadcast(root);

    if (ret == EOS_OK) {
        EOS_LOG_I("Successfully set JSON config item: %s", key);
    }

    return ret;
}
