/**
 * @file sni_api_eos.c
 * @brief ElenixOS API
 */

#include "sni_api_eos.h"

/* Includes ---------------------------------------------------*/
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "lvgl.h"
#include "sni_type_bridge.h"
#include "sni_types.h"
#include "sni_api_export.h"
#include "eos_log.h"
#include "eos_mem.h"
#include "script_engine_core.h"
#include "eos_font.h"
#include "eos_activity.h"
#include "eos_service_time.h"
#include "eos_app.h"
#include "eos_watchface.h"
#include "eos_service_storage.h"
#include "eos_app_header.h"
#include "eos_ww_clock_hand.h"
#include "sni_api_eos_permission.h"
/* Macros and Definitions -------------------------------------*/
#define EOS_API_NAME "eos"
#define CONSOLE_LOG_TAG script_engine_get_current_script_id()
/* Variables --------------------------------------------------*/
static jerry_value_t eos_api_obj;

typedef enum
{
    EOS_CONSOLE_LEVEL_LOG,
    EOS_CONSOLE_LEVEL_ERROR,
    EOS_CONSOLE_LEVEL_WARN,
    EOS_CONSOLE_LEVEL_DEBUG,
} eos_console_level_t;

static bool sni_api_eos_to_c_string(jerry_value_t js_val, char **out_str)
{
    if (!out_str || !jerry_value_is_string(js_val))
    {
        return false;
    }

    jerry_size_t str_len = jerry_string_size(js_val, JERRY_ENCODING_UTF8);
    char *str = eos_malloc(str_len + 1);
    if (!str)
    {
        return false;
    }

    jerry_string_to_buffer(js_val, JERRY_ENCODING_UTF8, (jerry_char_t *)str, str_len);
    str[str_len] = '\0';

    *out_str = str;
    return true;
}

static char *sni_api_eos_get_assets_file_str(jerry_value_t js_val)
{
    char *src = NULL;
    char path[EOS_FS_PATH_MAX];
    char *ret = NULL;
    const char *script_id = NULL;

    if (!sni_api_eos_to_c_string(js_val, &src))
    {
        return NULL;
    }

    script_id = script_engine_get_current_script_id();
    if (!script_id)
    {
        eos_free(src);
        return NULL;
    }

    if (script_engine_get_current_script_type() == SCRIPT_TYPE_APPLICATION)
    {
        snprintf(path, sizeof(path), EOS_APP_INSTALLED_DIR "%s/assets/%s", script_id, src);
    }
    else if (script_engine_get_current_script_type() == SCRIPT_TYPE_WATCHFACE)
    {
        snprintf(path, sizeof(path), EOS_WATCHFACE_INSTALLED_DIR "%s/assets/%s", script_id, src);
    }
    else
    {
        eos_free(src);
        return NULL;
    }

    eos_free(src);

    if (!eos_storage_is_file(path))
    {
        return NULL;
    }

    ret = eos_malloc(strlen(path) + 1);
    if (!ret)
    {
        return NULL;
    }

    strcpy(ret, path);
    return ret;
}

static bool sni_api_eos_config_write_to_file(cJSON *root)
{
    char *json_str = NULL;
    char config_file_path[EOS_FS_PATH_MAX];
    bool ret;

    if (!root)
    {
        return false;
    }

    json_str = cJSON_PrintUnformatted(root);
    if (!json_str)
    {
        return false;
    }

    if (script_engine_get_current_script_type() == SCRIPT_TYPE_APPLICATION)
    {
        snprintf(config_file_path,
                 sizeof(config_file_path),
                 EOS_APP_DATA_DIR "%s",
                 script_engine_get_current_script_id());
        eos_storage_mkdir_if_not_exist(config_file_path);
        snprintf(config_file_path,
                 sizeof(config_file_path),
                 EOS_APP_DATA_DIR "%s/config.json",
                 script_engine_get_current_script_id());
    }
    else if (script_engine_get_current_script_type() == SCRIPT_TYPE_WATCHFACE)
    {
        snprintf(config_file_path,
                 sizeof(config_file_path),
                 EOS_WATCHFACE_DATA_DIR "%s",
                 script_engine_get_current_script_id());
        eos_storage_mkdir_if_not_exist(config_file_path);
        snprintf(config_file_path,
                 sizeof(config_file_path),
                 EOS_WATCHFACE_DATA_DIR "%s/config.json",
                 script_engine_get_current_script_id());
    }
    else
    {
        eos_free(json_str);
        return false;
    }

    ret = (eos_storage_write_file(config_file_path, json_str, strlen(json_str)) == EOS_OK);

    eos_free(json_str);
    return ret;
}

