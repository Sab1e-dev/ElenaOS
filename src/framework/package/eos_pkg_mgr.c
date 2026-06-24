/**
 * @file eos_pkg_mgr.c
 * @brief Package manager
 */

#include "eos_pkg_mgr.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_port.h"
#define EOS_LOG_TAG "PackageManager"
#include "eos_log.h"
#include "eos_service_storage.h"
#include "eos_mem.h"
/* Macros and Definitions -------------------------------------*/
#define EOS_PKG_HEADER_LENGTH EOS_PKG_TABLE_OFFSET
/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

void eos_pkg_free(script_pkg_t *pkg)
{
    EOS_CHECK_PTR_RETURN(pkg);

    if (pkg->id)
        eos_free((void *)pkg->id);
    if (pkg->name)
        eos_free((void *)pkg->name);
    if (pkg->version)
        eos_free((void *)pkg->version);
    if (pkg->author)
        eos_free((void *)pkg->author);
    if (pkg->description)
        eos_free((void *)pkg->description);
    if (pkg->script_str)
        eos_free((void *)pkg->script_str);
    if (pkg->base_path)
        eos_free((void *)pkg->base_path);
    if (pkg->permissions)
    {
        for (uint8_t i = 0; i < pkg->permission_count; i++)
        {
            if (pkg->permissions[i])
                eos_free((void *)pkg->permissions[i]);
        }
        eos_free(pkg->permissions);
    }
    pkg->id = NULL;
    pkg->name = NULL;
    pkg->type = SCRIPT_TYPE_UNKNOWN;
    pkg->version = NULL;
    pkg->author = NULL;
    pkg->description = NULL;
    pkg->script_str = NULL;
    pkg->base_path = NULL;
    pkg->permissions = NULL;
    pkg->permission_count = 0;
    pkg->min_api_level = 0;
    pkg->target_api_level = 0;
}

eos_result_t eos_pkg_read_header(const char *pkg_path, eos_pkg_header_t *header)
{
    // Validate input parameters
    if (!pkg_path || !header) {
        EOS_LOG_E("Invalid parameters: pkg_path=%p, header=%p", pkg_path, header);
        return EOS_ERR_VAR_NULL;
    }

    // Check if it's a regular file
    if (!eos_storage_is_file(pkg_path)) {
        EOS_LOG_E("Path is not a file: %s", pkg_path);
        return EOS_ERR_FILE_ERROR;
    }

    // Open package file
    eos_file_t fp = eos_storage_file_open_read(pkg_path);
    if (fp == EOS_FILE_INVALID)
    {
        EOS_LOG_E("Failed to open package file: %s", pkg_path);
        return EOS_ERR_FILE_ERROR;
    }

    // Initialize header with zeros
    memset(header, 0, sizeof(eos_pkg_header_t));

    // Read magic number
    if (eos_storage_file_seek(fp, EOS_PKG_MAGIC_OFFSET) != EOS_OK ||
        eos_storage_file_read(fp, header->magic, 4) != 4)
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Failed to read magic number");
        return EOS_ERR_FILE_ERROR;
    }

    // Read package name
    if (eos_storage_file_seek(fp, EOS_PKG_NAME_OFFSET) != EOS_OK ||
        eos_storage_file_read(fp, header->pkg_name, EOS_PKG_NAME_LEN_MAX) != EOS_PKG_NAME_LEN_MAX)
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Failed to read package name");
        return EOS_ERR_FILE_ERROR;
    }
    header->pkg_name[EOS_PKG_NAME_LEN_MAX - 1] = '\0';

    // Read package ID
    if (eos_storage_file_seek(fp, EOS_PKG_ID_OFFSET) != EOS_OK ||
        eos_storage_file_read(fp, header->pkg_id, EOS_PKG_ID_LEN_MAX) != EOS_PKG_ID_LEN_MAX)
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Failed to read package id");
        return EOS_ERR_FILE_ERROR;
    }
    header->pkg_id[EOS_PKG_ID_LEN_MAX - 1] = '\0';

    // Read package version
    if (eos_storage_file_seek(fp, EOS_PKG_VERSION_OFFSET) != EOS_OK ||
        eos_storage_file_read(fp, header->pkg_version, EOS_PKG_VERSION_LEN_MAX) != EOS_PKG_VERSION_LEN_MAX)
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Failed to read package version");
        return EOS_ERR_FILE_ERROR;
    }
    header->pkg_version[EOS_PKG_VERSION_LEN_MAX - 1] = '\0';

    // Read min_api_level
    if (eos_storage_file_seek(fp, EOS_PKG_MIN_API_OFFSET) != EOS_OK ||
        eos_storage_file_read(fp, &header->min_api_level, sizeof(uint16_t)) != sizeof(uint16_t))
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Failed to read min_api_level");
        return EOS_ERR_FILE_ERROR;
    }

    // Read target_api_level
    if (eos_storage_file_seek(fp, EOS_PKG_TARGET_API_OFFSET) != EOS_OK ||
        eos_storage_file_read(fp, &header->target_api_level, sizeof(uint16_t)) != sizeof(uint16_t))
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Failed to read target_api_level");
        return EOS_ERR_FILE_ERROR;
    }

    // Read file_count
    if (eos_storage_file_seek(fp, EOS_PKG_FILE_COUNT_OFFSET) != EOS_OK ||
        eos_storage_file_read(fp, &header->file_count, sizeof(uint32_t)) != sizeof(uint32_t))
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Failed to read file count");
        return EOS_ERR_FILE_ERROR;
    }

    // Read reserved field
    if (eos_storage_file_seek(fp, EOS_PKG_RESERVED_OFFSET) != EOS_OK ||
        eos_storage_file_read(fp, &header->reserved, sizeof(uint32_t)) != sizeof(uint32_t))
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Failed to read reserved field");
        return EOS_ERR_FILE_ERROR;
    }
    eos_storage_file_close(fp);
    EOS_LOG_D("\n"
              "=============================\n"
              "Magic: %s | Pkg Name: %s | Pkg Version: %s\n"
              "File Count: %d | Table Offset: %d\n"
              "=============================",
              header->magic, header->pkg_name,
              header->pkg_version, header->file_count,
              EOS_PKG_HEADER_LENGTH);
    return EOS_OK;
}

