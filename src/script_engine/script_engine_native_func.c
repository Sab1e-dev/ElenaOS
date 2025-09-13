
/**
 * @file script_engine_native_func.c
 * @brief 原生函数实现及注册
 * @author Sab1e
 * @date 2025-07-26
 */

// Includes
#include "script_engine_native_func.h"
#include "jerryscript.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "lvgl.h"
#include "script_engine_core.h"
#include "elena_os_port.h"
#include "script_engine_nav.h"
#include "elena_os_img.h"
#include "elena_os_app.h"
#include "elena_os_watchface.h"
#include "elena_os_misc.h"
#include "elena_os_log.h"
#include "elena_os_port.h"
// Macros and Definitions

// Variables
extern script_pkg_t script_pkg;

// Function Implementations
/********************************** 错误处理 **********************************/
static jerry_value_t throw_error(const char *message)
{
    EOS_LOG_E("%s", message);
    jerry_value_t error_obj = jerry_error_sz(JERRY_ERROR_TYPE, (const jerry_char_t *)message);
    return jerry_throw_value(error_obj, true);
}
/********************************** 辅助函数 **********************************/
// 内部工具函数：写入配置文件
static bool config_write_to_file(cJSON *root)
{
    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str)
        return false;
    char config_file_path[PATH_MAX];
    if (script_pkg.type == SCRIPT_TYPE_APPLICATION)
    {
        snprintf(config_file_path, sizeof(config_file_path), EOS_APP_DATA_DIR "%s/config.json",
                 script_pkg.id);
    }
    else if (script_pkg.type == SCRIPT_TYPE_WATCHFACE)
    {
        snprintf(config_file_path, sizeof(config_file_path), EOS_WATCHFACE_DATA_DIR "%s/config.json",
                 script_pkg.id);
    }
    else
    {
        EOS_LOG_E("Unknown script type");
        return false;
    }
    EOS_LOG_D("Writing file: %s", config_file_path);
    FILE *fp = fopen(config_file_path, "w");
    if (!fp)
    {
        EOS_LOG_E("Open file failed");
        free(json_str);
        return false;
    }
    fputs(json_str, fp);
    fclose(fp);
    free(json_str);
    return true;
}

