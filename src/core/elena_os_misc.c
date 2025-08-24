/**
 * @file elena_os_misc.c
 * @brief 各种工具函数
 * @author Sab1e
 * @date 2025-08-22
 */

#include "elena_os_misc.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "elena_os_log.h"
#include "elena_os_port.h"
// Macros and Definitions

// Variables

// Function Implementations
bool eos_is_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
    {
        return true; // 存在且是目录
    }
    return false;
}

bool eos_is_file(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
    {
        return true; // 存在且是普通文件
    }
    return false;
}

eos_result_t eos_mkdir_if_not_exist(const char *path, mode_t mode)
{
    if (!eos_is_dir(path))
    {
        if (mkdir(path, mode) == 0)
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
    }
    return EOS_OK;
}

eos_result_t eos_create_file_if_not_exist(const char *path, const char *default_content)
{
    if (!eos_is_file(path))
    {
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -EOS_ERR_FILE_ERROR)
        {
            EOS_LOG_E("open %s failed, errno=%d", path, errno);
            return -EOS_ERR_FILE_ERROR;
        }

        if (default_content)
        {
            ssize_t len = strlen(default_content);
            ssize_t written = write(fd, default_content, len);
            if (written != len)
            {
                EOS_LOG_E("write %s failed, written=%zd, errno=%d", path, written, errno);
                close(fd);
                return -EOS_ERR_FILE_ERROR;
            }
        }

        close(fd);
        EOS_LOG_I("Created file: %s", path);
    }
    return EOS_OK;
}

eos_result_t eos_create_dir_recursive(const char *path)
{
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;
            mkdir(tmp, 0755); // POSIX 创建目录
            *p = '/';
        }
    }
    mkdir(tmp, 0755); // 创建最后一级目录
    return EOS_OK;
}

eos_result_t eos_rm_recursive(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        EOS_LOG_E("stat failed: %s, errno=%d\n", path, errno);
        return -EOS_ERR_FILE_ERROR;
    }

    if (S_ISDIR(st.st_mode))
    {
        DIR *dir = opendir(path);
        if (!dir)
        {
            EOS_LOG_E("opendir failed: %s, errno=%d\n", path, errno);
            return -EOS_ERR_FILE_ERROR;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            // 忽略 “.” 和 “..”
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

            if (eos_rm_recursive(full_path) != EOS_OK)
            {
                closedir(dir);
                return -EOS_ERR_FILE_ERROR;
            }
        }
        closedir(dir);

        // 删除空目录
        if (rmdir(path) != 0)
        {
            EOS_LOG_E("rmdir failed: %s, errno=%d\n", path, errno);
            return -EOS_ERR_FILE_ERROR;
        }
    }
    else
    {
        // 删除文件
        if (unlink(path) != 0)
        {
            EOS_LOG_E("unlink failed: %s, errno=%d\n", path, errno);
            return -EOS_ERR_FILE_ERROR;
        }
    }
    return EOS_OK;
}

bool eos_is_valid_filename(const char *name)
{
    if (!name || name[0] == '\0')
    {
        EOS_LOG_E("Filename NULL");
        return false; // 空名不行
    }

    const char *invalid_chars = "/\\:*?\"<>|";

    for (const char *p = name; *p; p++)
    {
        // 控制字符不允许
        if ((unsigned char)*p < 32)
        {
            EOS_LOG_E("Filename control char");
            return false;
        }
        // 特殊字符不允许
        if (strchr(invalid_chars, *p))
        {
            EOS_LOG_E("Filename invalid char");
            return false;
        }
    }
    return true; // 合法
}

char *eos_read_file(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        perror("fopen");
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *data = (char *)eos_mem_alloc(size + 1);
    if (!data)
    {
        fclose(fp);
        return NULL;
    }
    fread(data, 1, size, fp);
    data[size] = '\0'; // 记得结尾加0
    fclose(fp);
    return data;
}

const char *eos_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = eos_mem_alloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

void eos_pkg_free(script_pkg_t *pkg) {
    EOS_CHECK_PTR_RETURN(pkg);

    if (pkg->id)           eos_mem_free((void *)pkg->id);
    if (pkg->name)         eos_mem_free((void *)pkg->name);
    if (pkg->version)      eos_mem_free((void *)pkg->version);
    if (pkg->author)       eos_mem_free((void *)pkg->author);
    if (pkg->description)  eos_mem_free((void *)pkg->description);
    if (pkg->script_str)   eos_mem_free((void *)pkg->script_str);
    pkg->id = NULL;
    pkg->name = NULL;
    pkg->version = NULL;
    pkg->author = NULL;
    pkg->description = NULL;
    pkg->script_str = NULL;
}