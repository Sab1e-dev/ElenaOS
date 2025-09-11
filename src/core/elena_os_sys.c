/**
 * @file elena_os_sys.c
 * @brief 系统设置
 * @author Sab1e
 * @date 2025-08-21
 */

/**
 * TODO:
 * 应用列表详情页
 * 主题设置
 * 打开设置页面时会分配16KB内存，原因未知
 * 退出页面时再存储配置信息
 */

#include "elena_os_sys.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "lvgl.h"
#include "cJSON.h"
#include "elena_os_img.h"
#include "elena_os_msg_list.h"
#include "elena_os_lang.h"
#include "elena_os_log.h"
#include "elena_os_nav.h"
#include "elena_os_basic_widgets.h"
#include "elena_os_event.h"
#include "elena_os_test.h"
#include "elena_os_version.h"
#include "elena_os_port.h"
#include "elena_os_swipe_panel.h"
#include "elena_os_app.h"
#include "elena_os_watchface.h"
#include "elena_os_misc.h"
#include "elena_os_theme.h"
#include "elena_os_pkg_mgr.h"
// Macros and Definitions
#define EOS_SYS_DEFAULT_LANG_STR "English"
#define EOS_SYS_DEFAULT_WATCHFACE_ID_STR "cn.sab1e.clock"
#define EOS_SYS_DISPLAY_BRIGHTNESS_MIN 1 /**< 亮度为0即关闭屏幕 */
#define EOS_SYS_DISPLAY_BRIGHTNESS_MAX 100
// Variables

// Function Implementations

eos_result_t eos_sys_cfg_set_bool(const char *key, bool value)
{
    if (!key)
    {
        EOS_LOG_E("Invalid parameter: key is NULL");
        return -EOS_ERR_VAR_NULL;
    }

    // 检查配置文件是否存在
    if (!eos_is_file(EOS_SYS_CONFIG_FILE_PATH))
    {
        EOS_LOG_E("Config file does not exist");
        return -EOS_ERR_FILE_ERROR;
    }

    // 读取现有配置文件内容
    int fd = open(EOS_SYS_CONFIG_FILE_PATH, O_RDONLY);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for reading, errno=%d", errno);
        return -EOS_ERR_FILE_ERROR;
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char *file_content = eos_malloc_large(fsize + 1);
    if (!file_content)
    {
        EOS_LOG_E("Memory allocation failed");
        close(fd);
        return -EOS_ERR_MEM;
    }

    ssize_t read_size = read(fd, file_content, fsize);
    close(fd);
    if (read_size != fsize)
    {
        EOS_LOG_E("Failed to read config file, read_size=%zd, errno=%d", read_size, errno);
        eos_free_large(file_content);
        return -EOS_ERR_FILE_ERROR;
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    eos_free_large(file_content);
    if (!root)
    {
        EOS_LOG_E("Failed to parse JSON");
        return -EOS_ERR_JSON_ERROR;
    }

    // 更新或添加布尔值
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item)
    {
        cJSON_SetBoolValue(item, value);
    }
    else
    {
        cJSON_AddBoolToObject(root, key, value);
    }

    // 写回文件
    char *new_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!new_json)
    {
        EOS_LOG_E("Failed to generate JSON");
        return -EOS_ERR_JSON_ERROR;
    }

    fd = open(EOS_SYS_CONFIG_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for writing, errno=%d", errno);
        cJSON_free(new_json);
        return -EOS_ERR_FILE_ERROR;
    }

    ssize_t written = write(fd, new_json, strlen(new_json));
    cJSON_free(new_json);
    close(fd);

    if (written != (ssize_t)strlen(new_json))
    {
        EOS_LOG_E("Failed to write config file, written=%zd, errno=%d", written, errno);
        return -EOS_ERR_FILE_ERROR;
    }

    EOS_LOG_I("Successfully set config item: %s=%s", key, value ? "true" : "false");
    return EOS_OK;
}