// 内部工具函数：加载配置文件
static cJSON *config_load_from_file(void)
{
    char config_file_path[PATH_MAX];
    if (script_pkg.type == SCRIPT_TYPE_APPLICATION)
    {
        snprintf(config_file_path, sizeof(config_file_path), EOS_APP_DATA_DIR "%s/config.json",
                 script_pkg.id);
    }
    else if (script_pkg.type == SCRIPT_TYPE_WATCHFACE)
    {
        snprintf(config_file_path, sizeof(config_file_path), EOS_WATCHFACE_DATA_DIR "%s/config.json",
                 script_pkg.id);
    }
    else
    {
        EOS_LOG_E("Unknown script type");
        return NULL;
    }
    EOS_LOG_D("Load from file: %s", config_file_path);
    if (!eos_is_file(config_file_path))
    {
        return cJSON_CreateObject();
    }

    FILE *fp = fopen(config_file_path, "rb");
    if (!fp)
    {
        EOS_LOG_E("Open file failed");
        return cJSON_CreateObject();
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *data = (char *)malloc(size + 1);
    fread(data, 1, size, fp);
    data[size] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(data);
    free(data);

    if (!root)
    {
        return cJSON_CreateObject();
    }
    return root;
}
/********************************** 原生函数定义 **********************************/
/**
 * @brief 处理 JavaScript 的 print 调用
 *
 * 将所有参数转换为字符串并打印到标准输出。
 * 每个参数之间以空格分隔，末尾换行。
 * 适用于 JerryScript 引擎的原生函数绑定。
 *
 * @param call_info_p 指向调用信息的指针，当前未使用，可忽略。类型为 const jerry_call_info_t*。
 * @param args_p 参数数组，包含所有传入的 JavaScript 参数。类型为 const jerry_value_t[]。
 * @param args_count 参数数量，表示 args_p 数组的长度。类型为 jerry_length_t。
 * @return 返回一个未定义值（jerry_undefined），表示没有返回结果。
 */
jerry_value_t js_print_handler(const jerry_call_info_t *call_info_p,
                               const jerry_value_t args_p[],
                               const jerry_length_t args_count)
{
    (void)call_info_p; // 当前未用到 this/func_obj，可忽略

    for (jerry_length_t i = 0; i < args_count; i++)
    {
        jerry_value_t str_val;

        if (jerry_value_is_string(args_p[i]))
        {
            str_val = jerry_value_copy(args_p[i]);
        }
        else
        {
            str_val = jerry_value_to_string(args_p[i]); // 转为字符串
        }

        jerry_size_t size = jerry_string_size(str_val, JERRY_ENCODING_UTF8);
        jerry_char_t *buf = (jerry_char_t *)malloc(size + 1); // Explicitly cast void* to jerry_char_t*
        if (!buf)
        {
            jerry_value_free(str_val);
            continue;
        }

        jerry_string_to_buffer(str_val, JERRY_ENCODING_UTF8, buf, size);
        buf[size] = '\0';

        printf("%s", buf);
        if (i < args_count - 1)
        {
            printf(" ");
        }

        free(buf);
        jerry_value_free(str_val);
    }

    printf("\n");
    return jerry_undefined();
}
/**
 * @brief Native 延时
 */
jerry_value_t js_delay_handler(const jerry_call_info_t *call_info_p,
                               const jerry_value_t args_p[],
                               const jerry_length_t args_count)
{
    eos_delay(args_p[0]);
}
/**
 * @brief 在脚本专用的导航栈上创建新的 screen
 * @param void 无参数
 * @return lv_obj_t* 返回 LVGL 对象指针
 */
static jerry_value_t js_nav_scr_create(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc)
{
    if (script_pkg.type == SCRIPT_TYPE_WATCHFACE)
    {
        return throw_error("Watchface can't create screen");
    }
    // 调用底层函数
    lv_obj_t *ret_value = script_engine_nav_scr_create();

    // 处理返回值
    jerry_value_t js_result;
    // 包装为LVGL对象
    js_result = jerry_object();
    jerry_value_t ptr = jerry_number((double)(uintptr_t)ret_value);
    jerry_value_t cls = jerry_string_sz("lv_obj");
    jerry_object_set(js_result, jerry_string_sz("__ptr"), ptr);
    jerry_object_set(js_result, jerry_string_sz("__class"), cls);
    jerry_value_free(ptr);
    jerry_value_free(cls);

    return js_result;
}
/**
 * @brief 在导航栈上返回上一级
 * @param void 无参数
 * @return boolean 是否返回成功
 */
static jerry_value_t js_nav_back(const jerry_call_info_t *call_info_p,
                                 const jerry_value_t args[],
                                 const jerry_length_t argc)
{
    if (script_pkg.type == SCRIPT_TYPE_WATCHFACE)
    {
        return throw_error("Using navigation in the watchface is prohibited");
    }
    // 调用底层函数
    script_engine_result_t ret = script_engine_nav_back_clean();

    jerry_value_t js_result;

    if (ret == SE_OK)
    {
        js_result = jerry_boolean(true);
    }
    else
    {
        js_result = jerry_boolean(false);
    }

    return js_result;
}

static jerry_value_t js_lv_img_set_src(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc)
{
    // 参数数量检查
    if (argc < 2)
    {
        return throw_error("Insufficient arguments");
    }

    // 解析参数: obj (lv_obj_t*)
    // 对象类型参数，支持null
    void *arg_obj = NULL;
    if (!jerry_value_is_undefined(args[0]) && !jerry_value_is_null(args[0]))
    {
        jerry_value_t js_arg_obj = args[0];
        if (!jerry_value_is_object(js_arg_obj))
        {
            return throw_error("Argument 0 must be an object or null");
        }

        jerry_value_t arg_obj_ptr_prop = jerry_string_sz("__ptr");
        jerry_value_t arg_obj_ptr_val = jerry_object_get(js_arg_obj, arg_obj_ptr_prop);
        jerry_value_free(arg_obj_ptr_prop);

        if (!jerry_value_is_number(arg_obj_ptr_val))
        {
            jerry_value_free(arg_obj_ptr_val);
            return throw_error("Invalid __ptr property");
        }

        uintptr_t arg_obj_ptr = (uintptr_t)jerry_value_as_number(arg_obj_ptr_val);
        jerry_value_free(arg_obj_ptr_val);
        arg_obj = (void *)arg_obj_ptr;
    }

    // 解析参数: src (const char*)
    char *arg_src_str = NULL;
    const char *arg_src = NULL;
    if (!jerry_value_is_undefined(args[1]) && !jerry_value_is_null(args[1]))
    {
        if (!jerry_value_is_string(args[1]))
        {
            return throw_error("Argument 1 must be a string");
        }
        jerry_size_t arg_src_len = jerry_string_size(args[1], JERRY_ENCODING_UTF8);
        arg_src_str = (char *)malloc(arg_src_len + 1);
        jerry_string_to_buffer(args[1], JERRY_ENCODING_UTF8, (jerry_char_t *)arg_src_str, arg_src_len);
        arg_src_str[arg_src_len] = '\0';
        arg_src = arg_src_str;
    }
    char img_path[PATH_MAX];
    if (script_pkg.id == NULL)
    {
        return throw_error("Script package info is NULL");
    }
    if (script_pkg.type == SCRIPT_TYPE_APPLICATION)
    {
        snprintf(img_path, sizeof(img_path), EOS_APP_INSTALLED_DIR "%s/assets/%s",
                 script_pkg.id, arg_src);
    }
    else if (script_pkg.type == SCRIPT_TYPE_WATCHFACE)
    {
        snprintf(img_path, sizeof(img_path), EOS_WATCHFACE_INSTALLED_DIR "%s/assets/%s",
                 script_pkg.id, arg_src);
    }
    else
    {
        return throw_error("Unknown script type");
    }
    if (!eos_is_file(img_path))
    {
        return throw_error("Not a file");
    }
    EOS_LOG_D("Image Path: %s", img_path);
    // 调用底层函数
    eos_img_set_src(arg_obj, img_path);

    // 释放临时字符串内存
    if (arg_src_str)
        free(arg_src_str);

    return jerry_undefined();
}

// 设置字符串配置项
static jerry_value_t js_config_set_str(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc)
{
    if (argc < 2 || !jerry_value_is_string(args[0]) || !jerry_value_is_string(args[1]))
    {
        return throw_error("Usage: config_set_str(key, value)");
    }

    // key
    jerry_size_t key_len = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    char *key = malloc(key_len + 1);
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, (jerry_char_t *)key, key_len);
    key[key_len] = '\0';

    // value
    jerry_size_t val_len = jerry_string_size(args[1], JERRY_ENCODING_UTF8);
    char *value = malloc(val_len + 1);
    jerry_string_to_buffer(args[1], JERRY_ENCODING_UTF8, (jerry_char_t *)value, val_len);
    value[val_len] = '\0';

    // JSON 写入
    cJSON *root = config_load_from_file();
    cJSON *item = cJSON_GetObjectItem(root, key);

    if (item)
    {
        cJSON_ReplaceItemInObject(root, key, cJSON_CreateString(value));
    }
    else
    {
        cJSON_AddItemToObject(root, key, cJSON_CreateString(value));
    }
    config_write_to_file(root);
    cJSON_Delete(root);

    free(key);
    free(value);
    return jerry_undefined();
}

