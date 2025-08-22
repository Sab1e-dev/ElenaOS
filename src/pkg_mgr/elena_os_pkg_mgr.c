/**
 * @file elena_os_pkg_mgr.c
 * @brief 包管理器
 * @author Sab1e
 * @date 2025-08-22
 */

#include "elena_os_pkg_mgr.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <ftw.h>
#include "elena_os_misc.h"
#include "elena_os_port.h"
#include "elena_os_log.h"
// Macros and Definitions
#define EOS_PKG_HEADER_LENGTH sizeof(eos_pkg_header_t)
#define EOS_PKG_READ_BLOCK 512
// Variables

// Function Implementations

eos_result_t eos_pkg_mgr_unpack(const char *pkg_path, const char *output_path, const ScriptType_t pkg_type)
{
    // 打开包文件
    int fd = open(pkg_path, O_RDONLY);
    if (fd < 0)
    {
        EOS_LOG_E("Failed to open package file");
        return -EOS_ERR_FILE_ERROR;
    }

    // 读取包头
    eos_pkg_header_t header;
    if (read(fd, &header, EOS_PKG_HEADER_LENGTH) != EOS_PKG_HEADER_LENGTH)
    {
        close(fd);
        EOS_LOG_E("Failed to read package header");
        return -EOS_ERR_FILE_ERROR;
    }

    // 校验魔数
    ScriptType_t unpack_type = SCRIPT_TYPE_UNKNOWN;
    if (memcmp(header.magic, EOS_PKG_APP_MAGIC, 4) == 0)
    {
        unpack_type = SCRIPT_TYPE_APPLICATION;
    }
    else if (memcmp(header.magic, EOS_PKG_WATCHFACE_MAGIC, 4) == 0)
    {
        unpack_type = SCRIPT_TYPE_WATCHFACE;
    }
    else
    {
        close(fd);
        EOS_LOG_E("Invalid magic number");
        return -EOS_ERR_FILE_ERROR;
    }

    // 检查包类型是否匹配
    if (unpack_type != pkg_type)
    {
        close(fd);
        EOS_LOG_E("Package type mismatch: expected %d, got %d", pkg_type, unpack_type);
        return -EOS_ERR_VALUE_MISMATCH;
    }

    // 获取文件大小
    off_t file_size = lseek(fd, 0, SEEK_END);
    if (file_size == -1) {
        close(fd);
        EOS_LOG_E("Failed to get file size");
        return -EOS_ERR_FILE_ERROR;
    }
    
    // 验证文件表偏移量（应该在文件头之后）
    if (header.table_offset < sizeof(eos_pkg_header_t) || header.table_offset >= file_size) {
        close(fd);
        EOS_LOG_E("Invalid table offset: %u (file size: %ld)", header.table_offset, file_size);
        return -EOS_ERR_FILE_ERROR;
    }

    // 定位到文件表位置
    if (lseek(fd, header.table_offset, SEEK_SET) == -1) {
        close(fd);
        EOS_LOG_E("Failed to seek to file table at offset %u", header.table_offset);
        return -EOS_ERR_FILE_ERROR;
    }

    // 创建输出目录
    if (eos_create_dir_recursive(output_path) != EOS_OK)
    {
        close(fd);
        EOS_LOG_E("Failed to create output directory");
        return -EOS_ERR_FILE_ERROR;
    }

    // 处理每个文件条目
    for (uint32_t i = 0; i < header.file_count; i++)
    {
        // 读取文件名长度
        uint32_t name_len;
        read(fd, &name_len, sizeof(uint32_t));
        
        // 动态分配内存
        char *name = (char *)malloc(name_len + 1);
        
        // 读取文件名
        read(fd, name, name_len);
        name[name_len] = '\0';

        // 读取条目其他字段
        uint32_t is_dir, offset, size;
        if (read(fd, &is_dir, sizeof(uint32_t)) != sizeof(uint32_t) ||
            read(fd, &offset, sizeof(uint32_t)) != sizeof(uint32_t) ||
            read(fd, &size, sizeof(uint32_t)) != sizeof(uint32_t))
        {
            free(name);
            close(fd);
            EOS_LOG_E("Failed to read entry fields for %s", name);
            return -EOS_ERR_FILE_ERROR;
        }

        // 构建完整输出路径
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", output_path, name);

        if (is_dir)
        {
            // 创建目录
            if (eos_create_dir_recursive(full_path) != EOS_OK)
            {
                free(name);
                close(fd);
                EOS_LOG_E("Failed to create directory: %s", full_path);
                return -EOS_ERR_FILE_ERROR;
            }
            EOS_LOG_D("Created directory: %s", full_path);
        }
        else
        {
            // 验证文件偏移量和大小
            if (offset < header.table_offset || offset >= file_size) {
                free(name);
                close(fd);
                EOS_LOG_E("Invalid file offset: %u for %s", offset, name);
                return -EOS_ERR_FILE_ERROR;
            }
            
            if (offset + size > file_size) {
                free(name);
                close(fd);
                EOS_LOG_E("File size overflow: %u+%u=%u for %s", 
                          offset, size, offset + size, name);
                return -EOS_ERR_FILE_ERROR;
            }

            // 确保父目录存在
            char *last_slash = strrchr(full_path, '/');
            if (last_slash)
            {
                *last_slash = '\0';
                if (eos_create_dir_recursive(full_path) != EOS_OK)
                {
                    free(name);
                    close(fd);
                    EOS_LOG_E("Failed to create parent directory: %s", full_path);
                    return -EOS_ERR_FILE_ERROR;
                }
                *last_slash = '/';
            }

            // 创建文件并写入数据
            FILE *out_fp = fopen(full_path, "wb");
            if (!out_fp)
            {
                free(name);
                close(fd);
                EOS_LOG_E("Failed to create file: %s", full_path);
                return -EOS_ERR_FILE_ERROR;
            }

            // 保存当前位置，稍后返回
            off_t current_pos = lseek(fd, 0, SEEK_CUR);
            
            // 定位到文件数据
            if (lseek(fd, offset, SEEK_SET) == -1)
            {
                fclose(out_fp);
                free(name);
                close(fd);
                EOS_LOG_E("Failed to seek to file data for %s", name);
                return -EOS_ERR_FILE_ERROR;
            }

            // 逐块读取并写入文件
            uint32_t remaining = size;
            uint8_t buffer[EOS_PKG_READ_BLOCK];
            while (remaining > 0)
            {
                size_t to_read = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
                ssize_t r = read(fd, buffer, to_read);
                if (r <= 0)
                {
                    fclose(out_fp);
                    free(name);
                    close(fd);
                    EOS_LOG_E("Failed to read file data for %s", name);
                    return -EOS_ERR_FILE_ERROR;
                }
                if (fwrite(buffer, 1, r, out_fp) != (size_t)r)
                {
                    fclose(out_fp);
                    free(name);
                    close(fd);
                    EOS_LOG_E("Failed to write file data for %s", name);
                    return -EOS_ERR_FILE_ERROR;
                }
                remaining -= r;
            }

            fclose(out_fp);
            EOS_LOG_D("Created file: %s (size: %u bytes)", full_path, size);
            
            // 返回到文件表位置继续读取下一个条目
            if (lseek(fd, current_pos, SEEK_SET) == -1) {
                free(name);
                close(fd);
                EOS_LOG_E("Failed to return to file table");
                return -EOS_ERR_FILE_ERROR;
            }
        }

        free(name);
    }

    close(fd);
    return EOS_OK;
}