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
#include "elena_os_event.h"
#include "script_engine_core.h"
#include "cJSON.h"
// Macros and Definitions
#define EOS_APP_LIST_DEFAULT_CAPACITY 1 // 列表默认容量大小
/**
 * @brief 应用列表结构体
 *
 * 可变数组
 */
typedef struct
{
    char **data;     /**< 应用唯一ID */
    size_t size;     /**< 应用列表已存储的ID数量 */
    size_t capacity; /**< 应用列表的容量 */
} eos_app_list_t;
static eos_app_list_t app_list;
static bool app_list_initialized = false;
// Variables

// Function Implementations

// 添加排序相关的函数声明
static eos_result_t _eos_app_order_save(void);
static eos_result_t _eos_app_order_load(void);
static eos_result_t _eos_app_order_add(const char *app_id);
static eos_result_t _eos_app_order_remove(const char *app_id);

// 在变量定义部分添加应用顺序列表
static cJSON *app_order_json = NULL;

// 添加应用顺序保存函数
static eos_result_t _eos_app_order_save(void)
{
    if (!app_order_json)
    {
        return EOS_FAILED;
    }

    char *json_str = cJSON_Print(app_order_json);
    if (!json_str)
    {
        return EOS_FAILED;
    }

    FILE *fp = fopen(EOS_APP_LIST_APP_ORDER_PATH, "w");
    if (!fp)
    {
        free(json_str);
        return EOS_FAILED;
    }

    fputs(json_str, fp);
    fclose(fp);
    free(json_str);

    return EOS_OK;
}

// 添加应用顺序加载函数
static eos_result_t _eos_app_order_load(void)
{
    if (app_order_json)
    {
        cJSON_Delete(app_order_json);
        app_order_json = NULL;
    }

    // 检查文件是否存在
    if (!eos_is_file(EOS_APP_LIST_APP_ORDER_PATH))
    {
        // 创建默认的JSON结构
        app_order_json = cJSON_CreateArray();
        // 添加系统设置应用
        cJSON_AddItemToArray(app_order_json, cJSON_CreateString("sys.settings"));
        return _eos_app_order_save();
    }

    char *json_str = eos_read_file(EOS_APP_LIST_APP_ORDER_PATH);
    if (!json_str)
    {
        return EOS_FAILED;
    }

    app_order_json = cJSON_Parse(json_str);
    eos_free_large(json_str);

    if (!app_order_json)
    {
        // 解析失败，创建新的JSON
        app_order_json = cJSON_CreateArray();
        // 添加系统设置应用
        cJSON_AddItemToArray(app_order_json, cJSON_CreateString("sys.settings"));
        return _eos_app_order_save();
    }

    // 确保系统设置应用存在
    bool has_settings = false;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, app_order_json)
    {
        if (cJSON_IsString(item) && strcmp(item->valuestring, "sys.settings") == 0)
        {
            has_settings = true;
            break;
        }
    }

    if (!has_settings)
    {
        cJSON_AddItemToArray(app_order_json, cJSON_CreateString("sys.settings"));
        _eos_app_order_save();
    }

    return EOS_OK;
}

// 添加应用到顺序列表
static eos_result_t _eos_app_order_add(const char *app_id)
{
    if (!app_order_json || !app_id)
    {
        return EOS_FAILED;
    }

    // 检查是否已存在
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, app_order_json)
    {
        if (cJSON_IsString(item) && strcmp(item->valuestring, app_id) == 0)
        {
            return EOS_OK; // 已存在
        }
    }

    // 添加到数组末尾
    cJSON_AddItemToArray(app_order_json, cJSON_CreateString(app_id));
    return _eos_app_order_save();
}

// 从顺序列表中移除应用
static eos_result_t _eos_app_order_remove(const char *app_id)
{
    if (!app_order_json || !app_id) {
        return EOS_FAILED;
    }
    
    // 系统设置应用不能被移除
    if (strcmp(app_id, "sys.settings") == 0) {
        return EOS_OK;
    }
    
    // 查找应用在数组中的位置
    int index = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, app_order_json) {
        if (cJSON_IsString(item) && strcmp(item->valuestring, app_id) == 0) {
            // 使用索引删除元素
            cJSON_DeleteItemFromArray(app_order_json, index);
            return _eos_app_order_save();
        }
        index++;
    }
    
    return EOS_OK; // 未找到也算成功
}