// 设置布尔
static jerry_value_t js_config_set_boolean(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc)
{
    if (argc < 2 || !jerry_value_is_string(args[0]) || !jerry_value_is_boolean(args[1]))
    {
        return throw_error("Usage: config_set_boolean(key, bool)");
    }

    // key
    jerry_size_t key_len = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    char *key = malloc(key_len + 1);
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, (jerry_char_t *)key, key_len);
    key[key_len] = '\0';

    bool value = jerry_value_is_true(args[1]);

    // JSON 写入
    cJSON *root = config_load_from_file();
    if (!root)
    {
        return throw_error("Can't load config");
    }
    cJSON *item = cJSON_GetObjectItem(root, key);

    if (item)
    {
        cJSON_ReplaceItemInObject(root, key, cJSON_CreateBool(value));
    }
    else
    {
        cJSON_AddItemToObject(root, key, cJSON_CreateBool(value));
    }
    config_write_to_file(root);
    cJSON_Delete(root);

    free(key);
    return jerry_undefined();
}

// 设置数字
static jerry_value_t js_config_set_number(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc)
{
    if (argc < 2 || !jerry_value_is_string(args[0]) || !jerry_value_is_number(args[1]))
    {
        return throw_error("Usage: config_set_number(key, number)");
    }

    // key
    jerry_size_t key_len = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    char *key = malloc(key_len + 1);
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, (jerry_char_t *)key, key_len);
    key[key_len] = '\0';

    double value = jerry_value_as_number(args[1]);

    // JSON 写入
    cJSON *root = config_load_from_file();
    if (!root)
    {
        return throw_error("Can't load config");
    }
    cJSON *item = cJSON_GetObjectItem(root, key);

    if (item)
    {
        EOS_LOG_D("Replace item");
        cJSON_ReplaceItemInObject(root, key, cJSON_CreateNumber(value));
    }
    else
    {
        EOS_LOG_D("Create item");
        cJSON_AddItemToObject(root, key, cJSON_CreateNumber(value));
    }
    config_write_to_file(root);

    cJSON_Delete(root);

    free(key);
    return jerry_undefined();
}

