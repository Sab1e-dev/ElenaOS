/**
 * @file esh_builtin_system.c
 * @brief System and application ESH commands
 */

#include "esh_builtin_commands.h"

/* Includes ---------------------------------------------------*/
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "eos_app.h"
#include "eos_app_list.h"
#include "eos_config.h"
#include "eos_pkg_mgr.h"
#include "eos_recent_apps.h"
#include "eos_service_config.h"
#include "eos_service_state.h"
#include "eos_storage_paths.h"
#include "eos_service_storage.h"
#include "eos_mem.h"
#include "spm.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Prototypes ----------------------------------------*/

/* Function Implementations -----------------------------------*/

static const char *_program_state_name(script_program_state_t state)
{
    switch (state)
    {
        case SCRIPT_PROGRAM_STATE_ACTIVE:
            return "active";
        case SCRIPT_PROGRAM_STATE_SUSPENDED:
            return "suspended";
        case SCRIPT_PROGRAM_STATE_STOPPING:
            return "stopping";
        default:
            return "terminated";
    }
}

static const char *_json_type_name(const cJSON *item)
{
    if (cJSON_IsBool(item))
    {
        return "bool";
    }
    if (cJSON_IsNumber(item))
    {
        return "number";
    }
    if (cJSON_IsString(item))
    {
        return "string";
    }
    if (cJSON_IsArray(item))
    {
        return "array";
    }
    if (cJSON_IsObject(item))
    {
        return "object";
    }
    return "null";
}

static eos_result_t _write_text(esh_cmd_ctx_t *ctx, const char *text)
{
    size_t length;
    size_t offset = 0U;
    size_t chunk;

    if (!ctx || !text)
    {
        return EOS_ERR_INVALID_ARG;
    }

    length = strlen(text);
    while (offset < length)
    {
        chunk = length - offset;
        if (chunk > 64U)
        {
            chunk = 64U;
        }
        if (esh_write(ctx, text + offset, chunk) != EOS_OK)
        {
            return EOS_ERR_IO;
        }
        offset += chunk;
    }

    return esh_write(ctx, "\r\n", 2U);
}

#if EOS_COMPILE_MODE == EOS_DEBUG

static int _esh_js_eval(esh_cmd_ctx_t *ctx, const char *source, size_t source_length)
{
    char result[512];
    bool result_is_undefined = false;
    eos_result_t status;

    status = spm_console_eval(source, source_length, result, sizeof(result), &result_is_undefined);
    if (status != EOS_OK)
    {
        if (result[0] != '\0')
            return (int)esh_printf(ctx, "js: %s (error=%d)\r\n", result, status);
        return (int)esh_printf(ctx, "js: eval failed (error=%d)\r\n", status);
    }

    /* Match interactive language shells: calls such as console.log() may
     * produce their own output while returning undefined. Do not echo the
     * implementation detail as a second, noisy line. A string whose content
     * is "undefined" is still a meaningful result and must be printed. */
    if (result_is_undefined)
        return EOS_OK;

    return (int)_write_text(ctx, result);
}

static int _esh_js_join_source(char *buffer, size_t buffer_size, int argc, char *argv[], int first)
{
    size_t length = 0U;
    int index;

    if (!buffer || buffer_size == 0U || !argv || first >= argc)
        return EOS_ERR_INVALID_ARG;

    for (index = first; index < argc; index++)
    {
        size_t part_length = strlen(argv[index]);
        size_t separator = index == first ? 0U : 1U;
        if (length + separator + part_length + 1U > buffer_size)
            return EOS_ERR_PATH_TOO_LONG;
        if (separator != 0U)
            buffer[length++] = ' ';
        memcpy(buffer + length, argv[index], part_length);
        length += part_length;
    }
    buffer[length] = '\0';
    return (int)length;
}

int esh_builtin_cmd_js(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char source[ESH_LINE_MAX];
    char resolved[EOS_FS_PATH_MAX];
    char *file_source;
    eos_file_t file;
    uint32_t file_size = 0U;
    int source_length;
    eos_result_t status;

    if (!ctx || !argv || argc < 2)
        return (int)esh_printf(ctx, "js: usage: js eval <code>|file <path>\r\n");

    if (strcmp(argv[1], "eval") == 0)
    {
        source_length = _esh_js_join_source(source, sizeof(source), argc, argv, 2);
        if (source_length < 0)
            return (int)esh_printf(ctx, "js: code is empty or too long\r\n");
        return _esh_js_eval(ctx, source, (size_t)source_length);
    }

    if (strcmp(argv[1], "file") != 0 || argc != 3)
        return (int)esh_printf(ctx, "js: usage: js eval <code>|file <path>\r\n");

    if (!esh_builtin_resolve_path(ctx->esh, argv[2], resolved, sizeof(resolved)))
        return (int)esh_printf(ctx, "js: path too long: %s\r\n", argv[2]);
    if (!eos_storage_is_file(resolved))
        return (int)esh_printf(ctx, "js: no such file: %s\r\n", argv[2]);

    file = eos_storage_file_open_read(resolved);
    if (file == EOS_FILE_INVALID)
        return (int)esh_printf(ctx, "js: cannot open: %s\r\n", argv[2]);
    status = eos_storage_file_size(file, &file_size);
    eos_storage_file_close(file);
    if (status != EOS_OK)
        return (int)esh_printf(ctx, "js: cannot stat: %s\r\n", argv[2]);
    if (file_size == 0U || file_size > SCRIPT_ENGINE_EVAL_SOURCE_MAX)
        return (int)esh_printf(ctx, "js: file size must be 1..%u bytes\r\n", SCRIPT_ENGINE_EVAL_SOURCE_MAX);

    file_source = eos_storage_read_file(resolved);
    if (!file_source)
        return (int)esh_printf(ctx, "js: cannot read: %s\r\n", argv[2]);
    status = (eos_result_t)_esh_js_eval(ctx, file_source, file_size);
    eos_free(file_source);
    return (int)status;
}

