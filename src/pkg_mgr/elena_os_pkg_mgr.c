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
#define EOS_PKG_HEADER_LENGTH EOS_PKG_TABLE_OFFSET
// Variables

// Function Implementations

eos_result_t eos_pkg_mgr_unpack(const char *pkg_path, const char *output_path, const script_pkg_type_t pkg_type)
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
    memset(&header, 0, sizeof(header));

    // 读取magic
    if (lseek(fd, EOS_PKG_MAGIC_OFFSET, SEEK_SET) == -1 ||
        read(fd, header.magic, 4) != 4)
    {
        close(fd);
        EOS_LOG_E("Failed to read magic number");
        return -EOS_ERR_FILE_ERROR;
    }

    // 读取pkg_name
    if (lseek(fd, EOS_PKG_NAME_OFFSET, SEEK_SET) == -1 ||
        read(fd, header.pkg_name, EOS_PKG_NAME_LEN_MAX) != EOS_PKG_NAME_LEN_MAX)
    {
        close(fd);
        EOS_LOG_E("Failed to read package name");
        return -EOS_ERR_FILE_ERROR;
    }
    header.pkg_name[EOS_PKG_NAME_LEN_MAX-1] = '\0';

    // 读取pkg_version
    if (lseek(fd, EOS_PKG_VERSION_OFFSET, SEEK_SET) == -1 ||
        read(fd, header.pkg_version, EOS_PKG_VERSION_LEN_MAX) != EOS_PKG_VERSION_LEN_MAX)
    {
        close(fd);
        EOS_LOG_E("Failed to read package version");
        return -EOS_ERR_FILE_ERROR;
    }
    header.pkg_version[EOS_PKG_VERSION_LEN_MAX-1] = '\0';

    // 读取file_count
    if (lseek(fd, EOS_PKG_FILE_COUNT_OFFSET, SEEK_SET) == -1 ||
        read(fd, &header.file_count, sizeof(uint32_t)) != sizeof(uint32_t))
    {
        close(fd);
        EOS_LOG_E("Failed to read file count");
        return -EOS_ERR_FILE_ERROR;
    }

    // 读取reserved (虽然不使用，但为了完整性)
    if (lseek(fd, EOS_PKG_RESERVED_OFFSET, SEEK_SET) == -1 ||
        read(fd, &header.reserved, sizeof(uint32_t)) != sizeof(uint32_t))
    {
        close(fd);
        EOS_LOG_E("Failed to read reserved field");
        return -EOS_ERR_FILE_ERROR;
    }

    EOS_LOG_D("[PKG_MGR]====================\n"
        "Magic: %s | Pkg Name: %s | Pkg Version: %s\n"
        "File Count: %d | Table Offset: %d"
        ,header.magic,header.pkg_name,header.pkg_version
        ,header.file_count, EOS_PKG_HEADER_LENGTH);
    // 校验魔数
    script_pkg_type_t unpack_type = SCRIPT_TYPE_UNKNOWN;
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

    // 定位到文件表位置 (紧接在文件头之后)
    if (lseek(fd, EOS_PKG_TABLE_OFFSET, SEEK_SET) == -1) {
        close(fd);
        EOS_LOG_E("Failed to seek to file table at offset %u", EOS_PKG_TABLE_OFFSET);
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
        if (read(fd, &name_len, sizeof(uint32_t)) != sizeof(uint32_t)) {
            close(fd);
            EOS_LOG_E("Failed to read name length for entry %u", i);
            return -EOS_ERR_FILE_ERROR;
        }
        
        // 检查文件名长度是否合理
        if (name_len > PATH_MAX) {
            close(fd);
            EOS_LOG_E("Name length %u too long for entry %u", name_len, i);
            return -EOS_ERR_FILE_ERROR;
        }

        // 动态分配内存
        char *name = (char *)malloc(name_len + 1);
        if (!name) {
            close(fd);
            EOS_LOG_E("Memory allocation failed for entry %u", i);
            return -EOS_ERR_MEM;
        }
        
        // 读取文件名
        if (read(fd, name, name_len) != name_len) {
            free(name);
            close(fd);
            EOS_LOG_E("Failed to read name for entry %u", i);
            return -EOS_ERR_FILE_ERROR;
        }
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
            if (offset < EOS_PKG_TABLE_OFFSET || offset >= file_size) {
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
