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
#include "elena_os_base_item.h"
#include "elena_os_event.h"
#include "elena_os_test.h"
#include "elena_os_version.h"
#include "elena_os_port.h"
#include "elena_os_swipe_panel.h"
#include "elena_os_app.h"
#include "elena_os_watchface.h"
#include "elena_os_misc.h"
// Macros and Definitions
#define EOS_SYS_DEFAULT_LANG_STR "English"
#define EOS_SYS_DEFAULT_WATCHFACE_ID_STR "cn.sab1e.clock"
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

    char *file_content = eos_malloc(fsize + 1);
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
        eos_mem_free(file_content);
        return -EOS_ERR_FILE_ERROR;
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    eos_mem_free(file_content);
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

    char *file_content = eos_malloc(fsize + 1);
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
        eos_mem_free(file_content);
        return -EOS_ERR_FILE_ERROR;
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    eos_mem_free(file_content);
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

    char *file_content = eos_malloc(fsize + 1);
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
        eos_mem_free(file_content);
        return -EOS_ERR_FILE_ERROR;
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    eos_mem_free(file_content);
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
        if (!item) {
            EOS_LOG_D("Key '%s' not found in config, returning default", key);
        } else {
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
        return strdup(default_value);
    }

    // 检查配置文件是否存在
    if (!eos_is_file(EOS_SYS_CONFIG_FILE_PATH))
    {
        EOS_LOG_W("Config file does not exist, returning default value for key '%s'", key);
        return strdup(default_value);
    }

    // 读取配置文件内容
    int fd = open(EOS_SYS_CONFIG_FILE_PATH, O_RDONLY);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open config file for reading, errno=%d", errno);
        return strdup(default_value);
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char *file_content = malloc(fsize + 1);
    if (!file_content)
    {
        EOS_LOG_E("Memory allocation failed");
        close(fd);
        return strdup(default_value);
    }

    ssize_t read_size = read(fd, file_content, fsize);
    close(fd);
    if (read_size != fsize)
    {
        EOS_LOG_E("Failed to read config file, read_size=%zd, errno=%d", read_size, errno);
        free(file_content);
        return strdup(default_value);
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    free(file_content);
    if (!root)
    {
        EOS_LOG_E("Failed to parse JSON");
        return strdup(default_value);
    }

    // 获取字符串值
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (!item || !cJSON_IsString(item))
    {
        if (!item) {
            EOS_LOG_D("Key '%s' not found in config, returning default", key);
        } else {
            EOS_LOG_W("Value for key '%s' is not a string, returning default", key);
        }
        cJSON_Delete(root);
        return strdup(default_value);
    }

    char *result = strdup(item->valuestring);
    cJSON_Delete(root);

    if (!result)
    {
        EOS_LOG_E("Failed to duplicate string, returning default");
        return strdup(default_value);
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
        if (!item) {
            EOS_LOG_D("Key '%s' not found in config, returning default", key);
        } else {
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

    char *file_content = eos_malloc(fsize + 1);
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
        eos_mem_free(file_content);
        return -EOS_ERR_FILE_ERROR;
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    eos_mem_free(file_content);
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

static void _sys_app_list_btn_cb(lv_event_t *e)
{
    const char *app_id = (const char *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(app_id);

    // 创建新的页面用于绘制应用详情页
    lv_obj_t *scr = eos_nav_scr_create();
    lv_screen_load(scr);

    // 获取清单文件
    char manifest_path[PATH_MAX];
    snprintf(manifest_path, sizeof(manifest_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_MANIFEST_FILE_NAME,
             app_id);
    char *manifest_json = eos_read_file(manifest_path);
    if (!manifest_json)
    {
        EOS_LOG_E("Read manifest.json failed");
        return;
    }
    // 获取根节点
    cJSON *root = cJSON_Parse(manifest_json);
    eos_mem_free(manifest_json); // 解析完立即释放原始字符串
    if (!root)
    {
        EOS_LOG_E("parse error: %s\n", cJSON_GetErrorPtr());
        return;
    }

    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (!cJSON_IsString(name) || name->valuestring == NULL)
    {
        EOS_LOG_E("Get \"name\" failed");
        cJSON_Delete(root);
        return;
    }
    EOS_LOG_I("name = %s\n", name->valuestring);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, name->valuestring);
    lv_obj_center(label);
    cJSON_Delete(root);
}
/**
 * @brief 系统设置中的应用列表
 */
void eos_sys_app_list_create()
{
    // 创建新的页面用于绘制应用列表
    lv_obj_t *scr = eos_nav_scr_create();
    lv_screen_load(scr);

    lv_obj_t *app_list = lv_list_create(scr);
    lv_obj_set_size(app_list, lv_pct(100), lv_pct(100));

    size_t app_list_size = eos_app_list_size();
    for (size_t i = 0; i < app_list_size; i++)
    {
        char icon_path[PATH_MAX];
        snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME,
                 eos_app_list_get_id(i));
        if (!eos_is_file(icon_path))
        {
            memcpy(icon_path, EOS_IMG_APP, sizeof(EOS_IMG_APP));
        }
        EOS_LOG_D("Icon: %s", icon_path);

        // 获取清单文件
        char manifest_path[PATH_MAX];
        snprintf(manifest_path, sizeof(manifest_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_MANIFEST_FILE_NAME,
                 eos_app_list_get_id(i));
        char *manifest_json = eos_read_file(manifest_path);
        if (!manifest_json)
        {
            EOS_LOG_E("Read manifest.json failed");
            return;
        }
        // 获取根节点
        cJSON *root = cJSON_Parse(manifest_json);
        eos_mem_free(manifest_json); // 解析完立即释放原始字符串
        if (!root)
        {
            EOS_LOG_E("parse error: %s\n", cJSON_GetErrorPtr());
            return;
        }

        cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
        if (!cJSON_IsString(name) || name->valuestring == NULL)
        {
            EOS_LOG_E("Get \"name\" failed");
            cJSON_Delete(root);
            return;
        }
        EOS_LOG_I("name = %s\n", name->valuestring);

        // lv_obj_t *label = lv_label_create(scr);
        // lv_label_set_text(label, name->valuestring);
        // lv_obj_center(label);
        lv_obj_t *btn = eos_list_add_button(app_list, icon_path, name->valuestring);
        lv_obj_add_event_cb(btn, _sys_app_list_btn_cb, LV_EVENT_CLICKED, (void *)eos_app_list_get_id(i));
        cJSON_Delete(root);
    }
}

/**
 * @brief 系统设置页面
 */
void eos_sys_settings_create(void)
{
    lv_obj_t *scr = eos_nav_scr_create();
    lv_screen_load(scr);

    lv_obj_t *settings_list = lv_list_create(scr);
    lv_obj_set_size(settings_list, lv_pct(100), lv_pct(100));

    lv_obj_t *btn;
    lv_list_add_text(settings_list, "[ElenaOS Test List]");
    // 测试导航功能
    // btn = lv_list_add_button(settings_list, LV_SYMBOL_HOME, "Navigation");
    // lv_obj_add_event_cb(btn, _test_nav_cb_1, LV_EVENT_CLICKED, NULL);

}