// 获取字符串
static jerry_value_t js_config_get_str(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc)
{
    if (argc < 1 || !jerry_value_is_string(args[0]))
    {
        return throw_error("Usage: config_get_str(key)");
    }

    jerry_size_t key_len = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    char *key = malloc(key_len + 1);
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, (jerry_char_t *)key, key_len);
    key[key_len] = '\0';

    cJSON *root = config_load_from_file();
    if (!root)
    {
        return throw_error("Can't load config");
    }
    cJSON *item = cJSON_GetObjectItem(root, key);

    jerry_value_t ret = jerry_undefined();
    if (item && cJSON_IsString(item))
    {
        ret = jerry_string_sz(item->valuestring);
    }
    else
    {
        EOS_LOG_E("Can't get item");
    }

    cJSON_Delete(root);
    free(key);
    return ret;
}

// 获取布尔
static jerry_value_t js_config_get_boolean(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc)
{
    if (argc < 1 || !jerry_value_is_string(args[0]))
    {
        return throw_error("Usage: config_get_boolean(key)");
    }

    jerry_size_t key_len = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    char *key = malloc(key_len + 1);
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, (jerry_char_t *)key, key_len);
    key[key_len] = '\0';

    cJSON *root = config_load_from_file();
    if (!root)
    {
        return throw_error("Can't load config");
    }
    cJSON *item = cJSON_GetObjectItem(root, key);

    jerry_value_t ret = jerry_boolean(false);
    if (item && cJSON_IsBool(item))
    {
        ret = item->valueint ? jerry_boolean(true) : jerry_boolean(false);
    }
    else
    {
        EOS_LOG_E("Can't get item");
    }

    cJSON_Delete(root);
    free(key);
    return ret;
}

// 获取数字
static jerry_value_t js_config_get_number(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc)
{
    if (argc < 1 || !jerry_value_is_string(args[0]))
    {
        return throw_error("Usage: config_get_number(key)");
    }

    jerry_size_t key_len = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    char *key = malloc(key_len + 1);
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, (jerry_char_t *)key, key_len);
    key[key_len] = '\0';

    cJSON *root = config_load_from_file();
    if (!root)
    {
        return throw_error("Can't load config");
    }
    cJSON *item = cJSON_GetObjectItem(root, key);

    jerry_value_t ret = jerry_number(0);
    if (item && cJSON_IsNumber(item))
    {
        ret = jerry_number(item->valuedouble);
    }
    else
    {
        EOS_LOG_E("Can't get item");
    }

    cJSON_Delete(root);
    free(key);
    return ret;
}

// 返回时间对象给 JS
static jerry_value_t js_eos_time_get(const jerry_call_info_t *call_info_p,
                                     const jerry_value_t args[],
                                     const jerry_length_t argc)
{
    eos_datetime_t dt = eos_time_get();

    // 创建 JS 对象
    jerry_value_t obj = jerry_object();

    script_engine_set_prop_number(obj, "year", dt.year);
    script_engine_set_prop_number(obj, "month", dt.month);
    script_engine_set_prop_number(obj, "day", dt.day);
    script_engine_set_prop_number(obj, "hour", dt.hour);
    script_engine_set_prop_number(obj, "min", dt.min);
    script_engine_set_prop_number(obj, "sec", dt.sec);
    script_engine_set_prop_number(obj, "day_of_week", dt.day_of_week);

    return obj;
}

