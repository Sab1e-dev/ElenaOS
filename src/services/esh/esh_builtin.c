/**
 * @file esh_builtin.c
 * @brief Built-in ESH commands
 */

#include "esh.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include "eos_service_storage.h"
#include "eos_version.h"
#include "esh_ymodem.h"

/* Macros and Definitions -------------------------------------*/
#define _ESH_PATH_SEPARATOR '/'

/* Variables --------------------------------------------------*/

/* Function Prototypes ----------------------------------------*/
static bool _esh_resolve_path(const esh_t *esh, const char *path, char *resolved, size_t resolved_size);
static bool _esh_append_path_component(char *path,
                                       size_t path_size,
                                       size_t *path_length,
                                       const char *component,
                                       size_t component_length);

/* Function Implementations -----------------------------------*/

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

#define ESH_BUILTIN_COMMANDS(_)                                  \
    _(help, _esh_cmd_help, "list available commands")            \
    _(version, _esh_cmd_version, "show ElenixOS kernel version") \
    _(pwd, _esh_cmd_pwd, "print the current directory")          \
    _(cd, _esh_cmd_cd, "change the current directory")           \
    _(ls, _esh_cmd_ls, "list files and directories")             \
    _(echo, _esh_cmd_echo, "write arguments to the terminal")    \
    _(cat, _esh_cmd_cat, "print file contents")                  \
    _(mkdir, _esh_cmd_mkdir, "create directories")               \
    _(rm, _esh_cmd_rm, "remove files or directories")            \
    _(ymodem, _esh_cmd_ymodem, "send or receive a file with YMODEM")

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