#endif /* EOS_COMPILE_MODE == EOS_DEBUG */

static int _print_json_item(esh_cmd_ctx_t *ctx, const char *key, cJSON *item)
{
    char *text;

    if (!item)
    {
        return (int)esh_printf(ctx, "not found: %s\r\n", key);
    }

    text = cJSON_PrintUnformatted(item);
    if (!text)
    {
        return EOS_ERR_MEM;
    }

    if (esh_printf(ctx, "%s (%s): ", key, _json_type_name(item)) != EOS_OK || _write_text(ctx, text) != EOS_OK)
    {
        cJSON_free(text);
        return EOS_ERR_IO;
    }
    cJSON_free(text);
    return EOS_OK;
}

int esh_builtin_cmd_apps(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    uint32_t count;
    uint32_t index;

    (void)argv;

    if (!ctx || argc != 1)
    {
        return (int)esh_printf(ctx, "apps: usage: apps\r\n");
    }

    count = eos_app_get_installed();
    if (count == 0U)
    {
        return (int)esh_printf(ctx, "apps: none\r\n");
    }

    for (index = 0U; index < count; index++)
    {
        const char *id = eos_app_list_get_id(index);
        script_program_t *program = id ? spm_get_program_by_id_any_state(id) : NULL;
        const char *running_system_id = eos_app_list_get_running_system_id();

        if (esh_printf(ctx,
                       "%s state=%s\r\n",
                       id ? id : "(unknown)",
                       program
                           ? _program_state_name(program->state)
                           : (running_system_id && id && strcmp(running_system_id, id) == 0 ? "active" : "not-running"))
            != EOS_OK)
        {
            return EOS_ERR_IO;
        }
    }

    return EOS_OK;
}

int esh_builtin_cmd_recent(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    eos_recent_app_entry_t *entry;

    (void)argv;

    if (!ctx || argc != 1)
    {
        return (int)esh_printf(ctx, "recent: usage: recent\r\n");
    }

    if (eos_recent_apps_count() == 0U)
    {
        return (int)esh_printf(ctx, "recent: none\r\n");
    }

    for (entry = eos_recent_apps_get_head(); entry; entry = eos_recent_apps_get_next(entry))
    {
        if (esh_printf(ctx,
                       "%s name=%s last=%" PRIu32 " mem=%" PRIu32 " bytes depth=%" PRIu32 "\r\n",
                       entry->app_id,
                       entry->app_name,
                       entry->last_used_tick,
                       entry->est_mem_bytes,
                       entry->saved_stack_depth)
            != EOS_OK)
        {
            return EOS_ERR_IO;
        }
    }

    return EOS_OK;
}

int esh_builtin_cmd_app(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    script_program_t *program;
    eos_result_t result;

    if (!ctx || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc == 2 && strcmp(argv[1], "recent") == 0)
    {
        return esh_builtin_cmd_recent(ctx, 1, argv);
    }

    if (argc != 3)
    {
        return (int)esh_printf(ctx, "app: usage: app info|start|stop|restart <id>\r\n");
    }

    if (strcmp(argv[1], "info") == 0)
    {
        program = spm_get_program_by_id_any_state(argv[2]);
        if (!program)
        {
            const char *running_system_id = eos_app_list_get_running_system_id();
            if (running_system_id && strcmp(running_system_id, argv[2]) == 0)
            {
                return (
                    int)esh_printf(ctx, "%s state=active type=system name=%s version=builtin\r\n", argv[2], argv[2]);
            }
            return (int)esh_printf(ctx, "app: not running: %s\r\n", argv[2]);
        }
        return (int)esh_printf(ctx,
                               "%s state=%s type=%d name=%s version=%s\r\n",
                               argv[2],
                               _program_state_name(program->state),
                               program->type,
                               program->script.name ? program->script.name : "",
                               program->script.version ? program->script.version : "");
    }

    if (strcmp(argv[1], "start") == 0 || strcmp(argv[1], "restart") == 0)
    {
        result = eos_app_launch_immediately(argv[2]);
    }
    else if (strcmp(argv[1], "stop") == 0)
    {
        result = spm_app_stop_by_id(argv[2]);
    }
    else
    {
        return (int)esh_printf(ctx, "app: unknown action: %s\r\n", argv[1]);
    }

    return result == EOS_OK ? EOS_OK : (int)esh_printf(ctx, "app: operation failed: %s\r\n", argv[2]);
}