eos_result_t eos_pkg_mgr_unpack(const char *pkg_path, const char *output_path, const script_pkg_type_t pkg_type)
{
    // 打开包文件
    eos_file_t fp = eos_storage_file_open_read(pkg_path);
    if (fp == EOS_FILE_INVALID)
    {
        EOS_LOG_E("Failed to open package file");
        return EOS_ERR_FILE_ERROR;
    }

    // 读取包头
    eos_pkg_header_t header;
    if (eos_pkg_read_header(pkg_path, &header) != EOS_OK)
    {
        EOS_LOG_E("Failed to read header");
        eos_storage_file_close(fp);
        return EOS_FAILED;
    }

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
        eos_storage_file_close(fp);
        EOS_LOG_E("Invalid magic number");
        return EOS_ERR_FILE_ERROR;
    }

    // 检查包类型是否匹配
    if (unpack_type != pkg_type)
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Package type mismatch: expected %d, got %d", pkg_type, unpack_type);
        return EOS_ERR_VALUE_MISMATCH;
    }

    // 获取文件大小
    uint32_t file_size = 0;
    if (eos_storage_file_size(fp, &file_size) != EOS_OK)
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Failed to get file size");
        return EOS_ERR_FILE_ERROR;
    }

    // Seek to the file table position (immediately after the file header)
    if (eos_storage_file_seek(fp, EOS_PKG_TABLE_OFFSET) != EOS_OK)
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Failed to seek to file table at offset %u", EOS_PKG_TABLE_OFFSET);
        return EOS_ERR_FILE_ERROR;
    }

    // 创建输出目录
    if (eos_storage_mkdir_recursive(output_path) != EOS_OK)
    {
        eos_storage_file_close(fp);
        EOS_LOG_E("Failed to create output directory");
        return EOS_ERR_FILE_ERROR;
    }

    // Track the current file table offset instead of querying the underlying file position
    uint32_t table_pos = EOS_PKG_TABLE_OFFSET;

    // Process each file entry
    for (uint32_t i = 0; i < header.file_count; i++)
    {
        // Read the file name length
        uint32_t name_len;
        if (eos_storage_file_read(fp, &name_len, sizeof(uint32_t)) != sizeof(uint32_t))
        {
            eos_storage_file_close(fp);
            EOS_LOG_E("Failed to read name length for entry %u", i);
            return EOS_ERR_FILE_ERROR;
        }
        table_pos += sizeof(uint32_t);

        // Validate the file name length
        if (name_len > EOS_FS_PATH_MAX)
        {
            eos_storage_file_close(fp);
            EOS_LOG_E("Name length %u too long for entry %u", name_len, i);
            return EOS_ERR_FILE_ERROR;
        }

        // Read the file name
        char name[EOS_FS_PATH_MAX];
        if (eos_storage_file_read(fp, name, name_len) != (int)name_len)
        {
            eos_storage_file_close(fp);
            EOS_LOG_E("Failed to read name for entry %u", i);
            return EOS_ERR_FILE_ERROR;
        }
        name[name_len] = '\0';
        table_pos += name_len;

        // Read the remaining entry fields
        uint32_t is_dir, offset, size;
        if (eos_storage_file_read(fp, &is_dir, sizeof(uint32_t)) != sizeof(uint32_t) ||
            eos_storage_file_read(fp, &offset, sizeof(uint32_t)) != sizeof(uint32_t) ||
            eos_storage_file_read(fp, &size, sizeof(uint32_t)) != sizeof(uint32_t))
        {
            eos_storage_file_close(fp);
            EOS_LOG_E("Failed to read entry fields for %s", name);
            return EOS_ERR_FILE_ERROR;
        }
        table_pos += sizeof(uint32_t) * 3;

        // Compute the offset of the next entry in the table
        uint32_t next_entry_pos = table_pos;

        // Build the full output path
        char full_path[EOS_FS_PATH_MAX] = {0};
        snprintf(full_path, sizeof(full_path), "%s/%s", output_path, name);

        if (is_dir)
        {
            // Create the directory
            if (eos_storage_mkdir_recursive(full_path) != EOS_OK)
            {
                eos_storage_file_close(fp);
                EOS_LOG_E("Failed to create directory: %s", full_path);
                return EOS_ERR_FILE_ERROR;
            }
            EOS_LOG_D("Created directory: %s", full_path);
        }
        else
        {
            // Validate the file offset and size
            if (offset < EOS_PKG_TABLE_OFFSET || offset >= file_size)
            {
                eos_storage_file_close(fp);
                EOS_LOG_E("Invalid file offset: %u for %s", offset, name);
                return EOS_ERR_FILE_ERROR;
            }

            if (offset + size > file_size)
            {
                eos_storage_file_close(fp);
                EOS_LOG_E("File size overflow: %u+%u=%u for %s",
                          offset, size, offset + size, name);
                return EOS_ERR_FILE_ERROR;
            }

            // Ensure the parent directory exists
            char *last_slash = strrchr(full_path, '/');
            if (last_slash)
            {
                *last_slash = '\0';
                if (eos_storage_mkdir_recursive(full_path) != EOS_OK)
                {
                    eos_storage_file_close(fp);
                    EOS_LOG_E("Failed to create parent directory: %s", full_path);
                    return EOS_ERR_FILE_ERROR;
                }
                *last_slash = '/';
            }

            // Create the file and write data
            eos_file_t out_fp = eos_storage_file_open_write(full_path);
            if (out_fp == EOS_FILE_INVALID)
            {
                eos_storage_file_close(fp);
                EOS_LOG_E("Failed to create file: %s", full_path);
                return EOS_ERR_FILE_ERROR;
            }

            // Seek to the file data
            if (eos_storage_file_seek(fp, offset) != EOS_OK)
            {
                eos_storage_file_close(out_fp);
                eos_storage_file_close(fp);
                EOS_LOG_E("Failed to seek to file data for %s", name);
                return EOS_ERR_FILE_ERROR;
            }

            // Read and write the file in chunks
            uint32_t remaining = size;
            uint8_t buffer[EOS_PKG_READ_BLOCK];
            while (remaining > 0)
            {
                size_t to_read = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
                int r = eos_storage_file_read(fp, buffer, to_read);
                if (r <= 0)
                {
                    eos_storage_file_close(out_fp);
                    eos_storage_file_close(fp);
                    EOS_LOG_E("Failed to read file data for %s", name);
                    return EOS_ERR_FILE_ERROR;
                }
                if (eos_storage_file_write(out_fp, buffer, r) != r)
                {
                    eos_storage_file_close(out_fp);
                    eos_storage_file_close(fp);
                    EOS_LOG_E("Failed to write file data for %s", name);
                    return EOS_ERR_FILE_ERROR;
                }
                remaining -= r;
            }

            eos_storage_file_close(out_fp);
            EOS_LOG_D("Created file: %s (size: %u bytes)", full_path, size);

            // Restore the file table position for the next entry before reading the next file name
            if (eos_storage_file_seek(fp, next_entry_pos) != EOS_OK)
            {
                eos_storage_file_close(fp);
                EOS_LOG_E("Failed to restore table position after extracting %s", name);
                return EOS_ERR_FILE_ERROR;
            }
        }

        if (is_dir)
        {
            if (eos_storage_file_seek(fp, next_entry_pos) != EOS_OK)
            {
                eos_storage_file_close(fp);
                EOS_LOG_E("Failed to seek to next table entry after creating dir %s", full_path);
                return EOS_ERR_FILE_ERROR;
            }
        }

        table_pos = next_entry_pos;
    }

    eos_storage_file_close(fp);
    return EOS_OK;
}
