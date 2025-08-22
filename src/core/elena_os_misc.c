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

eos_result_t eos_create_file_if_not_exist(const char *path, const char *default_content) {
    if (!eos_is_file(path)) {
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -EOS_ERR_FILE_ERROR) {
            EOS_LOG_E("open %s failed, errno=%d", path, errno);
            return -EOS_ERR_FILE_ERROR;
        }

        if (default_content) {
            ssize_t len = strlen(default_content);
            ssize_t written = write(fd, default_content, len);
            if (written != len) {
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

