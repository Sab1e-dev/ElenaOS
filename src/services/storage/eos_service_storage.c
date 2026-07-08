/**
 * @file eos_service_storage.c
 * @brief Storage service file operation utilities
 */

#include "eos_service_storage.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_config.h"
#include "eos_log.h"
#include "eos_mem.h"
#include "eos_dfw.h"
#include "cJSON.h"
#include "eos_lvgl_fs.h"
/* Macros and Definitions -------------------------------------*/
#define _FILE_NAME_MAX_LENGTH 256
/* Variables --------------------------------------------------*/

/* Path handling utilities -------------------------------------*/

bool eos_storage_is_valid_filename(const char *name)
{
    if (!name || name[0] == '\0')
    {
        EOS_LOG_E("Filename NULL");
        return false;
    }

    const char *invalid_chars = "/\\:*?\"<>|";

    for (const char *p = name; *p; p++)
    {
        if ((unsigned char)*p < 32)
        {
            EOS_LOG_E("Filename control char");
            return false;
        }
        if (strchr(invalid_chars, *p))
        {
            EOS_LOG_E("Filename invalid char");
            return false;
        }
    }
    return true;
}

/* Function Implementations -----------------------------------*/

/************************** JSON Storage API **************************/

cJSON *eos_storage_json_load(const char *path)
{
    if (!eos_storage_is_file(path))
    {
        return NULL;
    }

    char *content = eos_storage_read_file(path);
    if (!content)
    {
        return NULL;
    }

    cJSON *root = cJSON_Parse(content);
    eos_free(content);

    return root;
}

eos_result_t eos_storage_json_save(const char *path, cJSON *root)
{
    EOS_CHECK_PTR_RETURN_VAL(path && root, EOS_ERR_INVALID_ARG);

    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str)
    {
        return EOS_ERR_JSON_ERROR;
    }

    eos_result_t ret = eos_storage_write_file(path, json_str, strlen(json_str));
    cJSON_free(json_str);

    return ret;
}

