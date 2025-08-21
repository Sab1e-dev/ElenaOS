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

ElenaOSResult_t eos_mkdir_if_not_exist(const char *path, mode_t mode)
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

ElenaOSResult_t eos_create_file_if_not_exist(const char *path, const char *default_content) {
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

int list_dir(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir failed");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        printf("Found: %s", entry->d_name);

        // 如果需要区分文件和目录，可以用 stat
        char fullpath[256];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode))
                printf(" [DIR]");
            else if (S_ISREG(st.st_mode))
                printf(" [FILE]");
        }

        printf("\n");
    }

    closedir(dir);
    return 0;
}