static cJSON *sni_api_eos_config_load_from_file(void)
{
    char config_file_path[EOS_FS_PATH_MAX];
    char *data = NULL;
    cJSON *root = NULL;

    if (script_engine_get_current_script_type() == SCRIPT_TYPE_APPLICATION)
    {
        eos_storage_mkdir_if_not_exist(EOS_APP_DATA_DIR);
        snprintf(config_file_path,
                 sizeof(config_file_path),
                 EOS_APP_DATA_DIR "%s/config.json",
                 script_engine_get_current_script_id());
    }
    else if (script_engine_get_current_script_type() == SCRIPT_TYPE_WATCHFACE)
    {
        eos_storage_mkdir_if_not_exist(EOS_WATCHFACE_DATA_DIR);
        snprintf(config_file_path,
                 sizeof(config_file_path),
                 EOS_WATCHFACE_DATA_DIR "%s/config.json",
                 script_engine_get_current_script_id());
    }
    else
    {
        return NULL;
    }

    if (!eos_storage_is_file(config_file_path))
    {
        return cJSON_CreateObject();
    }

    data = eos_storage_read_file(config_file_path);

    if (!data)
    {
        return cJSON_CreateObject();
    }

    root = cJSON_Parse(data);
    eos_free(data);

    if (!root)
    {
        return cJSON_CreateObject();
    }

    return root;
}

