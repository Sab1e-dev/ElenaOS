/**
 * @file esh_builtin.c
 * @brief Built-in ESH commands
 */

#include "esh.h"
#include "builtin/esh_builtin_commands.h"

/* Includes ---------------------------------------------------*/
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "eos_core.h"
#include "eos_mem.h"
#include "eos_service_storage.h"
#include "eos_version.h"
#include "esh_ymodem.h"

/* Macros and Definitions -------------------------------------*/
#define _ESH_PATH_SEPARATOR '/'

/* Variables --------------------------------------------------*/

/* Function Prototypes ----------------------------------------*/
static bool _esh_resolve_path(const esh_t *esh, const char *path, char *resolved, size_t resolved_size);
static bool _esh_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size);
static bool _esh_append_path_component(char *path,
                                       size_t path_size,
                                       size_t *path_length,
                                       const char *component,
                                       size_t component_length);

/* Function Implementations -----------------------------------*/

static bool _esh_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size)
{
    static const char *const units[] = {"bytes", "KB", "MB", "GB", "TB", "PB", "EB"};
    static const uint64_t unit_sizes[] = {1ULL,
                                          1024ULL,
                                          1024ULL * 1024ULL,
                                          1024ULL * 1024ULL * 1024ULL,
                                          1024ULL * 1024ULL * 1024ULL * 1024ULL,
                                          1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL,
                                          1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL};
    size_t unit_index = 0U;
    uint64_t whole;
    uint64_t fraction;
    int written;

    if (!buffer || buffer_size == 0U)
    {
        return false;
    }

    while (unit_index + 1U < sizeof(unit_sizes) / sizeof(unit_sizes[0]) && bytes >= unit_sizes[unit_index + 1U])
    {
        unit_index++;
    }

    if (unit_index == 0U)
    {
        written = snprintf(buffer, buffer_size, "%" PRIu64 " bytes", bytes);
    }
    else
    {
        whole = bytes / unit_sizes[unit_index];
        fraction = (uint64_t)(((double)(bytes % unit_sizes[unit_index]) * 100.0) / (double)unit_sizes[unit_index]);
        written = snprintf(buffer,
                           buffer_size,
                           "%" PRIu64 ".%02" PRIu64 " %s (%" PRIu64 " bytes)",
                           whole,
                           fraction,
                           units[unit_index],
                           bytes);
    }

    return written >= 0 && (size_t)written < buffer_size;
}

bool esh_builtin_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size)
{
    return _esh_format_bytes(bytes, buffer, buffer_size);
}

static bool _esh_append_path_component(char *path,
                                       size_t path_size,
                                       size_t *path_length,
                                       const char *component,
                                       size_t component_length)
{
    size_t separator_length = (*path_length > 1U) ? 1U : 0U;

    if (*path_length + separator_length + component_length + 1U > path_size)
    {
        return false;
    }

    if (separator_length > 0U)
    {
        path[(*path_length)++] = _ESH_PATH_SEPARATOR;
    }

    memcpy(path + *path_length, component, component_length);
    *path_length += component_length;
    path[*path_length] = '\0';
    return true;
}

static void _esh_pop_path_component(char *path, size_t *path_length)
{
    size_t index;

    if (*path_length <= 1U)
    {
        return;
    }

    for (index = *path_length; index > 1U; index--)
    {
        if (path[index - 1U] == _ESH_PATH_SEPARATOR)
        {
            path[index - 1U] = '\0';
            *path_length = index - 1U;
            return;
        }
    }

    path[1] = '\0';
    *path_length = 1U;
}

static bool _esh_resolve_path(const esh_t *esh, const char *path, char *resolved, size_t resolved_size)
{
    const char *component_start;
    const char *cursor;
    size_t base_length;
    size_t component_length;
    size_t resolved_length;

    if (!esh || !path || !resolved || resolved_size < 2U)
    {
        return false;
    }

    if (path[0] == _ESH_PATH_SEPARATOR)
    {
        resolved[0] = _ESH_PATH_SEPARATOR;
        resolved[1] = '\0';
        resolved_length = 1U;
    }
    else
    {
        base_length = strlen(esh->cwd);
        if (base_length == 0U || base_length + 1U > resolved_size)
        {
            return false;
        }

        memcpy(resolved, esh->cwd, base_length + 1U);
        resolved_length = base_length;
    }

    cursor = path;
    while (*cursor != '\0')
    {
        while (*cursor == _ESH_PATH_SEPARATOR)
        {
            cursor++;
        }

        if (*cursor == '\0')
        {
            break;
        }

        component_start = cursor;
        while (*cursor != '\0' && *cursor != _ESH_PATH_SEPARATOR)
        {
            cursor++;
        }

        component_length = (size_t)(cursor - component_start);
        if (component_length == 1U && component_start[0] == '.')
        {
            continue;
        }

        if (component_length == 2U && component_start[0] == '.' && component_start[1] == '.')
        {
            _esh_pop_path_component(resolved, &resolved_length);
            continue;
        }

        if (!_esh_append_path_component(resolved, resolved_size, &resolved_length, component_start, component_length))
        {
            return false;
        }
    }

    return true;
}