// 移动应用到指定位置
eos_result_t eos_app_order_move(const char *app_id, size_t new_index)
{
    if (!app_order_json || !app_id)
    {
        EOS_LOG_E("input NULL");
        return EOS_FAILED;
    }

    // 获取当前数组大小
    size_t array_size = cJSON_GetArraySize(app_order_json);
    if (new_index >= array_size)
    {
        EOS_LOG_E("Out of index");
        return EOS_FAILED; // 索引超出范围
    }

    // 查找应用在数组中的当前位置
    int current_index = -1;
    cJSON *item = NULL;
    for (int i = 0; i < array_size; i++)
    {
        item = cJSON_GetArrayItem(app_order_json, i);
        if (cJSON_IsString(item) && strcmp(item->valuestring, app_id) == 0)
        {
            current_index = i;
            break;
        }
    }

    if (current_index == -1)
    {
        EOS_LOG_E("App not found");
        return EOS_FAILED; // 未找到应用
    }

    // 如果已经在指定位置，直接返回
    if (current_index == new_index)
    {
        EOS_LOG_D("App already in target index");
        return EOS_OK;
    }

    // 移除应用
    cJSON *app_item = cJSON_DetachItemFromArray(app_order_json, current_index);

    // 插入到新位置
    if (new_index < array_size - 1)
    {
        cJSON_InsertItemInArray(app_order_json, new_index, app_item);
    }
    else
    {
        cJSON_AddItemToArray(app_order_json, app_item);
    }

    return _eos_app_order_save();
}

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

const char *eos_app_list_get_existing_id(const char *id)
{
    for (size_t i = 0; i < app_list.size; i++)
    {
        if (strcmp(app_list.data[i], id) == 0)
        {
            return app_list.data[i];
        }
    }
    return NULL;
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
    list->data[list->size] = eos_strdup(id); // 复制字符串
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
    // 添加到顺序列表
    _eos_app_order_add(header.pkg_id);
    _eos_app_list_refresh();
    EOS_LOG_D("App installed successfully: %s", header.pkg_name);
    const char *app_id = eos_app_list_get_existing_id(header.pkg_id);
    EOS_LOG_D("app_id=%s\npkg_id=%s", app_id, header.pkg_id);
    eos_event_broadcast(eos_event_get_code(EOS_EVENT_APP_INSTALLED), (void *)app_id);
    return EOS_OK;
}

eos_result_t eos_app_uninstall(const char *app_id)
{
    EOS_LOG_D("Uninstall: %s", app_id);
    // 卸载应用程序
    eos_event_broadcast(eos_event_get_code(EOS_EVENT_APP_DELETED), (void *)app_id);

    // 从顺序列表中移除
    _eos_app_order_remove(app_id);

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
    if (eos_is_dir(data_path))
    {
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

static void _app_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    const char *deleted_app_id = lv_event_get_param(e);
    const char *obj_app_id = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(obj);
    EOS_LOG_D("_app_delete_cb target obj=%p", obj);
    if (strcmp(deleted_app_id, obj_app_id) == 0)
    {
        eos_event_remove_cb(obj, eos_event_get_code(EOS_EVENT_APP_DELETED), _app_delete_cb);
        lv_obj_delete(obj);
    }
}

void eos_app_obj_auto_delete(lv_obj_t *obj, const char *app_id)
{
    EOS_CHECK_PTR_RETURN(obj);
    EOS_LOG_D("Auto del regesited: %s, ptr: %p", app_id, obj);
    eos_event_add_cb(obj,
                     _app_delete_cb,
                     eos_event_get_code(EOS_EVENT_APP_DELETED),
                     (void *)eos_strdup(app_id));
}

eos_result_t eos_app_init(void)
{
    // 初始化 从文件系统中读取应用列表
    _eos_app_list_refresh();

    // 加载应用顺序
    _eos_app_order_load();

    // 清理JSON中不存在的应用
    if (app_order_json)
    {
        cJSON *item = NULL;
        int index = 0;
        cJSON_ArrayForEach(item, app_order_json)
        {
            if (cJSON_IsString(item))
            {
                const char *app_id = item->valuestring;
                // 系统设置应用始终保留
                if (strcmp(app_id, "sys.settings") != 0 &&
                    !eos_app_list_contains(app_id))
                {
                    // 应用不存在，从JSON中移除
                    cJSON_DeleteItemFromArray(app_order_json, index);
                    _eos_app_order_save();
                    // 由于删除了一个元素，需要重新遍历
                    break;
                }
            }
            index++;
        }
    }

    return EOS_OK;
}