static jerry_value_t sni_api_eos_console_write(const jerry_value_t args_p[],
                                               const jerry_length_t args_count,
                                               eos_console_level_t level)
{
    const char *str;

    if (args_count < 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!jerry_value_is_string(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    if (!sni_tb_js2c(args_p[0], SNI_T_STRING, &str))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    switch (level)
    {
        case EOS_CONSOLE_LEVEL_LOG:
            EOS_LOG_I("[%s] %s", CONSOLE_LOG_TAG, str);
            break;
        case EOS_CONSOLE_LEVEL_ERROR:
            EOS_LOG_E("[%s] %s", CONSOLE_LOG_TAG, str);
            break;
        case EOS_CONSOLE_LEVEL_WARN:
            EOS_LOG_W("[%s] %s", CONSOLE_LOG_TAG, str);
            break;
        case EOS_CONSOLE_LEVEL_DEBUG:
            EOS_LOG_D("[%s] %s", CONSOLE_LOG_TAG, str);
            break;
        default:
            eos_free((void *)str);
            return sni_api_throw_error("Invalid console log level");
    }

    eos_free((void *)str);

    return jerry_undefined();
}
/* Function Implementations -----------------------------------*/

jerry_value_t sni_api_eos_view_active(const jerry_call_info_t *call_info_p,
                                      const jerry_value_t args_p[],
                                      const jerry_length_t args_count)
{
    (void)args_p;
    (void)call_info_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *result = eos_view_active();
    return sni_tb_c2js(&result, SNI_H_LV_OBJ);
}

jerry_value_t sni_api_eos_config_set_str(const jerry_call_info_t *call_info_p,
                                         const jerry_value_t args_p[],
                                         const jerry_length_t args_count)
{
    char *key = NULL;
    char *value = NULL;
    cJSON *root = NULL;
    cJSON *item = NULL;

    (void)call_info_p;

    if (args_count != 2 || !jerry_value_is_string(args_p[0]) || !jerry_value_is_string(args_p[1]))
    {
        return sni_api_throw_error("Usage: config.setStr(key, value)");
    }

    if (!sni_api_eos_to_c_string(args_p[0], &key) || !sni_api_eos_to_c_string(args_p[1], &value))
    {
        if (key)
        {
            eos_free(key);
        }
        if (value)
        {
            eos_free(value);
        }
        return sni_api_throw_error("Failed to convert argument");
    }

    root = sni_api_eos_config_load_from_file();
    if (!root)
    {
        eos_free(key);
        eos_free(value);
        return sni_api_throw_error("Can't load config");
    }

    item = cJSON_GetObjectItem(root, key);
    if (item)
    {
        cJSON_ReplaceItemInObject(root, key, cJSON_CreateString(value));
    }
    else
    {
        cJSON_AddItemToObject(root, key, cJSON_CreateString(value));
    }

    sni_api_eos_config_write_to_file(root);
    cJSON_Delete(root);
    eos_free(key);
    eos_free(value);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_config_set_bool(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_count)
{
    char *key = NULL;
    bool value;
    cJSON *root = NULL;
    cJSON *item = NULL;

    (void)call_info_p;

    if (args_count != 2 || !jerry_value_is_string(args_p[0]) || !jerry_value_is_boolean(args_p[1]))
    {
        return sni_api_throw_error("Usage: config.setBool(key, bool)");
    }

    if (!sni_api_eos_to_c_string(args_p[0], &key))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    value = jerry_value_is_true(args_p[1]);
    root = sni_api_eos_config_load_from_file();
    if (!root)
    {
        eos_free(key);
        return sni_api_throw_error("Can't load config");
    }

    item = cJSON_GetObjectItem(root, key);
    if (item)
    {
        cJSON_ReplaceItemInObject(root, key, cJSON_CreateBool(value));
    }
    else
    {
        cJSON_AddItemToObject(root, key, cJSON_CreateBool(value));
    }

    sni_api_eos_config_write_to_file(root);
    cJSON_Delete(root);
    eos_free(key);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_config_set_number(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    char *key = NULL;
    double value;
    cJSON *root = NULL;
    cJSON *item = NULL;

    (void)call_info_p;

    if (args_count != 2 || !jerry_value_is_string(args_p[0]) || !jerry_value_is_number(args_p[1]))
    {
        return sni_api_throw_error("Usage: config.setNumber(key, number)");
    }

    if (!sni_api_eos_to_c_string(args_p[0], &key))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    value = jerry_value_as_number(args_p[1]);
    root = sni_api_eos_config_load_from_file();
    if (!root)
    {
        eos_free(key);
        return sni_api_throw_error("Can't load config");
    }

    item = cJSON_GetObjectItem(root, key);
    if (item)
    {
        cJSON_ReplaceItemInObject(root, key, cJSON_CreateNumber(value));
    }
    else
    {
        cJSON_AddItemToObject(root, key, cJSON_CreateNumber(value));
    }

    sni_api_eos_config_write_to_file(root);
    cJSON_Delete(root);
    eos_free(key);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_config_get_str(const jerry_call_info_t *call_info_p,
                                         const jerry_value_t args_p[],
                                         const jerry_length_t args_count)
{
    char *key = NULL;
    cJSON *root = NULL;
    cJSON *item = NULL;
    jerry_value_t ret = jerry_undefined();

    (void)call_info_p;

    if (args_count != 1 || !jerry_value_is_string(args_p[0]))
    {
        return sni_api_throw_error("Usage: config.getStr(key)");
    }

    if (!sni_api_eos_to_c_string(args_p[0], &key))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    root = sni_api_eos_config_load_from_file();
    if (!root)
    {
        eos_free(key);
        return sni_api_throw_error("Can't load config");
    }

    item = cJSON_GetObjectItem(root, key);
    if (item && cJSON_IsString(item))
    {
        ret = jerry_string_sz(item->valuestring);
    }

    cJSON_Delete(root);
    eos_free(key);
    return ret;
}

jerry_value_t sni_api_eos_config_get_bool(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_count)
{
    char *key = NULL;
    cJSON *root = NULL;
    cJSON *item = NULL;
    jerry_value_t ret = jerry_boolean(false);

    (void)call_info_p;

    if (args_count != 1 || !jerry_value_is_string(args_p[0]))
    {
        return sni_api_throw_error("Usage: config.getBool(key)");
    }

    if (!sni_api_eos_to_c_string(args_p[0], &key))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    root = sni_api_eos_config_load_from_file();
    if (!root)
    {
        eos_free(key);
        return sni_api_throw_error("Can't load config");
    }

    item = cJSON_GetObjectItem(root, key);
    if (item && cJSON_IsBool(item))
    {
        ret = jerry_boolean(item->valueint != 0);
    }

    cJSON_Delete(root);
    eos_free(key);
    return ret;
}

jerry_value_t sni_api_eos_config_get_number(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    char *key = NULL;
    cJSON *root = NULL;
    cJSON *item = NULL;
    jerry_value_t ret = jerry_number(0);

    (void)call_info_p;

    if (args_count != 1 || !jerry_value_is_string(args_p[0]))
    {
        return sni_api_throw_error("Usage: config.getNumber(key)");
    }

    if (!sni_api_eos_to_c_string(args_p[0], &key))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    root = sni_api_eos_config_load_from_file();
    if (!root)
    {
        eos_free(key);
        return sni_api_throw_error("Can't load config");
    }

    item = cJSON_GetObjectItem(root, key);
    if (item && cJSON_IsNumber(item))
    {
        ret = jerry_number(item->valuedouble);
    }

    cJSON_Delete(root);
    eos_free(key);
    return ret;
}

jerry_value_t sni_api_eos_time_get_now(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args_p[],
                                       const jerry_length_t args_count)
{
    eos_datetime_t dt;
    jerry_value_t obj;

    (void)call_info_p;
    (void)args_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    dt = eos_time_get();
    obj = jerry_object();
    script_engine_set_prop_number(obj, "year", dt.year);
    script_engine_set_prop_number(obj, "month", dt.month);
    script_engine_set_prop_number(obj, "day", dt.day);
    script_engine_set_prop_number(obj, "hour", dt.hour);
    script_engine_set_prop_number(obj, "min", dt.min);
    script_engine_set_prop_number(obj, "sec", dt.sec);
    script_engine_set_prop_number(obj, "ms", dt.ms);
    script_engine_set_prop_number(obj, "day_of_week", dt.day_of_week);

    return obj;
}

jerry_value_t sni_api_eos_app_header_set_title(const jerry_call_info_t *call_info_p,
                                               const jerry_value_t args_p[],
                                               const jerry_length_t args_count)
{
    eos_activity_t *current;
    char *title = NULL;
    lv_obj_t *view;

    (void)call_info_p;

    if (args_count != 2)
    {
        return sni_api_throw_error("Usage: appHeader.setTitle(view, title)");
    }

    if (!jerry_value_is_null(args_p[0]) && !jerry_value_is_object(args_p[0]))
    {
        return sni_api_throw_error("Invalid view argument type");
    }

    if (!jerry_value_is_null(args_p[0]))
    {
        if (!sni_tb_js2c(args_p[0], SNI_H_LV_OBJ, &view))
        {
            return sni_api_throw_error("Invalid view argument");
        }
    }

    if (!jerry_value_is_null(args_p[1]) && !jerry_value_is_undefined(args_p[1]))
    {
        if (!sni_api_eos_to_c_string(args_p[1], &title))
        {
            return sni_api_throw_error("Invalid title argument");
        }
    }

    current = eos_activity_get_current();
    if (!current)
    {
        if (title)
        {
            eos_free(title);
        }
        return sni_api_throw_error("No current activity");
    }

    eos_activity_set_title(current, title);
    if (title)
    {
        eos_free(title);
    }

    return jerry_undefined();
}

jerry_value_t sni_api_eos_app_header_hide(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_count)
{
    (void)call_info_p;
    (void)args_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    eos_app_header_hide();
    return jerry_undefined();
}

jerry_value_t sni_api_eos_app_header_show(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_count)
{
    eos_activity_t *current;

    (void)call_info_p;
    (void)args_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    current = eos_activity_get_current();
    if (!current)
    {
        return sni_api_throw_error("No current activity");
    }

    eos_app_header_show(current);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_clock_hand_create(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    lv_obj_t *obj;
    char *src;
    int32_t type;
    int32_t cx;
    int32_t cy;
    lv_obj_t *ret_obj;

    (void)call_info_p;

    if (args_count != 5)
    {
        return sni_api_throw_error("Usage: clockHand.create(obj, src, type, cx, cy)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_LV_OBJ, &obj))
    {
        return sni_api_throw_error("Invalid object argument");
    }

    if (!jerry_value_is_string(args_p[1]) || !jerry_value_is_number(args_p[2]) || !jerry_value_is_number(args_p[3])
        || !jerry_value_is_number(args_p[4]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    src = sni_api_eos_get_assets_file_str(args_p[1]);
    if (!src)
    {
        return sni_api_throw_error("Invalid image source");
    }

    type = (int32_t)jerry_value_as_number(args_p[2]);
    cx = (int32_t)jerry_value_as_number(args_p[3]);
    cy = (int32_t)jerry_value_as_number(args_p[4]);

    ret_obj = eos_clock_hand_create(obj, src, (eos_clock_hand_type_t)type, cx, cy);
    eos_free(src);
    return sni_tb_c2js(&ret_obj, SNI_H_LV_OBJ);
}

jerry_value_t sni_api_eos_clock_hand_center(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    lv_obj_t *obj;

    (void)call_info_p;

    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: clockHand.center(obj)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_LV_OBJ, &obj))
    {
        return sni_api_throw_error("Invalid object argument");
    }

    eos_clock_hand_center(obj);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_clock_hand_place_pivot(const jerry_call_info_t *call_info_p,
                                                 const jerry_value_t args_p[],
                                                 const jerry_length_t args_count)
{
    lv_obj_t *obj;
    int32_t x;
    int32_t y;

    (void)call_info_p;

    if (args_count != 3)
    {
        return sni_api_throw_error("Usage: clockHand.placePivot(obj, x, y)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_LV_OBJ, &obj) || !jerry_value_is_number(args_p[1])
        || !jerry_value_is_number(args_p[2]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    x = (int32_t)jerry_value_as_number(args_p[1]);
    y = (int32_t)jerry_value_as_number(args_p[2]);
    eos_clock_hand_place_pivot(obj, x, y);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_clock_hand_attach(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    lv_obj_t *obj;
    int32_t type;

    (void)call_info_p;

    if (args_count != 2)
    {
        return sni_api_throw_error("Usage: clockHand.attach(obj, type)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_LV_OBJ, &obj) || !jerry_value_is_number(args_p[1]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    type = (int32_t)jerry_value_as_number(args_p[1]);
    eos_clock_hand_attach(obj, (eos_clock_hand_type_t)type);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_clock_hand_center_style(const jerry_call_info_t *call_info_p,
                                                  const jerry_value_t args_p[],
                                                  const jerry_length_t args_count)
{
    lv_obj_t *obj;
    int32_t px;
    int32_t py;

    (void)call_info_p;

    if (args_count != 3)
    {
        return sni_api_throw_error("Usage: clockHand.centerStyle(obj, pivotX, pivotY)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_LV_OBJ, &obj) || !jerry_value_is_number(args_p[1])
        || !jerry_value_is_number(args_p[2]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    px = (int32_t)jerry_value_as_number(args_p[1]);
    py = (int32_t)jerry_value_as_number(args_p[2]);
    eos_clock_hand_center_style(obj, px, py);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_activity_current(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args_p[],
                                           const jerry_length_t args_count)
{
    eos_activity_t *activity;

    (void)call_info_p;
    (void)args_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    activity = eos_activity_get_current();
    return sni_tb_c2js(&activity, SNI_H_EOS_ACTIVITY);
}

jerry_value_t sni_api_eos_activity_visible(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args_p[],
                                           const jerry_length_t args_count)
{
    eos_activity_t *activity;

    (void)call_info_p;
    (void)args_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    activity = eos_activity_get_visible();
    return sni_tb_c2js(&activity, SNI_H_EOS_ACTIVITY);
}

jerry_value_t sni_api_eos_activity_bottom(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_count)
{
    eos_activity_t *activity;

    (void)call_info_p;
    (void)args_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    activity = eos_activity_get_bottom();
    return sni_tb_c2js(&activity, SNI_H_EOS_ACTIVITY);
}

jerry_value_t sni_api_eos_activity_watchface(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args_p[],
                                             const jerry_length_t args_count)
{
    eos_activity_t *activity;

    (void)call_info_p;
    (void)args_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    activity = eos_activity_get_watchface();
    return sni_tb_c2js(&activity, SNI_H_EOS_ACTIVITY);
}

jerry_value_t sni_api_eos_activity_get_view(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    eos_activity_t *activity;
    lv_obj_t *view;

    (void)call_info_p;

    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: activity.getView(activity)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_EOS_ACTIVITY, &activity))
    {
        return sni_api_throw_error("Invalid activity argument");
    }

    view = eos_activity_get_view(activity);
    return sni_tb_c2js(&view, SNI_H_LV_OBJ);
}

jerry_value_t sni_api_eos_activity_set_view(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    eos_activity_t *activity;
    lv_obj_t *view;

    (void)call_info_p;

    if (args_count != 2)
    {
        return sni_api_throw_error("Usage: activity.setView(activity, view)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_EOS_ACTIVITY, &activity) || !sni_tb_js2c(args_p[1], SNI_H_LV_OBJ, &view))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    eos_activity_set_view(activity, view);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_activity_get_title(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args_p[],
                                             const jerry_length_t args_count)
{
    eos_activity_t *activity;
    const char *title;

    (void)call_info_p;

    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: activity.getTitle(activity)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_EOS_ACTIVITY, &activity))
    {
        return sni_api_throw_error("Invalid activity argument");
    }

    title = eos_activity_get_title(activity);
    if (!title)
    {
        return jerry_undefined();
    }

    return jerry_string_sz(title);
}

jerry_value_t sni_api_eos_activity_set_title(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args_p[],
                                             const jerry_length_t args_count)
{
    eos_activity_t *activity;
    char *title = NULL;

    (void)call_info_p;

    if (args_count != 2 || !jerry_value_is_string(args_p[1]))
    {
        return sni_api_throw_error("Usage: activity.setTitle(activity, title)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_EOS_ACTIVITY, &activity) || !sni_api_eos_to_c_string(args_p[1], &title))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    eos_activity_set_title(activity, title);
    eos_free(title);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_activity_set_type(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    eos_activity_t *activity;
    int32_t type;

    (void)call_info_p;

    if (args_count != 2 || !jerry_value_is_number(args_p[1]))
    {
        return sni_api_throw_error("Usage: activity.setType(activity, type)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_EOS_ACTIVITY, &activity))
    {
        return sni_api_throw_error("Invalid activity argument");
    }

    type = (int32_t)jerry_value_as_number(args_p[1]);
    eos_activity_set_type(activity, (eos_activity_type_t)type);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_activity_get_type(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    eos_activity_t *activity;
    eos_activity_type_t type;

    (void)call_info_p;

    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: activity.getType(activity)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_EOS_ACTIVITY, &activity))
    {
        return sni_api_throw_error("Invalid activity argument");
    }

    type = eos_activity_get_type(activity);
    return jerry_number((double)type);
}

jerry_value_t sni_api_eos_activity_set_app_header_visible(const jerry_call_info_t *call_info_p,
                                                          const jerry_value_t args_p[],
                                                          const jerry_length_t args_count)
{
    eos_activity_t *activity;
    bool visible;

    (void)call_info_p;

    if (args_count != 2 || !jerry_value_is_boolean(args_p[1]))
    {
        return sni_api_throw_error("Usage: activity.setAppHeaderVisible(activity, visible)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_EOS_ACTIVITY, &activity))
    {
        return sni_api_throw_error("Invalid activity argument");
    }

    visible = jerry_value_is_true(args_p[1]);
    eos_activity_set_app_header_visible(activity, visible);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_activity_is_app_header_visible(const jerry_call_info_t *call_info_p,
                                                         const jerry_value_t args_p[],
                                                         const jerry_length_t args_count)
{
    eos_activity_t *activity;
    bool visible;

    (void)call_info_p;

    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: activity.isAppHeaderVisible(activity)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_EOS_ACTIVITY, &activity))
    {
        return sni_api_throw_error("Invalid activity argument");
    }

    visible = eos_activity_is_app_header_visible(activity);
    return jerry_boolean(visible);
}

jerry_value_t sni_api_eos_activity_enter(const jerry_call_info_t *call_info_p,
                                         const jerry_value_t args_p[],
                                         const jerry_length_t args_count)
{
    eos_activity_t *activity;

    (void)call_info_p;

    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: activity.enter(activity)");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_EOS_ACTIVITY, &activity))
    {
        return sni_api_throw_error("Invalid activity argument");
    }

    eos_activity_enter(activity);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_activity_back(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    eos_result_t ret;

    (void)call_info_p;
    (void)args_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ret = eos_activity_back();
    return jerry_boolean(ret == EOS_OK);
}

jerry_value_t sni_api_eos_activity_root_screen(const jerry_call_info_t *call_info_p,
                                               const jerry_value_t args_p[],
                                               const jerry_length_t args_count)
{
    lv_obj_t *screen;

    (void)call_info_p;
    (void)args_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    screen = eos_activity_get_root_screen();
    return sni_tb_c2js(&screen, SNI_H_LV_OBJ);
}

jerry_value_t sni_api_eos_activity_is_transition_in_progress(const jerry_call_info_t *call_info_p,
                                                             const jerry_value_t args_p[],
                                                             const jerry_length_t args_count)
{
    (void)call_info_p;
    (void)args_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    return jerry_boolean(eos_activity_is_transition_in_progress());
}

jerry_value_t sni_api_eos_console_log(const jerry_call_info_t *call_info_p,
                                      const jerry_value_t args_p[],
                                      const jerry_length_t args_count)
{
    (void)call_info_p;

    return sni_api_eos_console_write(args_p, args_count, EOS_CONSOLE_LEVEL_LOG);
}

jerry_value_t sni_api_eos_console_error(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    (void)call_info_p;

    return sni_api_eos_console_write(args_p, args_count, EOS_CONSOLE_LEVEL_ERROR);
}

jerry_value_t sni_api_eos_console_warn(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args_p[],
                                       const jerry_length_t args_count)
{
    (void)call_info_p;

    return sni_api_eos_console_write(args_p, args_count, EOS_CONSOLE_LEVEL_WARN);
}

jerry_value_t sni_api_eos_console_debug(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    (void)call_info_p;

    return sni_api_eos_console_write(args_p, args_count, EOS_CONSOLE_LEVEL_DEBUG);
}

const sni_method_desc_t eos_class_static_methods_view[] = {
    {.name = "active", .handler = sni_api_eos_view_active},
    {.name = NULL, .handler = NULL},
};

const sni_method_desc_t eos_class_static_methods_console[] = {
    {.name = "log", .handler = sni_api_eos_console_log},
    {.name = "error", .handler = sni_api_eos_console_error},
    {.name = "warn", .handler = sni_api_eos_console_warn},
    {.name = "info", .handler = sni_api_eos_console_log},
    {.name = "debug", .handler = sni_api_eos_console_debug},
    {.name = NULL, .handler = NULL},
};

const sni_method_desc_t eos_class_static_methods_config[] = {
    {.name = "setStr", .handler = sni_api_eos_config_set_str},
    {.name = "setBool", .handler = sni_api_eos_config_set_bool},
    {.name = "setNumber", .handler = sni_api_eos_config_set_number},
    {.name = "getStr", .handler = sni_api_eos_config_get_str},
    {.name = "getBool", .handler = sni_api_eos_config_get_bool},
    {.name = "getNumber", .handler = sni_api_eos_config_get_number},
    {.name = NULL, .handler = NULL},
};

const sni_method_desc_t eos_class_static_methods_time[] = {
    {.name = "getNow", .handler = sni_api_eos_time_get_now},
    {.name = NULL, .handler = NULL},
};

const sni_method_desc_t eos_class_static_methods_app_header[] = {
    {.name = "setTitle", .handler = sni_api_eos_app_header_set_title},
    {.name = "hide", .handler = sni_api_eos_app_header_hide},
    {.name = "show", .handler = sni_api_eos_app_header_show},
    {.name = NULL, .handler = NULL},
};

const sni_method_desc_t eos_class_static_methods_clock_hand[] = {
    {.name = "create", .handler = sni_api_eos_clock_hand_create},
    {.name = "center", .handler = sni_api_eos_clock_hand_center},
    {.name = "placePivot", .handler = sni_api_eos_clock_hand_place_pivot},
    {.name = "attach", .handler = sni_api_eos_clock_hand_attach},
    {.name = "centerStyle", .handler = sni_api_eos_clock_hand_center_style},
    {.name = NULL, .handler = NULL},
};

const sni_method_desc_t eos_class_static_methods_activity[] = {
    {.name = "current", .handler = sni_api_eos_activity_current},
    {.name = "visible", .handler = sni_api_eos_activity_visible},
    {.name = "bottom", .handler = sni_api_eos_activity_bottom},
    {.name = "watchface", .handler = sni_api_eos_activity_watchface},
    {.name = "rootScreen", .handler = sni_api_eos_activity_root_screen},
    {.name = "getView", .handler = sni_api_eos_activity_get_view},
    {.name = "setView", .handler = sni_api_eos_activity_set_view},
    {.name = "getTitle", .handler = sni_api_eos_activity_get_title},
    {.name = "setTitle", .handler = sni_api_eos_activity_set_title},
    {.name = "getType", .handler = sni_api_eos_activity_get_type},
    {.name = "setType", .handler = sni_api_eos_activity_set_type},
    {.name = "setAppHeaderVisible", .handler = sni_api_eos_activity_set_app_header_visible},
    {.name = "isAppHeaderVisible", .handler = sni_api_eos_activity_is_app_header_visible},
    {.name = "enter", .handler = sni_api_eos_activity_enter},
    {.name = "back", .handler = sni_api_eos_activity_back},
    {.name = "isTransitionInProgress", .handler = sni_api_eos_activity_is_transition_in_progress},
    {.name = NULL, .handler = NULL},
};

const sni_class_desc_t eos_class_desc_view = {
    .name = "view",
    .constructor = NULL,
    .base_class = NULL,
    .methods = NULL,
    .properties = NULL,
    .static_methods = eos_class_static_methods_view,
    .constants = NULL,
};

const sni_class_desc_t eos_class_desc_console = {
    .name = "console",
    .constructor = NULL,
    .base_class = NULL,
    .methods = NULL,
    .properties = NULL,
    .static_methods = eos_class_static_methods_console,
    .constants = NULL,
};

const sni_class_desc_t eos_class_desc_config = {
    .name = "config",
    .constructor = NULL,
    .base_class = NULL,
    .methods = NULL,
    .properties = NULL,
    .static_methods = eos_class_static_methods_config,
    .constants = NULL,
};

const sni_class_desc_t eos_class_desc_time = {
    .name = "time",
    .constructor = NULL,
    .base_class = NULL,
    .methods = NULL,
    .properties = NULL,
    .static_methods = eos_class_static_methods_time,
    .constants = NULL,
};

const sni_class_desc_t eos_class_desc_app_header = {
    .name = "appHeader",
    .constructor = NULL,
    .base_class = NULL,
    .methods = NULL,
    .properties = NULL,
    .static_methods = eos_class_static_methods_app_header,
    .constants = NULL,
};

const sni_class_desc_t eos_class_desc_clock_hand = {
    .name = "clockHand",
    .constructor = NULL,
    .base_class = NULL,
    .methods = NULL,
    .properties = NULL,
    .static_methods = eos_class_static_methods_clock_hand,
    .constants = NULL,
};

const sni_class_desc_t eos_class_desc_activity = {
    .name = "activity",
    .constructor = NULL,
    .base_class = NULL,
    .methods = NULL,
    .properties = NULL,
    .static_methods = eos_class_static_methods_activity,
    .constants = NULL,
};

const sni_method_desc_t eos_class_static_methods_permission[] = {
    {.name = "request", .handler = sni_api_eos_permission_request},
    {.name = "check", .handler = sni_api_eos_permission_check},
    {.name = NULL, .handler = NULL},
};

const sni_class_desc_t eos_class_desc_permission = {
    .name = "permission",
    .constructor = NULL,
    .base_class = NULL,
    .methods = NULL,
    .properties = NULL,
    .static_methods = eos_class_static_methods_permission,
    .constants = NULL,
};

const sni_class_desc_t *const eos_api_classes[] = {
    &eos_class_desc_view,
    &eos_class_desc_console,
    &eos_class_desc_config,
    &eos_class_desc_time,
    &eos_class_desc_app_header,
    &eos_class_desc_clock_hand,
    &eos_class_desc_activity,
    &eos_class_desc_permission,
    NULL,
};

const sni_constant_desc_t eos_root_constants[] = {
    {.name = "FONT_SIZE_LARGE", .type = SNI_CONST_INT, .value.i = EOS_FONT_SIZE_LARGE},
    {.name = "FONT_SIZE_MEDIUM", .type = SNI_CONST_INT, .value.i = EOS_FONT_SIZE_MEDIUM},
    {.name = "FONT_SIZE_SMALL", .type = SNI_CONST_INT, .value.i = EOS_FONT_SIZE_SMALL},
    {.name = "DISPLAY_WIDTH", .type = SNI_CONST_INT, .value.i = EOS_DISPLAY_WIDTH},
    {.name = "DISPLAY_HEIGHT", .type = SNI_CONST_INT, .value.i = EOS_DISPLAY_HEIGHT},
    {.name = "CLOCK_HAND_HOUR", .type = SNI_CONST_INT, .value.i = EOS_CLOCK_HAND_HOUR},
    {.name = "CLOCK_HAND_MINUTE", .type = SNI_CONST_INT, .value.i = EOS_CLOCK_HAND_MINUTE},
    {.name = "CLOCK_HAND_SECOND", .type = SNI_CONST_INT, .value.i = EOS_CLOCK_HAND_SECOND},
    {.name = "ACTIVITY_TYPE_NULL", .type = SNI_CONST_INT, .value.i = EOS_ACTIVITY_TYPE_NULL},
    {.name = "ACTIVITY_TYPE_APP", .type = SNI_CONST_INT, .value.i = EOS_ACTIVITY_TYPE_APP},
    {.name = "ACTIVITY_TYPE_APP_LIST", .type = SNI_CONST_INT, .value.i = EOS_ACTIVITY_TYPE_APP_LIST},
    {.name = "ACTIVITY_TYPE_WATCHFACE", .type = SNI_CONST_INT, .value.i = EOS_ACTIVITY_TYPE_WATCHFACE},
    {.name = "ACTIVITY_TYPE_WATCHFACE_LIST", .type = SNI_CONST_INT, .value.i = EOS_ACTIVITY_TYPE_WATCHFACE_LIST},
    {.name = NULL, .type = SNI_CONST_INT, .value.i = 0},
};

void sni_api_eos_init(void)
{
    eos_api_obj = sni_api_build(eos_api_classes);
    if (!jerry_value_is_object(eos_api_obj))
    {
        EOS_LOG_E("Failed to build ElenixOS API object");
        return;
    }
    if (!sni_api_register_constants(eos_root_constants, eos_api_obj))
    {
        EOS_LOG_E("Failed to register ElenixOS API constants");
    }
}

void sni_api_eos_mount(jerry_value_t realm)
{
    bool result = sni_api_mount(realm, eos_api_obj, EOS_API_NAME);
    if (!result)
    {
        EOS_LOG_E("Failed to mount ElenixOS API");
    }
}