bool esh_builtin_resolve_path(const esh_t *esh, const char *path, char *resolved, size_t resolved_size)
{
    return _esh_resolve_path(esh, path, resolved, resolved_size);
}

static int _esh_cmd_pwd(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!ctx || !ctx->esh)
    {
        return EOS_ERR_INVALID_ARG;
    }

    return (int)esh_printf(ctx, "%s\r\n", ctx->esh->cwd);
}

static int _esh_cmd_cd(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    const char *target;
    char resolved[EOS_FS_PATH_MAX];

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc > 2)
    {
        return (int)esh_printf(ctx, "cd: usage: cd [directory]\r\n");
    }

    target = (argc == 1) ? "/" : argv[1];
    if (!_esh_resolve_path(ctx->esh, target, resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "cd: path too long: %s\r\n", target);
    }

    if (!eos_storage_is_dir(resolved))
    {
        return (int)esh_printf(ctx, "cd: no such directory: %s\r\n", target);
    }

    memcpy(ctx->esh->cwd, resolved, strlen(resolved) + 1U);
    return EOS_OK;
}

static int _esh_cmd_echo(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    int index;
    eos_result_t result;

    if (!ctx || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    for (index = 1; index < argc; index++)
    {
        result = esh_printf(ctx, "%s%s", index == 1 ? "" : " ", argv[index]);
        if (result != EOS_OK)
        {
            return (int)result;
        }
    }

    return (int)esh_printf(ctx, "\r\n");
}

static int _esh_cmd_ls(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    const char *target;
    char resolved[EOS_FS_PATH_MAX];
    char name[EOS_FS_PATH_MAX];
    char entry_path[EOS_FS_PATH_MAX];
    eos_dir_t directory;
    eos_result_t result = EOS_OK;

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc > 2)
    {
        return (int)esh_printf(ctx, "ls: usage: ls [path]\r\n");
    }

    target = (argc == 1) ? ctx->esh->cwd : argv[1];
    if (!_esh_resolve_path(ctx->esh, target, resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "ls: path too long: %s\r\n", target);
    }

    if (eos_storage_is_file(resolved))
    {
        return (int)esh_printf(ctx, "%s\r\n", resolved);
    }

    if (!eos_storage_is_dir(resolved))
    {
        return (int)esh_printf(ctx, "ls: no such file or directory: %s\r\n", target);
    }

    directory = eos_storage_dir_open(resolved);
    if (!directory)
    {
        return (int)esh_printf(ctx, "ls: cannot open directory: %s\r\n", target);
    }

    while (eos_storage_dir_read(directory, name, sizeof(name)) == EOS_OK)
    {
        size_t resolved_length = strlen(resolved);

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        {
            continue;
        }

        memcpy(entry_path, resolved, resolved_length + 1U);
        if (!_esh_append_path_component(entry_path, sizeof(entry_path), &resolved_length, name, strlen(name)))
        {
            result = esh_printf(ctx, "ls: path too long: %s\r\n", name);
            break;
        }

        result = esh_printf(ctx, "%s%s\r\n", name, eos_storage_is_dir(entry_path) ? "/" : "");
        if (result != EOS_OK)
        {
            break;
        }
    }

    eos_storage_dir_close(directory);
    return (int)result;
}