eos_result_t eos_sys_cfg_set_string(const char *key, const char *value)
{
    if (!key || !value)
    {
        EOS_LOG_E("Invalid parameters: key or value is NULL");
        return -EOS_ERR_VAR_NULL;
    }

    // 检查配置文件是否存在
    if (!eos_is_file(EOS_SYS_CONFIG_FILE_PATH))
    {
        EOS_LOG_E("Config file does not exist");
        return -EOS_ERR_FILE_ERROR;
    }

    // 读取现有配置文件内容
    int fd = open(EOS_SYS_CONFIG_FILE_PATH, O_RDONLY);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for reading, errno=%d", errno);
        return -EOS_ERR_FILE_ERROR;
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char *file_content = eos_malloc_large(fsize + 1);
    if (!file_content)
    {
        EOS_LOG_E("Memory allocation failed");
        close(fd);
        return -EOS_ERR_MEM;
    }

    ssize_t read_size = read(fd, file_content, fsize);
    close(fd);
    if (read_size != fsize)
    {
        EOS_LOG_E("Failed to read config file, read_size=%zd, errno=%d", read_size, errno);
        eos_free_large(file_content);
        return -EOS_ERR_FILE_ERROR;
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    eos_free_large(file_content);
    if (!root)
    {
        EOS_LOG_E("Failed to parse JSON");
        return -EOS_ERR_JSON_ERROR;
    }

    // 更新或添加字符串值
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item)
    {
        cJSON_SetValuestring(item, value);
    }
    else
    {
        cJSON_AddStringToObject(root, key, value);
    }

    // 写回文件
    char *new_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!new_json)
    {
        EOS_LOG_E("Failed to generate JSON");
        return -EOS_ERR_JSON_ERROR;
    }

    fd = open(EOS_SYS_CONFIG_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for writing, errno=%d", errno);
        cJSON_free(new_json);
        return -EOS_ERR_FILE_ERROR;
    }

    ssize_t written = write(fd, new_json, strlen(new_json));
    cJSON_free(new_json);
    close(fd);

    if (written != (ssize_t)strlen(new_json))
    {
        EOS_LOG_E("Failed to write config file, written=%zd, errno=%d", written, errno);
        return -EOS_ERR_FILE_ERROR;
    }

    EOS_LOG_I("Successfully set config item: %s=%s", key, value);
    return EOS_OK;
}

eos_result_t eos_sys_cfg_set_number(const char *key, double value)
{
    if (!key)
    {
        EOS_LOG_E("Invalid parameter: key is NULL");
        return -EOS_ERR_VAR_NULL;
    }

    // 检查配置文件是否存在
    if (!eos_is_file(EOS_SYS_CONFIG_FILE_PATH))
    {
        EOS_LOG_E("Config file does not exist");
        return -EOS_ERR_FILE_ERROR;
    }

    // 读取现有配置文件内容
    int fd = open(EOS_SYS_CONFIG_FILE_PATH, O_RDONLY);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for reading, errno=%d", errno);
        return -EOS_ERR_FILE_ERROR;
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char *file_content = eos_malloc_large(fsize + 1);
    if (!file_content)
    {
        EOS_LOG_E("Memory allocation failed");
        close(fd);
        return -EOS_ERR_MEM;
    }

    ssize_t read_size = read(fd, file_content, fsize);
    close(fd);
    if (read_size != fsize)
    {
        EOS_LOG_E("Failed to read config file, read_size=%zd, errno=%d", read_size, errno);
        eos_free_large(file_content);
        return -EOS_ERR_FILE_ERROR;
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    eos_free_large(file_content);
    if (!root)
    {
        EOS_LOG_E("Failed to parse JSON");
        return -EOS_ERR_JSON_ERROR;
    }

    // 更新或添加数字值
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item)
    {
        cJSON_SetNumberValue(item, value);
    }
    else
    {
        cJSON_AddNumberToObject(root, key, value);
    }

    // 写回文件
    char *new_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!new_json)
    {
        EOS_LOG_E("Failed to generate JSON");
        return -EOS_ERR_JSON_ERROR;
    }

    fd = open(EOS_SYS_CONFIG_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for writing, errno=%d", errno);
        cJSON_free(new_json);
        return -EOS_ERR_FILE_ERROR;
    }

    ssize_t written = write(fd, new_json, strlen(new_json));
    cJSON_free(new_json);
    close(fd);

    if (written != (ssize_t)strlen(new_json))
    {
        EOS_LOG_E("Failed to write config file, written=%zd, errno=%d", written, errno);
        return -EOS_ERR_FILE_ERROR;
    }

    EOS_LOG_I("Successfully set config item: %s=%f", key, value);
    return EOS_OK;
}

