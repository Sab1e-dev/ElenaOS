/**
 * @file elena_os_app.c
 * @brief 应用系统
 * @author Sab1e
 * @date 2025-08-21
 */

#include "elena_os_app.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include "elena_os_misc.h"
#include "elena_os_port.h"
#include "elena_os_log.h"
#include "elena_os_pkg_mgr.h"
#include "script_engine_core.h"
// Macros and Definitions
#define EOS_APP_LIST_DEFAULT_CAPACITY 1 // 列表默认容量大小
/**
 * @brief 应用列表结构体
 * 
 * 可变数组
 */
typedef struct
{
    char **data;        /**< 应用唯一ID */
    size_t size;        /**< 应用列表已存储的ID数量 */
    size_t capacity;    /**< 应用列表的容量 */
} eos_app_list_t;
static eos_app_list_t app_list;
static bool app_list_initialized = false;
// Variables

// Function Implementations

size_t eos_app_list_size(void)
{
    return app_list.size;
}

const char *eos_app_list_get_id(size_t index)
{
    if (index >= app_list.size)
    {
        EOS_LOG_E("Index out of bounds: %zu", index);
        return NULL;
    }
    return app_list.data[index];
}

bool eos_app_list_contains(const char *app_id)
{
    for (size_t i = 0; i < app_list.size; i++)
    {
        if (strcmp(app_list.data[i], app_id) == 0)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief 初始化应用列表
 */
void _eos_app_list_init(eos_app_list_t *list, size_t capacity)
{
    list->data = malloc(capacity * sizeof(char *));
    list->size = 0;
    list->capacity = capacity;
}

/**
 * @brief 向应用列表添加新的应用
 */
void _eos_app_list_add(eos_app_list_t *list, const char *id)
{
    if (list->size == list->capacity)
    {
        list->capacity *= 2;
        list->data = realloc(list->data, list->capacity * sizeof(char *));
    }
    list->data[list->size] = strdup(id); // 复制字符串
    list->size++;
}

/**
 * @brief 释放列表的数据
 */
void _eos_app_list_free(eos_app_list_t *list)
{
    for (size_t i = 0; i < list->size; i++)
    {
        free(list->data[i]);
    }
    free(list->data);
}

/**
 * @brief 从 Flash 获取已安装的应用
 */
eos_result_t _eos_app_list_get_installed(void)
{
    DIR *dir;
    struct dirent *entry;

    // 打开应用程序安装目录
    dir = opendir(EOS_APP_INSTALLED_DIR);
    if (!dir)
    {
        EOS_LOG_E("Failed to open app directory: %s", EOS_APP_INSTALLED_DIR);
        return EOS_FAILED;
    }

    // 遍历目录中的所有条目
    while ((entry = readdir(dir)) != NULL)
    {
        // 跳过 "." 和 ".." 目录
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // 构建完整路径
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), EOS_APP_INSTALLED_DIR "%s", entry->d_name);

        // 检查是否为目录
        if (eos_is_dir(full_path))
        {
            EOS_LOG_D("Found installed app: %s", entry->d_name);
            // 添加到应用程序列表
            _eos_app_list_add(&app_list, entry->d_name);
        }
    }

    closedir(dir);
    EOS_LOG_I("Loaded %zu installed apps", app_list.size);
    return EOS_OK;
}

/**
 * @brief 刷新应用列表
 */
eos_result_t _eos_app_list_refresh()
{
    memcpy(&app_list, 0, sizeof(app_list));
    _eos_app_list_init(&app_list, EOS_APP_LIST_DEFAULT_CAPACITY);
    if (_eos_app_list_get_installed() != EOS_OK)
    {
        EOS_LOG_E("Get installed app failed");
        return EOS_FAILED;
    }
    return EOS_OK;
}

eos_result_t eos_app_install(const char *eapk_path)
{
    EOS_CHECK_PTR_RETURN_VAL(eapk_path, EOS_ERR_VAR_NULL);
    // 获取软件包头
    eos_pkg_header_t header;
    if (eos_pkg_read_header(eapk_path, &header) != EOS_OK)
    {
        EOS_LOG_E("Read header failed: %s", eapk_path);
        return EOS_FAILED;
    }
    if (!eos_is_valid_filename(header.pkg_id))
    {
        EOS_LOG_E("Invalid package id");
        return EOS_FAILED;
    }
    // 拼接路径
    char path[PATH_MAX];
    snprintf(path, sizeof(path), EOS_APP_INSTALLED_DIR "%s", header.pkg_id);
    char data_path[PATH_MAX];
    snprintf(data_path, sizeof(data_path), EOS_APP_DATA_DIR "%s", header.pkg_id);
    EOS_LOG_D("APP_PATH: %s", path);
    // 检查应用是否存在
    if (eos_is_dir(path))
    {
        // 如果存在则删除
        eos_rm_recursive(path);
    }
    // 创建应用名称的文件夹
    if (mkdir(path, 0755) == 0)
    {
        EOS_LOG_I("Created dir: %s\n", path);
    }
    else
    {
        if (errno != EEXIST)
        {
            EOS_LOG_E("mkdir");
            return -EOS_ERR_FILE_ERROR;
        }
    }
    if (!eos_is_dir(data_path))
    {
        if (mkdir(data_path, 0755) == 0)
        {
            EOS_LOG_I("Created dir: %s\n", data_path);
        }
        else
        {
            if (errno != EEXIST)
            {
                EOS_LOG_E("mkdir");
                return -EOS_ERR_FILE_ERROR;
            }
        }
    }
    // 安装应用程序
    script_pkg_type_t type = SCRIPT_TYPE_APPLICATION;
    eos_result_t ret = eos_pkg_mgr_unpack(eapk_path, path, type);
    if (ret != EOS_OK)
    {
        EOS_LOG_E("App unpack failed. Code: %d", ret);
        eos_rm_recursive(path);
        return EOS_FAILED;
    }
    _eos_app_list_refresh();
    EOS_LOG_D("App installed successfully: %s", header.pkg_name);
    return EOS_OK;
}

eos_result_t eos_app_uninstall(const char *app_id)
{
    // 卸载应用程序
    char path[PATH_MAX];
    snprintf(path, sizeof(path), EOS_APP_INSTALLED_DIR "%s", app_id);
    char data_path[PATH_MAX];
    snprintf(data_path, sizeof(data_path), EOS_APP_DATA_DIR "%s", app_id);
    if (!eos_is_dir(path))
    {
        EOS_LOG_E("App does not esist: %s", app_id);
        return EOS_FAILED;
    }

    eos_result_t ret = eos_rm_recursive(path);

    if (ret != EOS_OK)
    {
        EOS_LOG_E("Uninstall failed, code: %d", ret);
        return EOS_FAILED;
    }

    // 清理应用数据
    if(eos_is_dir(data_path)){
        ret = eos_rm_recursive(path);
    }

    if (ret != EOS_OK)
    {
        EOS_LOG_E("Uninstall failed, code: %d", ret);
        return EOS_FAILED;
    }
    _eos_app_list_refresh();
    EOS_LOG_D("App uninstalled successfully: %s", app_id);
    return EOS_OK;
}

eos_result_t eos_app_init(void)
{
    // 初始化 从文件系统中读取应用列表
    _eos_app_list_refresh();
    return EOS_OK;
}