static int _esh_cat_file(esh_cmd_ctx_t *ctx, const char *path)
{
    char resolved[EOS_FS_PATH_MAX];
    char buffer[ESH_PRINTF_BUFFER_SIZE];
    eos_file_t file;
    ssize_t read_length;
    eos_result_t result = EOS_OK;

    if (!_esh_resolve_path(ctx->esh, path, resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "cat: path too long: %s\r\n", path);
    }

    if (eos_storage_is_dir(resolved))
    {
        return (int)esh_printf(ctx, "cat: %s: is a directory\r\n", path);
    }

    file = eos_storage_file_open_read(resolved);
    if (file == EOS_FILE_INVALID)
    {
        return (int)esh_printf(ctx, "cat: no such file: %s\r\n", path);
    }

    while ((read_length = eos_storage_file_read(file, buffer, sizeof(buffer))) > 0)
    {
        result = esh_write(ctx, buffer, (size_t)read_length);
        if (result != EOS_OK)
        {
            break;
        }
    }

    eos_storage_file_close(file);
    if (result != EOS_OK)
    {
        return (int)result;
    }

    if (read_length < 0)
    {
        return (int)esh_printf(ctx, "cat: read failed: %s\r\n", path);
    }

    return EOS_OK;
}

static int _esh_cmd_cat(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    int index;
    int result;

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc < 2)
    {
        return (int)esh_printf(ctx, "cat: usage: cat file...\r\n");
    }

    for (index = 1; index < argc; index++)
    {
        result = _esh_cat_file(ctx, argv[index]);
        if (result != EOS_OK)
        {
            return result;
        }
    }

    return EOS_OK;
}

static int _esh_cmd_clear(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    static const char sequence[] = "\033[2J\033[H";

    (void)argv;

    if (!ctx || argc != 1)
    {
        return (int)esh_printf(ctx, "clear: usage: clear\r\n");
    }

    return (int)esh_write(ctx, sequence, sizeof(sequence) - 1U);
}

static int _esh_cmd_stat(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    int index;
    char resolved[EOS_FS_PATH_MAX];
    char size_text[64];
    eos_file_t file;
    uint32_t size;
    eos_result_t result;

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc < 2)
    {
        return (int)esh_printf(ctx, "stat: usage: stat <file>...\r\n");
    }

    for (index = 1; index < argc; index++)
    {
        if (!_esh_resolve_path(ctx->esh, argv[index], resolved, sizeof(resolved)))
        {
            return (int)esh_printf(ctx, "stat: path too long: %s\r\n", argv[index]);
        }

        if (eos_storage_is_dir(resolved))
        {
            result = esh_printf(ctx, "%s: directory\r\n", argv[index]);
        }
        else if (eos_storage_is_file(resolved))
        {
            file = eos_storage_file_open_read(resolved);
            if (file == EOS_FILE_INVALID)
            {
                return (int)esh_printf(ctx, "stat: cannot open: %s\r\n", argv[index]);
            }

            result = eos_storage_file_size(file, &size);
            eos_storage_file_close(file);
            if (result != EOS_OK)
            {
                return (int)esh_printf(ctx, "stat: cannot read metadata: %s\r\n", argv[index]);
            }

            if (!_esh_format_bytes(size, size_text, sizeof(size_text)))
            {
                return EOS_ERR_IO;
            }
            result = esh_printf(ctx, "%s: regular file, %s\r\n", argv[index], size_text);
        }
        else
        {
            return (int)esh_printf(ctx, "stat: no such file or directory: %s\r\n", argv[index]);
        }

        if (result != EOS_OK)
        {
            return (int)result;
        }
    }

    return EOS_OK;
}

static int _esh_copy_file(esh_cmd_ctx_t *ctx, const char *source, const char *destination)
{
    uint8_t buffer[256];
    eos_file_t source_file;
    eos_file_t destination_file;
    ssize_t read_length;
    ssize_t written;

    source_file = eos_storage_file_open_read(source);
    if (source_file == EOS_FILE_INVALID)
    {
        return (int)esh_printf(ctx, "cp: cannot open source: %s\r\n", source);
    }

    destination_file = eos_storage_file_open_write(destination);
    if (destination_file == EOS_FILE_INVALID)
    {
        eos_storage_file_close(source_file);
        return (int)esh_printf(ctx, "cp: cannot open destination: %s\r\n", destination);
    }

    while ((read_length = eos_storage_file_read(source_file, buffer, sizeof(buffer))) > 0)
    {
        written = eos_storage_file_write(destination_file, buffer, (size_t)read_length);
        if (written != read_length)
        {
            eos_storage_file_close(destination_file);
            eos_storage_file_close(source_file);
            return (int)esh_printf(ctx, "cp: write failed: %s\r\n", destination);
        }
    }

    eos_storage_file_close(destination_file);
    eos_storage_file_close(source_file);
    if (read_length < 0)
    {
        return (int)esh_printf(ctx, "cp: read failed: %s\r\n", source);
    }

    return EOS_OK;
}

