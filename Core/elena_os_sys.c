/**
 * @file elena_os_sys.c
 * @brief 系统
 * @author Sab1e
 * @date 2025-08-21
 */

#include "elena_os_sys.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

// Macros and Definitions
#define EOS_SYS_DEFAULT_LANG_STR "English"
// Variables

// Function Implementations
ElenaOSResult_t _create_default_cfg_json(const char *path)
{
    // 创建 JSON 对象
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return -EOS_ERR_JSON_ERROR;
    }

    cJSON_AddStringToObject(root, "version", ELENA_OS_VERSION_FULL);
    cJSON_AddStringToObject(root, "language", EOS_SYS_DEFAULT_LANG_STR);

    // 转换为字符串
    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        cJSON_Delete(root);
        return -EOS_ERR_JSON_ERROR;
    }

    // 写入文件
    ElenaOSResult_t ret = eos_create_file_if_not_exist(path, json_str);
    
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
    eos_mkdir_if_not_exist(EOS_APP_PACKAGE_DIR, 0755);
    eos_mkdir_if_not_exist(EOS_APP_DATA_DIR, 0755);
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

ElenaOSResult_t eos_sys_add_config_item(const char *key, const char *value)
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

    char *file_content = malloc(fsize + 1);
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
        free(file_content);
        return -EOS_ERR_FILE_ERROR;
    }
    file_content[fsize] = '\0';

    // 解析JSON
    cJSON *root = cJSON_Parse(file_content);
    free(file_content);
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