bool eos_sys_cfg_get_bool(const char *key, bool default_value)
{
    if (!key)
    {
        EOS_LOG_E("Invalid parameter: key is NULL");
        return default_value;
    }

    // 检查配置文件是否存在
    if (!eos_is_file(EOS_SYS_CONFIG_FILE_PATH))
    {
        EOS_LOG_W("Config file does not exist, returning default value for key '%s'", key);
        return default_value;
    }

    // 读取配置文件内容
    int fd = open(EOS_SYS_CONFIG_FILE_PATH, O_RDONLY);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for reading, errno=%d", errno);
        return default_value;
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char *file_content = malloc(fsize + 1);
    if (!file_content)
    {
        EOS_LOG_E("Memory allocation failed");
        close(fd);
        return default_value;
    }

    ssize_t read_size = read(fd, file_content, fsize);
    close(fd);
    if (read_size != fsize)
    {
        EOS_LOG_E("Failed to read config file, read_size=%zd, errno=%d", read_size, errno);
        free(file_content);
        return default_value;
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    free(file_content);
    if (!root)
    {
        EOS_LOG_E("Failed to parse JSON");
        return default_value;
    }

    // 获取布尔值
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (!item || !cJSON_IsBool(item))
    {
        if (!item)
        {
            EOS_LOG_D("Key '%s' not found in config, returning default", key);
        }
        else
        {
            EOS_LOG_W("Value for key '%s' is not a boolean, returning default", key);
        }
        cJSON_Delete(root);
        return default_value;
    }

    bool result = cJSON_IsTrue(item);
    cJSON_Delete(root);

    EOS_LOG_D("Successfully got boolean config item: %s=%s", key, result ? "true" : "false");
    return result;
}

char *eos_sys_cfg_get_string(const char *key, const char *default_value)
{
    if (!key)
    {
        EOS_LOG_E("Invalid parameter: key is NULL");
        return eos_strdup(default_value);
    }

    // 检查配置文件是否存在
    if (!eos_is_file(EOS_SYS_CONFIG_FILE_PATH))
    {
        EOS_LOG_W("Config file does not exist, returning default value for key '%s'", key);
        return eos_strdup(default_value);
    }

    // 读取配置文件内容
    int fd = open(EOS_SYS_CONFIG_FILE_PATH, O_RDONLY);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for reading, errno=%d", errno);
        return eos_strdup(default_value);
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char *file_content = malloc(fsize + 1);
    if (!file_content)
    {
        EOS_LOG_E("Memory allocation failed");
        close(fd);
        return eos_strdup(default_value);
    }

    ssize_t read_size = read(fd, file_content, fsize);
    close(fd);
    if (read_size != fsize)
    {
        EOS_LOG_E("Failed to read config file, read_size=%zd, errno=%d", read_size, errno);
        free(file_content);
        return eos_strdup(default_value);
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    free(file_content);
    if (!root)
    {
        EOS_LOG_E("Failed to parse JSON");
        return eos_strdup(default_value);
    }

    // 获取字符串值
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (!item || !cJSON_IsString(item))
    {
        if (!item)
        {
            EOS_LOG_D("Key '%s' not found in config, returning default", key);
        }
        else
        {
            EOS_LOG_W("Value for key '%s' is not a string, returning default", key);
        }
        cJSON_Delete(root);
        return eos_strdup(default_value);
    }

    char *result = eos_strdup(item->valuestring);
    cJSON_Delete(root);

    if (!result)
    {
        EOS_LOG_E("Failed to duplicate string, returning default");
        return eos_strdup(default_value);
    }

    EOS_LOG_D("Successfully got string config item: %s=%s", key, result);
    return result;
}

double eos_sys_cfg_get_number(const char *key, double default_value)
{
    if (!key)
    {
        EOS_LOG_E("Invalid parameter: key is NULL");
        return default_value;
    }

    // 检查配置文件是否存在
    if (!eos_is_file(EOS_SYS_CONFIG_FILE_PATH))
    {
        EOS_LOG_W("Config file does not exist, returning default value for key '%s'", key);
        return default_value;
    }

    // 读取配置文件内容
    int fd = open(EOS_SYS_CONFIG_FILE_PATH, O_RDONLY);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for reading, errno=%d", errno);
        return default_value;
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char *file_content = malloc(fsize + 1);
    if (!file_content)
    {
        EOS_LOG_E("Memory allocation failed");
        close(fd);
        return default_value;
    }

    ssize_t read_size = read(fd, file_content, fsize);
    close(fd);
    if (read_size != fsize)
    {
        EOS_LOG_E("Failed to read config file, read_size=%zd, errno=%d", read_size, errno);
        free(file_content);
        return default_value;
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    free(file_content);
    if (!root)
    {
        EOS_LOG_E("Failed to parse JSON");
        return default_value;
    }

    // 获取数字值
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (!item || !cJSON_IsNumber(item))
    {
        if (!item)
        {
            EOS_LOG_D("Key '%s' not found in config, returning default", key);
        }
        else
        {
            EOS_LOG_W("Value for key '%s' is not a number, returning default", key);
        }
        cJSON_Delete(root);
        return default_value;
    }

    double result = item->valuedouble;
    cJSON_Delete(root);

    EOS_LOG_D("Successfully got number config item: %s=%f", key, result);
    return result;
}

eos_result_t _create_default_cfg_json(const char *path)
{
    // 创建 JSON 对象
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return -EOS_ERR_JSON_ERROR;
    }

    cJSON_AddStringToObject(root, EOS_SYS_CFG_KEY_VERSION, ELENA_OS_VERSION_FULL);
    cJSON_AddStringToObject(root, EOS_SYS_CFG_KEY_LANGUAGE, EOS_SYS_DEFAULT_LANG_STR);
    cJSON_AddStringToObject(root, EOS_SYS_CFG_KEY_WATCHFACE_ID, EOS_SYS_DEFAULT_WATCHFACE_ID_STR);
    // 转换为字符串
    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str)
    {
        cJSON_Delete(root);
        return -EOS_ERR_JSON_ERROR;
    }

    // 写入文件
    eos_result_t ret = eos_create_file_if_not_exist(path, json_str);

    // 释放内存
    cJSON_free(json_str);
    cJSON_Delete(root);

    return ret;
}

void eos_sys_init()
{
    // 判断系统文件是否存在
    eos_mkdir_if_not_exist(EOS_SYS_DIR, 0755);
    eos_mkdir_if_not_exist(EOS_SYS_CONFIG_DIR, 0755);

    eos_mkdir_if_not_exist(EOS_APP_DIR, 0755);
    eos_mkdir_if_not_exist(EOS_APP_INSTALLED_DIR, 0755);
    eos_mkdir_if_not_exist(EOS_APP_DATA_DIR, 0755);

    eos_mkdir_if_not_exist(EOS_WATCHFACE_DIR, 0755);
    eos_mkdir_if_not_exist(EOS_WATCHFACE_INSTALLED_DIR, 0755);
    eos_mkdir_if_not_exist(EOS_WATCHFACE_DATA_DIR, 0755);

    eos_mkdir_if_not_exist(EOS_SYS_RES_DIR, 0755);
    eos_mkdir_if_not_exist(EOS_SYS_RES_IMG_DIR, 0755);
    // 如果系统文件不存在则创建
    if (!eos_is_file(EOS_SYS_CONFIG_FILE_PATH))
    {
        _create_default_cfg_json(EOS_SYS_CONFIG_FILE_PATH);
    }

    /************************** 加载系统设置 **************************/
    // 蓝牙设置
    if (eos_sys_cfg_get_bool(EOS_SYS_CFG_KEY_BLUETOOTH, false))
    {
        eos_bluetooth_enable();
    }
    // 显示设置
    uint8_t brightness = eos_sys_cfg_get_number(EOS_SYS_CFG_KEY_DISPLAY_BRIGHTNESS, 50);
    if (brightness < 1 || brightness > 100)
        brightness = 50;
    eos_display_set_brightness(brightness);
}

/**
 * @brief 系统恢复出厂设置
 * @warning 未测试
 */
void eos_sys_factory_reset()
{
    // int ret = rmdir(EOS_SYS_DIR);
    // if (ret != 0)
    // {
    //     EOS_LOG_E("Can't delete " EOS_SYS_DIR);
    //     return;
    // }
    // eos_cpu_reset();
}

eos_result_t eos_sys_add_config_item(const char *key, const char *value)
{
    if (!key || !value)
    {
        EOS_LOG_E("Invalid parameters: key or value is NULL");
        return -EOS_ERR_VAR_NULL;
    }

    // 检查配置文件是否存在
    if (!eos_is_file(EOS_SYS_CONFIG_FILE_PATH))
    {
        EOS_LOG_E("Config file does not exist");
        return -EOS_ERR_FILE_ERROR;
    }

    // 读取现有配置文件内容
    int fd = open(EOS_SYS_CONFIG_FILE_PATH, O_RDONLY);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for reading, errno=%d", errno);
        return -EOS_ERR_FILE_ERROR;
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char *file_content = eos_malloc_large(fsize + 1);
    if (!file_content)
    {
        EOS_LOG_E("Memory allocation failed");
        close(fd);
        return -EOS_ERR_MEM;
    }

    ssize_t read_size = read(fd, file_content, fsize);
    close(fd);
    if (read_size != fsize)
    {
        EOS_LOG_E("Failed to read config file, read_size=%zd, errno=%d", read_size, errno);
        eos_free_large(file_content);
        return -EOS_ERR_FILE_ERROR;
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    eos_free_large(file_content);
    if (!root)
    {
        EOS_LOG_E("Failed to parse JSON");
        return -EOS_ERR_JSON_ERROR;
    }

    // 检查键是否已存在
    if (cJSON_HasObjectItem(root, key))
    {
        EOS_LOG_W("Key '%s' already exists in config", key);
        cJSON_Delete(root);
        return -EOS_ERR_JSON_ERROR;
    }

    // 添加新项
    cJSON_AddStringToObject(root, key, value);

    // 写回文件
    char *new_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!new_json)
    {
        EOS_LOG_E("Failed to generate JSON");
        return -EOS_ERR_JSON_ERROR;
    }

    fd = open(EOS_SYS_CONFIG_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for writing, errno=%d", errno);
        cJSON_free(new_json);
        return -EOS_ERR_FILE_ERROR;
    }

    ssize_t written = write(fd, new_json, strlen(new_json));
    cJSON_free(new_json);
    close(fd);

    if (written != (ssize_t)strlen(new_json))
    {
        EOS_LOG_E("Failed to write config file, written=%zd, errno=%d", written, errno);
        return -EOS_ERR_FILE_ERROR;
    }

    EOS_LOG_I("Successfully added new config item: %s=%s", key, value);
    return EOS_OK;
}

/************************** 蓝牙 **************************/
static void _bluetooth_enable_switch_cb(lv_event_t *e)
{
    lv_obj_t *bt_sw = lv_event_get_target(e);
    EOS_CHECK_PTR_RETURN(bt_sw);
    if (lv_obj_has_state(bt_sw, LV_STATE_CHECKED))
    {
        eos_bluetooth_enable();
        eos_sys_cfg_set_bool(EOS_SYS_CFG_KEY_BLUETOOTH, true);
    }
    else
    {
        eos_bluetooth_disable();
        eos_sys_cfg_set_bool(EOS_SYS_CFG_KEY_BLUETOOTH, false);
    }
}

static void _sys_screen_bluetooth(lv_event_t *e)
{
    lv_obj_t *scr = eos_nav_scr_create();
    eos_screen_bind_header(scr, current_lang[STR_ID_SETTINGS_BLUETOOTH]);
    lv_screen_load(scr);

    lv_obj_t *list = lv_list_create(scr);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(list, 20, 0);
    lv_obj_set_style_pad_ver(list, 0, 0);
    lv_obj_center(list);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    // 占位符
    eos_list_add_placeholder(list, 110);
    lv_obj_t *bt_sw = eos_list_add_switch(list, current_lang[STR_ID_SETTINGS_BLUETOOTH_ENABLE]);
    lv_obj_set_state(bt_sw, LV_STATE_CHECKED, eos_sys_cfg_get_bool(EOS_SYS_CFG_KEY_BLUETOOTH, false));
    lv_obj_add_event_cb(bt_sw, _bluetooth_enable_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
}
/************************** 显示设置 **************************/
static void _brightness_slider_value_changed_cb(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    eos_display_set_brightness(lv_slider_get_value(sl));
}

static void _brightness_slider_released_cb(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(sl);
    eos_display_set_brightness(val);
    eos_sys_cfg_set_number(EOS_SYS_CFG_KEY_DISPLAY_BRIGHTNESS, val);
}

static void _list_slider_minus_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(slider);
    int32_t min = lv_slider_get_min_value(slider);
    int32_t val = lv_slider_get_value(slider);
    if (val == min)
        return;
    val -= 5;
    lv_slider_set_value(slider, val, LV_ANIM_ON);
    eos_sys_cfg_set_number(EOS_SYS_CFG_KEY_DISPLAY_BRIGHTNESS, val);
    eos_display_set_brightness(val);
}

static void _list_slider_plus_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(slider);
    int32_t max = lv_slider_get_max_value(slider);
    int32_t val = lv_slider_get_value(slider);
    if (val == max)
        return;
    val += 5;
    lv_slider_set_value(slider, val, LV_ANIM_ON);
    eos_sys_cfg_set_number(EOS_SYS_CFG_KEY_DISPLAY_BRIGHTNESS, val);
    eos_display_set_brightness(val);
}