static int _esh_cmd_cp(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char source[EOS_FS_PATH_MAX];
    char destination[EOS_FS_PATH_MAX];

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc != 3)
    {
        return (int)esh_printf(ctx, "cp: usage: cp <source> <destination>\r\n");
    }

    if (!_esh_resolve_path(ctx->esh, argv[1], source, sizeof(source))
        || !_esh_resolve_path(ctx->esh, argv[2], destination, sizeof(destination)))
    {
        return (int)esh_printf(ctx, "cp: path too long\r\n");
    }

    if (strcmp(source, destination) == 0)
    {
        return (int)esh_printf(ctx, "cp: source and destination are the same\r\n");
    }

    if (!eos_storage_is_file(source))
    {
        return (int)esh_printf(ctx, "cp: source is not a file: %s\r\n", argv[1]);
    }

    if (eos_storage_is_dir(destination))
    {
        return (int)esh_printf(ctx, "cp: destination is a directory: %s\r\n", argv[2]);
    }

    return _esh_copy_file(ctx, source, destination);
}

static int _esh_cmd_mv(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char source[EOS_FS_PATH_MAX];
    char destination[EOS_FS_PATH_MAX];
    eos_result_t result;

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc != 3)
    {
        return (int)esh_printf(ctx, "mv: usage: mv <source> <destination>\r\n");
    }

    if (!_esh_resolve_path(ctx->esh, argv[1], source, sizeof(source))
        || !_esh_resolve_path(ctx->esh, argv[2], destination, sizeof(destination)))
    {
        return (int)esh_printf(ctx, "mv: path too long\r\n");
    }

    if (strcmp(source, destination) == 0)
    {
        return (int)esh_printf(ctx, "mv: source and destination are the same\r\n");
    }

    if (!eos_storage_is_file(source) && !eos_storage_is_dir(source))
    {
        return (int)esh_printf(ctx, "mv: no such file or directory: %s\r\n", argv[1]);
    }

    result = eos_storage_file_move(source, destination);
    if (result != EOS_OK)
    {
        return (int)esh_printf(ctx, "mv: cannot move: %s\r\n", argv[1]);
    }

    return EOS_OK;
}

static int _esh_cmd_touch(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    int index;
    char resolved[EOS_FS_PATH_MAX];
    eos_result_t result;

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc < 2)
    {
        return (int)esh_printf(ctx, "touch: usage: touch <file>...\r\n");
    }

    for (index = 1; index < argc; index++)
    {
        if (!_esh_resolve_path(ctx->esh, argv[index], resolved, sizeof(resolved)))
        {
            return (int)esh_printf(ctx, "touch: path too long: %s\r\n", argv[index]);
        }

        result = eos_storage_create_file_if_not_exist(resolved, NULL);
        if (result != EOS_OK)
        {
            return (int)esh_printf(ctx, "touch: cannot create file: %s\r\n", argv[index]);
        }
    }

    return EOS_OK;
}

static int _esh_cmd_df(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char resolved[EOS_FS_PATH_MAX];
    const char *target;
    eos_storage_space_t space;
    uint64_t used_bytes;
    char total_text[64];
    char used_text[64];
    char free_text[64];
    eos_result_t result;

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc > 2)
    {
        return (int)esh_printf(ctx, "df: usage: df [path]\r\n");
    }

    target = argc == 1 ? ctx->esh->cwd : argv[1];
    if (!_esh_resolve_path(ctx->esh, target, resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "df: path too long: %s\r\n", target);
    }

    result = eos_storage_get_space(resolved, &space);
    if (result == EOS_ERR_DEV_OPS_NOT_SUPPORTED)
    {
        return (int)esh_printf(ctx, "df: file system capacity is not supported\r\n");
    }
    if (result != EOS_OK)
    {
        return (int)esh_printf(ctx, "df: cannot query: %s\r\n", target);
    }

    used_bytes = space.total_bytes >= space.free_bytes ? space.total_bytes - space.free_bytes : 0U;
    if (!_esh_format_bytes(space.total_bytes, total_text, sizeof(total_text))
        || !_esh_format_bytes(used_bytes, used_text, sizeof(used_text))
        || !_esh_format_bytes(space.free_bytes, free_text, sizeof(free_text)))
    {
        return EOS_ERR_IO;
    }
    result = esh_printf(ctx, "Filesystem %s\r\n", target);
    if (result != EOS_OK)
    {
        return (int)result;
    }

    result = esh_printf(ctx, "  total: %s\r\n", total_text);
    if (result != EOS_OK)
    {
        return (int)result;
    }

    result = esh_printf(ctx, "  used:  %s\r\n", used_text);
    if (result != EOS_OK)
    {
        return (int)result;
    }

    return (int)esh_printf(ctx, "  free:  %s\r\n", free_text);
}