bool eos_storage_json_get_bool(const char *path, const char *key, bool default_value)
{
    cJSON *root = eos_storage_json_load(path);
    if (!root)
    {
        return default_value;
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    bool result = (item && cJSON_IsBool(item)) ? cJSON_IsTrue(item) : default_value;

    cJSON_Delete(root);
    return result;
}

eos_result_t eos_storage_json_set_bool(const char *path, const char *key, bool value)
{
    cJSON *root = eos_storage_json_load(path);
    if (!root)
    {
        root = cJSON_CreateObject();
        if (!root)
        {
            return EOS_ERR_MEM;
        }
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item)
    {
        cJSON_SetBoolValue(item, value);
    }
    else
    {
        cJSON_AddBoolToObject(root, key, value);
    }

    eos_result_t ret = eos_storage_json_save(path, root);
    cJSON_Delete(root);

    return ret;
}

char *eos_storage_json_get_string(const char *path, const char *key, const char *default_value)
{
    cJSON *root = eos_storage_json_load(path);
    if (!root)
    {
        return eos_strdup(default_value);
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    char *result = (item && cJSON_IsString(item)) ? eos_strdup(item->valuestring) : eos_strdup(default_value);

    cJSON_Delete(root);
    return result;
}

eos_result_t eos_storage_json_set_string(const char *path, const char *key, const char *value)
{
    cJSON *root = eos_storage_json_load(path);
    if (!root)
    {
        root = cJSON_CreateObject();
        if (!root)
        {
            return EOS_ERR_MEM;
        }
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item)
    {
        cJSON_SetValuestring(item, value);
    }
    else
    {
        cJSON_AddStringToObject(root, key, value);
    }

    eos_result_t ret = eos_storage_json_save(path, root);
    cJSON_Delete(root);

    return ret;
}

double eos_storage_json_get_number(const char *path, const char *key, double default_value)
{
    cJSON *root = eos_storage_json_load(path);
    if (!root)
    {
        return default_value;
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    double result = (item && cJSON_IsNumber(item)) ? item->valuedouble : default_value;

    cJSON_Delete(root);
    return result;
}

eos_result_t eos_storage_json_set_number(const char *path, const char *key, double value)
{
    cJSON *root = eos_storage_json_load(path);
    if (!root)
    {
        root = cJSON_CreateObject();
        if (!root)
        {
            return EOS_ERR_MEM;
        }
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item)
    {
        cJSON_SetNumberValue(item, value);
    }
    else
    {
        cJSON_AddNumberToObject(root, key, value);
    }

    eos_result_t ret = eos_storage_json_save(path, root);
    cJSON_Delete(root);

    return ret;
}

cJSON *eos_storage_json_get_json(const char *path, const char *key)
{
    cJSON *root = eos_storage_json_load(path);
    if (!root)
    {
        return NULL;
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (!item || (!cJSON_IsObject(item) && !cJSON_IsArray(item)))
    {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON_DetachItemViaPointer(root, item);
    cJSON_Delete(root);

    return item;
}

eos_result_t eos_storage_json_set_json(const char *path, const char *key, cJSON *json_value)
{
    cJSON *root = eos_storage_json_load(path);
    if (!root)
    {
        root = cJSON_CreateObject();
        if (!root)
        {
            return EOS_ERR_MEM;
        }
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item)
    {
        cJSON_ReplaceItemInObject(root, key, json_value);
    }
    else
    {
        cJSON_AddItemToObject(root, key, json_value);
    }

    eos_result_t ret = eos_storage_json_save(path, root);
    cJSON_Delete(root);

    return ret;
}

eos_result_t eos_storage_json_create_if_not_exist(const char *path, const char *default_json)
{
    if (eos_storage_is_file(path))
    {
        return EOS_OK;
    }

    const char *content = default_json ? default_json : "{}";
    return eos_storage_create_file_if_not_exist(path, content);
}

bool eos_storage_is_dir(const char *path)
{
    return (eos_fs_type(path) == EOS_FS_TYPE_DIR) ? true : false;
}

bool eos_storage_is_file(const char *path)
{
    return (eos_fs_type(path) == EOS_FS_TYPE_FILE) ? true : false;
}

eos_result_t eos_storage_puts(const char *s, eos_file_t fp)
{
    if (fp == EOS_FILE_INVALID || !s)
        return EOS_ERR_INVALID_ARG;

    ssize_t written = eos_fs_write(fp, s, strlen(s));
    return (written < 0) ? EOS_ERR_IO : EOS_OK;
}

eos_result_t eos_storage_mkdir_if_not_exist(const char *path)
{
    int type = eos_fs_type(path);
    if (type == EOS_FS_TYPE_DIR)
    {
        return EOS_OK;
    }

    if (type == EOS_FS_TYPE_FILE)
    {
        return EOS_ERR_ALREADY_EXISTS;
    }

    if (type == EOS_FS_TYPE_NOT_EXIST)
    {
        return (eos_fs_mkdir(path) == EOS_OK) ? EOS_OK : EOS_ERR_FILE_ERROR;
    }

    return EOS_ERR_FILE_ERROR;
}

eos_result_t eos_storage_create_file_if_not_exist(const char *path, const char *default_content)
{
    int type = eos_fs_type(path);
    if (type == EOS_FS_TYPE_FILE)
    {
        return EOS_OK;
    }

    if (type == EOS_FS_TYPE_DIR)
    {
        return EOS_ERR_ALREADY_EXISTS;
    }

    if (type == EOS_FS_TYPE_NOT_EXIST)
    {
        eos_file_t fp = eos_fs_open_write(path);
        if (fp == EOS_FILE_INVALID)
            return EOS_ERR_FILE_ERROR;

        if (default_content)
        {
            ssize_t len = strlen(default_content);
            ssize_t written = eos_fs_write(fp, default_content, len);
            if (written != len)
            {
                EOS_LOG_E("write %s failed, written=%zd", path, written);
                eos_fs_close(fp);
                return EOS_ERR_IO;
            }
        }

        eos_fs_close(fp);
        EOS_LOG_I("Created file: %s", path);
        return EOS_OK;
    }

    return EOS_ERR_FILE_ERROR;
}

eos_result_t eos_storage_mkdir_recursive(const char *path)
{
    char tmp[_FILE_NAME_MAX_LENGTH];
    char *p = NULL;
    size_t len;

    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    len = strlen(tmp);

#if EOS_FS_TYPE == EOS_FS_FATFS
    if (len > 0 && (tmp[len - 1] == '\\' || tmp[len - 1] == '/'))
    {
        tmp[len - 1] = '\0';
        len--;
    }
#else
    if (len > 0 && tmp[len - 1] == '/')
    {
        tmp[len - 1] = '\0';
        len--;
    }
#endif

    if (len >= sizeof(tmp) - 1)
    {
        return EOS_ERR_PATH_TOO_LONG;
    }

    p = tmp;
#if EOS_FS_TYPE == EOS_FS_FATFS
    if (len >= 2 && tmp[1] == ':')
    {
        p = tmp + 2;
        if (*p == '\\' || *p == '/')
        {
            p++;
        }
    }
#else
    if (tmp[0] == '/')
    {
        p = tmp + 1;
    }
#endif

    for (; *p; p++)
    {
#if EOS_FS_TYPE == EOS_FS_FATFS
        if (*p == '\\' || *p == '/')
        {
#else
        if (*p == '/')
        {
#endif
            *p = '\0';

            int type = eos_fs_type(tmp);
            if (type == EOS_FS_TYPE_NOT_EXIST)
            {
                if (eos_fs_mkdir(tmp) != EOS_OK)
                {
                    return EOS_ERR_FILE_ERROR;
                }
            }
            else if (type != EOS_FS_TYPE_DIR)
            {
                return EOS_ERR_INVALID_STATE;
            }

#if EOS_FS_TYPE == EOS_FS_FATFS
            *p = '\\';
#else
            *p = '/';
#endif
        }
    }

    int type = eos_fs_type(tmp);
    if (type == EOS_FS_TYPE_NOT_EXIST)
    {
        if (eos_fs_mkdir(tmp) != EOS_OK)
        {
            return EOS_ERR_FILE_ERROR;
        }
    }
    else if (type != EOS_FS_TYPE_DIR)
    {
        return EOS_ERR_INVALID_STATE;
    }

    return EOS_OK;
}

eos_result_t eos_storage_write_file_immediate(const char *path, const void *data, size_t data_size)
{
    EOS_CHECK_PTR_RETURN_VAL(data, EOS_ERR_INVALID_ARG);
    if (data_size == 0)
    {
        return EOS_ERR_INVALID_ARG;
    }

    eos_file_t fp = eos_fs_open_write(path);
    if (fp == EOS_FILE_INVALID)
        return EOS_ERR_FILE_ERROR;

    ssize_t written = eos_fs_write(fp, data, data_size);

    eos_fs_close(fp);

    return (written == data_size) ? EOS_OK : EOS_ERR_IO;
}

char *eos_storage_read_file_immediate(const char *path)
{
    eos_file_t fp = eos_fs_open_read(path);
    if (fp == EOS_FILE_INVALID)
    {
        EOS_LOG_E("Failed to open file: %s", path);
        return NULL;
    }

    uint32_t file_size = 0;
    eos_fs_size(fp, &file_size);

    if (file_size <= 0)
    {
        EOS_LOG_E("Invalid file size");
        eos_fs_close(fp);
        return NULL;
    }

    char *buf = eos_malloc(file_size + 1);
    if (!buf)
    {
        EOS_LOG_E("Failed to allocate memory for file");
        eos_fs_close(fp);
        return NULL;
    }

    ssize_t bytes_read = eos_fs_read(fp, buf, file_size);

    if (bytes_read != file_size)
    {
        EOS_LOG_E("Failed to read complete file (read %zd of %ld bytes)", bytes_read, file_size);
        eos_fs_close(fp);
        return NULL;
    }
    eos_fs_close(fp);
    buf[file_size] = '\0';
    return buf;
}

eos_result_t eos_storage_write_file(const char *path, const void *data, size_t data_size)
{
#if EOS_DFW_ENABLE
    if (!path || !data || data_size == 0)
    {
        return EOS_ERR_INVALID_ARG;
    }
    return eos_dfw_write(path, (const uint8_t *)data, data_size) ? EOS_OK : EOS_ERR_FILE_ERROR;
#else
    return eos_storage_write_file_immediate(path, data, data_size);
#endif
}

char *eos_storage_read_file(const char *path)
{
#if EOS_DFW_ENABLE
    return (char *)eos_dfw_read(path);
#else
    return eos_storage_read_file_immediate(path);
#endif
}

eos_result_t eos_storage_rm_recursive(const char *path)
{
    if (strcmp(path, "/") == 0 || strcmp(path, "\\") == 0)
    {
        return EOS_ERR_INVALID_ARG;
    }

    int type = eos_fs_type(path);

    switch (type)
    {
        case EOS_FS_TYPE_NOT_EXIST:
            return EOS_OK;

        case EOS_FS_TYPE_FILE:
            return (eos_storage_file_remove(path) == EOS_OK) ? EOS_OK : EOS_ERR_FILE_ERROR;

        case EOS_FS_TYPE_DIR:
        {
            eos_dir_t dir = eos_storage_dir_open(path);
            if (!dir)
            {
                return EOS_ERR_FILE_ERROR;
            }

            char filename[_FILE_NAME_MAX_LENGTH];
            char fullpath[EOS_FS_PATH_MAX];
            eos_result_t result = EOS_OK;

            while (eos_storage_dir_read(dir, filename, sizeof(filename)) == EOS_OK)
            {
                if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
                {
                    continue;
                }

#if EOS_FS_TYPE == EOS_FS_FATFS
                snprintf(fullpath, sizeof(fullpath), "%s\\%s", path, filename);
#else
                snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
#endif

                if (eos_storage_rm_recursive(fullpath) != EOS_OK)
                {
                    result = EOS_ERR_FILE_ERROR;
                    break;
                }
            }

            eos_storage_dir_close(dir);

            if (result != EOS_OK)
            {
                return result;
            }

            return (eos_fs_rmdir(path) == EOS_OK) ? EOS_OK : EOS_ERR_FILE_ERROR;
        }

        default:
            return EOS_ERR_FILE_ERROR;
    }
}

/************************** File Handle API Implementations **************************/

eos_file_t eos_storage_file_open_read(const char *path)
{
    return eos_fs_open_read(path);
}

eos_file_t eos_storage_file_open_write(const char *path)
{
    return eos_fs_open_write(path);
}

void eos_storage_file_close(eos_file_t fp)
{
    eos_fs_close(fp);
}

eos_result_t eos_storage_file_seek(eos_file_t fp, uint32_t offset)
{
    if (fp == EOS_FILE_INVALID)
    {
        EOS_LOG_E("Invalid file handle");
        return EOS_ERR_INVALID_ARG;
    }

    return eos_fs_seek(fp, offset);
}

ssize_t eos_storage_file_read(eos_file_t fp, void *buf, size_t size)
{
    if (fp == EOS_FILE_INVALID || !buf)
    {
        EOS_LOG_E("Invalid parameters");
        return -1;
    }

    return eos_fs_read(fp, buf, size);
}

ssize_t eos_storage_file_write(eos_file_t fp, const void *buf, size_t size)
{
    if (fp == EOS_FILE_INVALID || !buf)
    {
        EOS_LOG_E("Invalid parameters");
        return -1;
    }

    return eos_fs_write(fp, buf, size);
}

eos_result_t eos_storage_file_size(eos_file_t fp, uint32_t *size)
{
    if (fp == EOS_FILE_INVALID || !size)
    {
        EOS_LOG_E("Invalid parameters");
        return EOS_ERR_INVALID_ARG;
    }

    return eos_fs_size(fp, size);
}

eos_result_t eos_storage_file_tell(eos_file_t fp, uint32_t *pos)
{
    if (fp == EOS_FILE_INVALID || !pos)
    {
        EOS_LOG_E("Invalid parameters");
        return EOS_ERR_INVALID_ARG;
    }

    return eos_fs_tell(fp, pos);
}

eos_result_t eos_storage_file_remove(const char *path)
{
    return eos_fs_remove(path);
}

eos_dir_t eos_storage_dir_open(const char *path)
{
    return eos_fs_opendir(path);
}

eos_result_t eos_storage_dir_read(eos_dir_t dir, char *name_buf, size_t buf_size)
{
    if (!dir || !name_buf || buf_size == 0)
    {
        EOS_LOG_E("Invalid parameters");
        return EOS_ERR_INVALID_ARG;
    }

    return eos_fs_readdir(dir, name_buf, buf_size);
}

void eos_storage_dir_close(eos_dir_t dir)
{
    eos_fs_closedir(dir);
}

void eos_service_storage_init(void)
{
    eos_lvgl_fs_register();
#if EOS_DFW_ENABLE
    eos_dfw_init();
#endif /* EOS_DFW_ENABLE */
    EOS_LOG_I("Storage service initialized");
}