static void _sys_screen_display(lv_event_t *e)
{
    lv_obj_t *scr = eos_nav_scr_create();
    eos_screen_bind_header(scr, current_lang[STR_ID_SETTINGS_DISPLAY]);
    lv_screen_load(scr);

    lv_obj_t *list = lv_list_create(scr);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_center(list);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    // 占位符
    eos_list_add_placeholder(list, 110);

    eos_list_slider_t *brightness_slider = eos_list_add_slider(list, current_lang[STR_ID_SETTINGS_DISPLAY_BRIGHTNESS]);
    lv_slider_set_value(brightness_slider->slider, eos_sys_cfg_get_number(EOS_SYS_CFG_KEY_DISPLAY_BRIGHTNESS, 50), LV_ANIM_ON);
    lv_slider_set_range(brightness_slider->slider, EOS_SYS_DISPLAY_BRIGHTNESS_MIN, EOS_SYS_DISPLAY_BRIGHTNESS_MAX);
    lv_obj_add_event_cb(brightness_slider->slider, _brightness_slider_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(brightness_slider->slider, _brightness_slider_released_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(brightness_slider->minus_btn, _list_slider_minus_cb, LV_EVENT_CLICKED, brightness_slider->slider);
    lv_obj_add_event_cb(brightness_slider->plus_btn, _list_slider_plus_cb, LV_EVENT_CLICKED, brightness_slider->slider);
}
/************************** 通知 **************************/
static void _sys_screen_notification(lv_event_t *e)
{
    lv_obj_t *scr = eos_nav_scr_create();
    eos_screen_bind_header(scr, current_lang[STR_ID_SETTINGS_DISPLAY]);
    lv_screen_load(scr);
}
/************************** 应用列表 **************************/

/**
 * @brief 卸载按钮回调
 */
static void _uninstall_btn_cb(lv_event_t *e)
{
    const char *app_id = (const char *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(app_id);
    eos_app_uninstall(app_id);
    eos_nav_back_clean();
}

/**
 * @brief 应用列表回调，打开应用详情
 * @param e
 */
static void _sys_app_list_btn_cb(lv_event_t *e)
{
    const char *app_id = (const char *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(app_id);

    // 获取清单文件
    char manifest_path[PATH_MAX];
    snprintf(manifest_path, sizeof(manifest_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_MANIFEST_FILE_NAME,
             app_id);
    script_pkg_t pkg = {0};
    if (script_engine_get_manifest(manifest_path, &pkg) != SE_OK)
    {
        EOS_LOG_E("Read manifest failed: %s", manifest_path);
        return;
    }
    EOS_LOG_D("App Info:\n"
              "id=%s | name=%s | version=%s |\n"
              "author:%s | description:%s",
              pkg.id, pkg.name, pkg.version,
              pkg.version, pkg.description);
    char script_path[PATH_MAX];
    snprintf(script_path, sizeof(script_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_SCRIPT_ENTRY_FILE_NAME,
             app_id);
    if (!eos_is_file(script_path))
    {
        EOS_LOG_E("Can't find script: %s", script_path);
        return;
    }

    // 创建新的页面用于绘制应用详情页
    lv_obj_t *scr = eos_nav_scr_create();
    eos_screen_bind_header(scr, pkg.name);
    lv_screen_load(scr);

    lv_obj_t *list = lv_list_create(scr);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(list, 20, 0);
    lv_obj_set_style_pad_ver(list, 0, 0);
    lv_obj_center(list);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    eos_list_add_placeholder(list, 110);

    lv_obj_t *container = eos_list_add_container(list);
    lv_obj_set_size(container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    char icon_path[PATH_MAX];
    snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME,
             app_id);
    if (!eos_is_file(icon_path))
    {
        memcpy(icon_path, EOS_IMG_APP, sizeof(EOS_IMG_APP));
    }
    eos_row_create(container, NULL, pkg.name, icon_path, 128, 128);

    eos_row_create(container, current_lang[STR_ID_SETTINGS_APPS_APPID], app_id, NULL, 0, 0);
    eos_row_create(container, current_lang[STR_ID_SETTINGS_APPS_AUTHOR], pkg.author, NULL, 0, 0);
    eos_row_create(container, current_lang[STR_ID_SETTINGS_APPS_VERSION], pkg.version, NULL, 0, 0);

    if (strcmp(pkg.description, "") != 0)
    {
        lv_obj_t *inner_container = eos_list_add_title_container(list, current_lang[STR_ID_SETTINGS_APPS_DESCRIPTON]);
        lv_obj_set_size(inner_container, lv_pct(100), LV_SIZE_CONTENT);

        lv_obj_t *desc_label = lv_label_create(inner_container);
        lv_label_set_text(desc_label, pkg.description);
        lv_obj_set_width(desc_label, lv_pct(100));
        lv_label_set_long_mode(desc_label, LV_LABEL_LONG_WRAP);
    }

    lv_obj_t *uninstall_btn = lv_button_create(list);
    lv_obj_set_size(uninstall_btn, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(uninstall_btn, EOS_THEME_DANGEROS_COLOR, 0);

    lv_obj_t *uninstall_btn_label = lv_label_create(uninstall_btn);
    lv_label_set_text(uninstall_btn_label, current_lang[STR_ID_SETTINGS_APPS_UINSTALL]);
    lv_obj_add_event_cb(uninstall_btn, _uninstall_btn_cb, LV_EVENT_CLICKED, (void *)app_id);
    lv_obj_center(uninstall_btn_label);
}

static void _app_btn_create(lv_obj_t *parent, const char *app_id)
{
    char icon_path[PATH_MAX];
    snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME,
             app_id);
    if (!eos_is_file(icon_path))
    {
        memcpy(icon_path, EOS_IMG_APP, sizeof(EOS_IMG_APP));
    }
    EOS_LOG_D("Icon: %s", icon_path);

    // 获取清单文件
    char manifest_path[PATH_MAX];
    snprintf(manifest_path, sizeof(manifest_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_MANIFEST_FILE_NAME,
             app_id);
    script_pkg_t pkg = {0};
    if (script_engine_get_manifest(manifest_path, &pkg) != SE_OK)
    {
        EOS_LOG_E("Read manifest failed: %s", manifest_path);
        return;
    }

    EOS_LOG_I("name = %s\n", pkg.name);

    lv_obj_t *btn = eos_list_add_button(parent, icon_path, pkg.name);
    lv_obj_set_size(btn, lv_pct(100), EOS_LIST_CONTAINER_HEIGHT);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(btn, LV_DIR_NONE); // 禁止滚动
    lv_obj_set_style_bg_color(btn, EOS_THEME_SECONDARY_COLOR, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 18, 0);
    lv_obj_set_style_margin_bottom(btn, 20, 0);
    lv_obj_set_style_align(btn, LV_ALIGN_CENTER, 0);
    lv_obj_set_style_radius(btn, EOS_LIST_OBJ_RADIUS, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW); // 水平排布
    lv_obj_set_flex_align(btn,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(btn, _sys_app_list_btn_cb, LV_EVENT_CLICKED, (void *)app_id);
    eos_app_obj_auto_delete(btn, app_id);
}

static void _app_installed_cb(lv_event_t *e)
{
    lv_obj_t *parent = lv_event_get_target(e);
    const char *installed_app_id = lv_event_get_param(e);
    EOS_CHECK_PTR_RETURN(parent && installed_app_id);
    _app_btn_create(parent, installed_app_id);
}

/**
 * @brief 系统设置中的应用列表
 */
static void _sys_screen_apps(lv_event_t *e)
{
    // 创建新的页面用于绘制应用列表
    lv_obj_t *scr = eos_nav_scr_create();
    eos_screen_bind_header(scr, current_lang[STR_ID_SETTINGS_APPS]);
    lv_screen_load(scr);

    lv_obj_t *app_list = lv_list_create(scr);
    lv_obj_set_size(app_list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(app_list, 20, 0);
    lv_obj_set_style_pad_ver(app_list, 0, 0);
    lv_obj_center(app_list);
    lv_obj_set_scrollbar_mode(app_list, LV_SCROLLBAR_MODE_OFF);
    eos_event_add_cb(app_list, _app_installed_cb, eos_event_get_code(EOS_EVENT_APP_INSTALLED), NULL);
    eos_list_add_placeholder(app_list, 110);

    size_t app_list_size = eos_app_list_size();
    for (size_t i = 0; i < app_list_size; i++)
    {
        _app_btn_create(app_list, eos_app_list_get_id(i));
    }
}
/************************** 系统设置 **************************/
void eos_sys_settings_create(void)
{
    lv_obj_t *scr = eos_nav_scr_create();
    eos_screen_bind_header(scr, current_lang[STR_ID_SETTINGS]);
    lv_screen_load(scr);

    lv_obj_t *settings_list = lv_list_create(scr);
    lv_obj_set_size(settings_list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(settings_list, 0, 0);
    lv_obj_center(settings_list);
    lv_obj_set_scrollbar_mode(settings_list, LV_SCROLLBAR_MODE_OFF);

    // 占位符
    eos_list_add_placeholder(settings_list, 110);

    lv_obj_t *btn;
    // 蓝牙设置
    btn = eos_list_add_circle_icon_button(settings_list, lv_color_hex(0x3988ff), LV_SYMBOL_BLUETOOTH, current_lang[STR_ID_SETTINGS_BLUETOOTH]);
    lv_obj_add_event_cb(btn, _sys_screen_bluetooth, LV_EVENT_CLICKED, NULL);
    // 显示设置
    btn = eos_list_add_circle_icon_button(settings_list, lv_color_hex(0xffbb39), LV_SYMBOL_IMAGE, current_lang[STR_ID_SETTINGS_DISPLAY]);
    lv_obj_add_event_cb(btn, _sys_screen_display, LV_EVENT_CLICKED, NULL);
    // 通知设置
    btn = eos_list_add_circle_icon_button(settings_list, lv_color_hex(0xff3939), LV_SYMBOL_BELL, current_lang[STR_ID_SETTINGS_NOTIFICATION]);
    lv_obj_add_event_cb(btn, _sys_screen_notification, LV_EVENT_CLICKED, NULL);
    // 应用列表
    btn = eos_list_add_circle_icon_button(settings_list, lv_color_hex(0x00b8a9), LV_SYMBOL_LIST, current_lang[STR_ID_SETTINGS_APPS]);
    lv_obj_add_event_cb(btn, _sys_screen_apps, LV_EVENT_CLICKED, NULL);

    eos_list_add_placeholder(settings_list, 50);
}