static int _esh_cmd_free(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char used_text[64];
    char free_text[64];

    (void)argv;

    if (!ctx || argc != 1)
    {
        return (int)esh_printf(ctx, "free: usage: free\r\n");
    }

    if (!_esh_format_bytes((uint64_t)eos_mem_get_used_bytes(), used_text, sizeof(used_text))
        || !_esh_format_bytes((uint64_t)eos_mem_get_free_bytes(), free_text, sizeof(free_text)))
    {
        return EOS_ERR_IO;
    }

    return (int)esh_printf(ctx, "Memory\r\n  used: %s\r\n  free: %s\r\n", used_text, free_text);
}

static int _esh_cmd_uptime(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    uint32_t uptime_ms;
    uint32_t total_seconds;
    uint32_t days;
    uint32_t hours;
    uint32_t minutes;
    uint32_t seconds;

    (void)argv;

    if (!ctx || argc != 1)
    {
        return (int)esh_printf(ctx, "uptime: usage: uptime\r\n");
    }

    uptime_ms = eos_tick_get();
    total_seconds = uptime_ms / 1000U;
    days = total_seconds / 86400U;
    hours = (total_seconds % 86400U) / 3600U;
    minutes = (total_seconds % 3600U) / 60U;
    seconds = total_seconds % 60U;
    return (int)
        esh_printf(ctx, "up %" PRIu32 "d %" PRIu32 "h %" PRIu32 "m %" PRIu32 "s\r\n", days, hours, minutes, seconds);
}

static int _esh_cmd_hexdump(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char resolved[EOS_FS_PATH_MAX];
    char line[96];
    uint8_t buffer[16];
    eos_file_t file;
    uint32_t offset = 0U;
    ssize_t read_length;
    size_t index;
    size_t line_length;
    int written;

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc != 2)
    {
        return (int)esh_printf(ctx, "hexdump: usage: hexdump <file>\r\n");
    }

    if (!_esh_resolve_path(ctx->esh, argv[1], resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "hexdump: path too long: %s\r\n", argv[1]);
    }

    file = eos_storage_file_open_read(resolved);
    if (file == EOS_FILE_INVALID)
    {
        return (int)esh_printf(ctx, "hexdump: no such file: %s\r\n", argv[1]);
    }

    while ((read_length = eos_storage_file_read(file, buffer, sizeof(buffer))) > 0)
    {
        written = snprintf(line, sizeof(line), "%08" PRIx32 "  ", offset);
        if (written < 0 || (size_t)written >= sizeof(line))
        {
            eos_storage_file_close(file);
            return EOS_ERR_IO;
        }
        line_length = (size_t)written;

        for (index = 0U; index < sizeof(buffer); index++)
        {
            written = snprintf(&line[line_length],
                               sizeof(line) - line_length,
                               index < (size_t)read_length ? "%02x " : "   ",
                               index < (size_t)read_length ? buffer[index] : 0U);
            if (written < 0 || (size_t)written >= sizeof(line) - line_length)
            {
                eos_storage_file_close(file);
                return EOS_ERR_IO;
            }
            line_length += (size_t)written;
        }

        written = snprintf(&line[line_length], sizeof(line) - line_length, " | ");
        if (written < 0 || (size_t)written >= sizeof(line) - line_length)
        {
            eos_storage_file_close(file);
            return EOS_ERR_IO;
        }
        line_length += (size_t)written;
        for (index = 0U; index < (size_t)read_length; index++)
        {
            uint8_t byte = buffer[index];
            line[line_length++] = byte >= 0x20U && byte <= 0x7EU ? (char)byte : '.';
        }
        line[line_length++] = ' ';
        line[line_length++] = '|';
        line[line_length++] = '\r';
        line[line_length++] = '\n';
        if (esh_write(ctx, line, line_length) != EOS_OK)
        {
            eos_storage_file_close(file);
            return EOS_ERR_IO;
        }
        offset += (uint32_t)read_length;
    }

    eos_storage_file_close(file);
    if (read_length < 0)
    {
        return (int)esh_printf(ctx, "hexdump: read failed: %s\r\n", argv[1]);
    }

    return EOS_OK;
}