static jerry_value_t js_lv_tiny_ttf_create_file(const jerry_call_info_t *call_info_p,
                                                const jerry_value_t args[],
                                                const jerry_length_t argc)
{
    // 参数数量检查
    if (argc < 2)
    {
        return throw_error("Insufficient arguments");
    }

    // 解析参数: src (const char*)
    char *arg_src_str = NULL;
    const char *arg_src = NULL;
    if (!jerry_value_is_undefined(args[0]) && !jerry_value_is_null(args[0]))
    {
        if (!jerry_value_is_string(args[0]))
        {
            return throw_error("Argument 0 must be a string");
        }
        jerry_size_t arg_src_len = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
        arg_src_str = (char *)malloc(arg_src_len + 0);
        jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, (jerry_char_t *)arg_src_str, arg_src_len);
        arg_src_str[arg_src_len] = '\0';
        arg_src = arg_src_str;
    }

    // 解析参数: font_size (uint32_t)
    if (!jerry_value_is_number(args[1]))
    {
        if (arg_src_str) free(arg_src_str);
        return throw_error("Argument 1 must be a number");
    }
    uint32_t font_size = (uint32_t)jerry_value_as_number(args[1]);

    char font_path[PATH_MAX];
    if (script_pkg.id == NULL)
    {
        if (arg_src_str) free(arg_src_str);
        return throw_error("Script package info is NULL");
    }
    
    if (script_pkg.type == SCRIPT_TYPE_APPLICATION)
    {
        snprintf(font_path, sizeof(font_path), EOS_APP_INSTALLED_DIR "%s/assets/%s",
                 script_pkg.id, arg_src);
    }
    else if (script_pkg.type == SCRIPT_TYPE_WATCHFACE)
    {
        snprintf(font_path, sizeof(font_path), EOS_WATCHFACE_INSTALLED_DIR "%s/assets/%s",
                 script_pkg.id, arg_src);
    }
    else
    {
        if (arg_src_str) free(arg_src_str);
        return throw_error("Unknown script type");
    }
    
    if (!eos_is_file(font_path))
    {
        if (arg_src_str) free(arg_src_str);
        return throw_error("Font file not found");
    }
    
    EOS_LOG_D("Font Path: %s", font_path);

    // 调用底层函数创建字体
    lv_font_t *font = lv_tiny_ttf_create_file(font_path, font_size);
    
    // 释放临时字符串内存
    if (arg_src_str)
        free(arg_src_str);

    if (!font)
    {
        return throw_error("Failed to create font");
    }

    // 包装为LVGL字体对象返回
    jerry_value_t js_result = jerry_object();
    jerry_value_t ptr = jerry_number((double)(uintptr_t)font);
    jerry_value_t type = jerry_string_sz("lv_font");
    jerry_object_set(js_result, jerry_string_sz("__ptr"), ptr);
    jerry_object_set(js_result, jerry_string_sz("__type"), type);
    jerry_value_free(ptr);
    jerry_value_free(type);

    return js_result;
}

/********************************** 注册原生函数 **********************************/

/**
 * @brief 原生函数列表
 */
const script_engine_func_entry_t script_engine_native_funcs[] = {
    {.name = "print",
     .handler = js_print_handler},
    {.name = "delay",
     .handler = js_delay_handler},
    {.name = "nav_scr_create",
     .handler = js_nav_scr_create},
    {.name = "nav_back",
     .handler = js_nav_back},
    {.name = "lv_img_set_src",
     .handler = js_lv_img_set_src},
    {.name = "config_set_str",
     .handler = js_config_set_str},
    {.name = "config_set_boolean",
     .handler = js_config_set_boolean},
    {.name = "config_set_number",
     .handler = js_config_set_number},
    {.name = "config_get_str",
     .handler = js_config_get_str},
    {.name = "config_get_boolean",
     .handler = js_config_get_boolean},
    {.name = "config_get_number",
     .handler = js_config_get_number},
    {.name = "eos_time_get",
     .handler = js_eos_time_get},
    {.name = "lv_tiny_ttf_create_file",
     .handler = js_lv_tiny_ttf_create_file},
};

/**
 * @brief 将原生函数注册到 JerryScript 全局对象中
 */
void script_engine_register_natives()
{
    script_engine_register_functions(script_engine_native_funcs, sizeof(script_engine_native_funcs) / sizeof(script_engine_func_entry_t));
}