static int _config_set_value(const char *key, const char *value)
{
    char *end;
    double number;

    if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0)
    {
        return (int)eos_config_set_bool(key, strcmp(value, "true") == 0);
    }

    number = strtod(value, &end);
    if (*value != '\0' && *end == '\0')
    {
        return (int)eos_config_set_number(key, number);
    }

    return (int)eos_config_set_string(key, value);
}

int esh_builtin_cmd_config(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char *content;
    cJSON *item;
    eos_result_t result;

    if (!ctx || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc == 2 && strcmp(argv[1], "list") == 0)
    {
        content = eos_storage_read_file(EOS_CONFIG_FILE_PATH);
        if (!content)
        {
            return (int)esh_printf(ctx, "config: cannot read configuration\r\n");
        }
        result = _write_text(ctx, content);
        eos_free(content);
        return (int)result;
    }

    if (argc == 3 && strcmp(argv[1], "get") == 0)
    {
        cJSON *root = eos_storage_json_load(EOS_CONFIG_FILE_PATH);
        if (!root)
        {
            return (int)esh_printf(ctx, "config: cannot read configuration\r\n");
        }
        item = cJSON_DetachItemFromObject(root, argv[2]);
        result = _print_json_item(ctx, argv[2], item);
        cJSON_Delete(item);
        cJSON_Delete(root);
        return result;
    }

    if (argc == 4 && strcmp(argv[1], "set") == 0)
    {
        result = (eos_result_t)_config_set_value(argv[2], argv[3]);
        return result == EOS_OK ? EOS_OK : (int)esh_printf(ctx, "config: set failed: %s\r\n", argv[2]);
    }

    return (int)esh_printf(ctx, "config: usage: config list|get <key>|set <key> <value>\r\n");
}

int esh_builtin_cmd_state(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char *content;
    cJSON *root;
    cJSON *item;
    eos_result_t result;

    if (!ctx || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc == 2 && strcmp(argv[1], "dump") == 0)
    {
        content = eos_storage_read_file(EOS_STATE_FILE_PATH);
        if (!content)
        {
            return (int)esh_printf(ctx, "state: cannot read state\r\n");
        }
        result = _write_text(ctx, content);
        eos_free(content);
        return (int)result;
    }

    if (argc == 3 && strcmp(argv[1], "get") == 0)
    {
        root = eos_storage_json_load(EOS_STATE_FILE_PATH);
        if (!root)
        {
            return (int)esh_printf(ctx, "state: cannot read state\r\n");
        }
        item = cJSON_DetachItemFromObject(root, argv[2]);
        result = _print_json_item(ctx, argv[2], item);
        cJSON_Delete(item);
        cJSON_Delete(root);
        return result;
    }

    return (int)esh_printf(ctx, "state: usage: state dump|get <key>\r\n");
}

int esh_builtin_cmd_pkg(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    eos_pkg_header_t header;
    eos_result_t result;
    uint32_t count;
    uint32_t index;

    if (!ctx || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc == 2 && strcmp(argv[1], "list") == 0)
    {
        count = eos_app_get_installed();
        for (index = 0U; index < count; index++)
        {
            if (esh_printf(ctx, "%s\r\n", eos_app_list_get_id(index)) != EOS_OK)
            {
                return EOS_ERR_IO;
            }
        }
        return EOS_OK;
    }

    if (argc == 3 && strcmp(argv[1], "info") == 0)
    {
        result = eos_pkg_read_header(argv[2], &header);
        if (result != EOS_OK)
        {
            return (int)esh_printf(ctx, "pkg: cannot read package: %s\r\n", argv[2]);
        }
        return (int)esh_printf(ctx,
                               "id=%s name=%s version=%s type=%s api=%" PRIu16 "..%" PRIu16 " files=%" PRIu32 "\r\n",
                               header.pkg_id,
                               header.pkg_name,
                               header.pkg_version,
                               memcmp(header.magic, EOS_PKG_WATCHFACE_MAGIC, 4U) == 0 ? "watchface" : "application",
                               header.min_api_level,
                               header.target_api_level,
                               header.file_count);
    }

    if (argc == 3 && strcmp(argv[1], "install") == 0)
    {
        result = eos_app_install(argv[2]);
    }
    else if (argc == 3 && strcmp(argv[1], "uninstall") == 0)
    {
        result = eos_app_uninstall(argv[2]);
    }
    else
    {
        return (int)esh_printf(ctx, "pkg: usage: pkg list|info <file>|install <file>|uninstall <id>\r\n");
    }

    return result == EOS_OK ? EOS_OK : (int)esh_printf(ctx, "pkg: operation failed\r\n");
}