static int _esh_cmd_mkdir(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    int index;
    char resolved[EOS_FS_PATH_MAX];
    eos_result_t result;

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc < 2)
    {
        return (int)esh_printf(ctx, "mkdir: usage: mkdir <directory>...\r\n");
    }

    for (index = 1; index < argc; index++)
    {
        if (!_esh_resolve_path(ctx->esh, argv[index], resolved, sizeof(resolved)))
        {
            return (int)esh_printf(ctx, "mkdir: path too long: %s\r\n", argv[index]);
        }

        result = eos_storage_mkdir_recursive(resolved);
        if (result != EOS_OK)
        {
            return (int)esh_printf(ctx, "mkdir: cannot create directory: %s\r\n", argv[index]);
        }
    }

    return EOS_OK;
}

static int _esh_cmd_rm(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    bool recursive = false;
    int first_path = 1;
    int index;
    char resolved[EOS_FS_PATH_MAX];
    eos_result_t result;

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc > 1 && strcmp(argv[1], "-r") == 0)
    {
        recursive = true;
        first_path++;
    }

    if (argc <= first_path)
    {
        return (int)esh_printf(ctx, "rm: usage: rm [-r] <file>...\r\n");
    }

    for (index = first_path; index < argc; index++)
    {
        if (!_esh_resolve_path(ctx->esh, argv[index], resolved, sizeof(resolved)))
        {
            return (int)esh_printf(ctx, "rm: path too long: %s\r\n", argv[index]);
        }

        if (!eos_storage_is_file(resolved) && !eos_storage_is_dir(resolved))
        {
            return (int)esh_printf(ctx, "rm: no such file or directory: %s\r\n", argv[index]);
        }

        if (!recursive && eos_storage_is_dir(resolved))
        {
            return (int)esh_printf(ctx, "rm: cannot remove directory without -r: %s\r\n", argv[index]);
        }

        result = recursive ? eos_storage_rm_recursive(resolved) : eos_storage_file_remove(resolved);
        if (result != EOS_OK)
        {
            return (int)esh_printf(ctx, "rm: cannot remove: %s\r\n", argv[index]);
        }
    }

    return EOS_OK;
}

static int _esh_cmd_ymodem(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char resolved[EOS_FS_PATH_MAX];

    if (!ctx || !ctx->esh || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc != 3 || (strcmp(argv[1], "send") != 0 && strcmp(argv[1], "recv") != 0))
    {
        return (int)esh_printf(ctx, "ymodem: usage: ymodem send <file> | ymodem recv <file>\r\n");
    }

    if (!_esh_resolve_path(ctx->esh, argv[2], resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "ymodem: path too long: %s\r\n", argv[2]);
    }

    if (strcmp(argv[1], "send") == 0)
    {
        return (int)esh_ymodem_start_send(ctx, resolved);
    }

    return (int)esh_ymodem_start_receive(ctx, resolved);
}

static int _esh_cmd_version(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    return (int)esh_printf(ctx, "ElenixOS kernel v" ELENIX_OS_VERSION_FULL "\r\n");
}

static int _esh_cmd_help(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    const esh_command_t *command;

    (void)argv;

    if (!ctx || !ctx->esh)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc > 1)
    {
        for (command = ctx->esh->commands_begin; command < ctx->esh->commands_end; command++)
        {
            if (command->name && strcmp(command->name, argv[1]) == 0)
            {
                return esh_printf(ctx, "%s - %s\r\n", command->name, command->description ? command->description : "");
            }
        }

        return esh_printf(ctx, "esh: unknown command: %s\r\n", argv[1]);
    }

    for (command = ctx->esh->commands_begin; command < ctx->esh->commands_end; command++)
    {
        if (command->name && command->handler)
        {
            eos_result_t result =
                esh_printf(ctx, "%-12s %s\r\n", command->name, command->description ? command->description : "");

            if (result != EOS_OK)
            {
                return result;
            }
        }
    }

    return EOS_OK;
}

#define ESH_BUILTIN_COMMANDS(_)                                       \
    _(help, _esh_cmd_help, "list available commands")                 \
    _(version, _esh_cmd_version, "show ElenixOS kernel version")      \
    _(pwd, _esh_cmd_pwd, "print the current directory")               \
    _(cd, _esh_cmd_cd, "change the current directory")                \
    _(ls, _esh_cmd_ls, "list files and directories")                  \
    _(echo, _esh_cmd_echo, "write arguments to the terminal")         \
    _(cat, _esh_cmd_cat, "print file contents")                       \
    _(clear, _esh_cmd_clear, "clear the terminal")                    \
    _(stat, _esh_cmd_stat, "show file information")                   \
    _(cp, _esh_cmd_cp, "copy files")                                  \
    _(mv, _esh_cmd_mv, "move or rename files")                        \
    _(touch, _esh_cmd_touch, "create empty files")                    \
    _(df, _esh_cmd_df, "show file system space")                      \
    _(free, _esh_cmd_free, "show memory usage")                       \
    _(uptime, _esh_cmd_uptime, "show system uptime")                  \
    _(hexdump, _esh_cmd_hexdump, "display file contents in hex")      \
    _(mkdir, _esh_cmd_mkdir, "create directories")                    \
    _(rm, _esh_cmd_rm, "remove files or directories")                 \
    _(ymodem, _esh_cmd_ymodem, "send or receive a file with YMODEM")  \
    _(log, esh_builtin_cmd_log, "view and configure system logs")     \
    _(mem, esh_builtin_cmd_mem, "show memory and runtime heaps")      \
    _(stack, esh_builtin_cmd_stack, "show stack diagnostics")         \
    _(crashlog, esh_builtin_cmd_crashlog, "show last script crash")   \
    _(sensor, esh_builtin_cmd_sensor, "diagnose sensors")             \
    _(battery, esh_builtin_cmd_battery, "show battery diagnostics")   \
    _(power, esh_builtin_cmd_power, "show power diagnostics")         \
    _(display, esh_builtin_cmd_display, "show display diagnostics")   \
    _(touchdiag, esh_builtin_cmd_touch, "diagnose touch input")       \
    _(time, esh_builtin_cmd_time, "show system time")                 \
    _(vibrator, esh_builtin_cmd_vibrator, "test the vibrator")        \
    _(audio, esh_builtin_cmd_audio, "diagnose audio devices")         \
    _(ble, esh_builtin_cmd_ble, "show or control Bluetooth")          \
    _(apps, esh_builtin_cmd_apps, "list installed applications")      \
    _(recent, esh_builtin_cmd_recent, "list recent applications")     \
    _(app, esh_builtin_cmd_app, "inspect or control an application")  \
    _(config, esh_builtin_cmd_config, "inspect system configuration") \
    _(state, esh_builtin_cmd_state, "inspect system state")           \
    _(pkg, esh_builtin_cmd_pkg, "manage application packages")        \
    _(find, esh_builtin_cmd_find, "find files recursively")           \
    _(grep, esh_builtin_cmd_grep, "search text in a file")            \
    _(head, esh_builtin_cmd_head, "show the beginning of a file")     \
    _(tail, esh_builtin_cmd_tail, "show the end of a file")           \
    _(wc, esh_builtin_cmd_wc, "count file contents")                  \
    _(crc32, esh_builtin_cmd_crc32, "calculate a CRC32 checksum")     \
    _(sha256, esh_builtin_cmd_sha256, "calculate a SHA256 checksum")

#define _ESH_EXPORT_BUILTIN(name, handler, description) ESH_CMD_EXPORT(name, handler, description);
ESH_BUILTIN_COMMANDS(_ESH_EXPORT_BUILTIN)
#undef _ESH_EXPORT_BUILTIN

#define _ESH_DEFINE_BUILTIN(name, handler, description) {#name, handler, description},
static const esh_command_t _esh_builtin_commands[] = {ESH_BUILTIN_COMMANDS(_ESH_DEFINE_BUILTIN)};
#undef _ESH_DEFINE_BUILTIN
#undef ESH_BUILTIN_COMMANDS

const esh_command_t *_esh_builtin_command_begin(void)
{
    return _esh_builtin_commands;
}

const esh_command_t *_esh_builtin_command_end(void)
{
    return _esh_builtin_commands + (sizeof(_esh_builtin_commands) / sizeof(_esh_builtin_commands[0]));
}
