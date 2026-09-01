/**
 * @file esh.c
 * @brief Lightweight Elenix Shell core implementation
 */

#include "esh.h"

/* Includes ---------------------------------------------------*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Macros and Definitions -------------------------------------*/
#define _ESH_BACKSPACE 0x08U
#define _ESH_DELETE 0x7FU
#define _ESH_ENTER_CR 0x0DU
#define _ESH_ENTER_LF 0x0AU
#define _ESH_BELL "\a"
#define _ESH_CRLF "\r\n"
#define _ESH_CLEAR_SPACES "                                                                "

/* Variables --------------------------------------------------*/

/* Function Prototypes ----------------------------------------*/
static eos_result_t _esh_release_internal(esh_t *esh, esh_owner_token_t token, esh_close_reason_t reason);

/* Function Implementations -----------------------------------*/

static void _esh_reset_line(esh_t *esh)
{
    esh->line_length = 0U;
    esh->line_overflow = false;
    esh->ignore_lf = false;
    esh->line[0] = '\0';
}

static eos_result_t _esh_write_bytes(esh_t *esh, const uint8_t *data, size_t length)
{
    size_t written;

    if (length == 0U)
    {
        return EOS_OK;
    }

    if (!esh || !esh->owner_active || !esh->active_frontend || !esh->active_frontend->write || !data)
    {
        return ESH_ERR_NO_OWNER;
    }

    written = esh->active_frontend->write(data, length, esh->active_frontend->user_data);
    return (written == length) ? EOS_OK : EOS_ERR_IO;
}

static eos_result_t _esh_write_string(esh_t *esh, const char *text)
{
    if (!text)
    {
        return EOS_ERR_INVALID_ARG;
    }

    return _esh_write_bytes(esh, (const uint8_t *)text, strlen(text));
}

static eos_result_t _esh_clear_visible_line(esh_t *esh)
{
    size_t remaining;
    size_t chunk;

    if (!esh->prompt_visible)
    {
        return EOS_OK;
    }

    if (_esh_write_string(esh, "\r") != EOS_OK)
    {
        return EOS_ERR_IO;
    }

    remaining = strlen(ESH_PROMPT) + esh->line_length;
    while (remaining > 0U)
    {
        chunk = remaining < (sizeof(_ESH_CLEAR_SPACES) - 1U) ? remaining : (sizeof(_ESH_CLEAR_SPACES) - 1U);
        if (_esh_write_bytes(esh, (const uint8_t *)_ESH_CLEAR_SPACES, chunk) != EOS_OK)
        {
            return EOS_ERR_IO;
        }
        remaining -= chunk;
    }

    return _esh_write_string(esh, "\r");
}

static eos_result_t _esh_write_prompt(esh_t *esh)
{
    eos_result_t result = _esh_write_string(esh, ESH_PROMPT);

    if (result == EOS_OK)
    {
        esh->prompt_visible = true;
    }

    return result;
}

static const esh_command_t *_esh_find_command(const esh_t *esh, const char *name)
{
    const esh_command_t *command;

    if (!esh || !name || !esh->commands_begin || !esh->commands_end)
    {
        return NULL;
    }

    for (command = esh->commands_begin; command < esh->commands_end; command++)
    {
        if (command->name && strcmp(command->name, name) == 0)
        {
            return command;
        }
    }

    return NULL;
}

static eos_result_t _esh_tokenize(esh_t *esh, size_t *argc_out)
{
    char *cursor;
    size_t argc = 0U;

    if (!esh || !argc_out)
    {
        return EOS_ERR_INVALID_ARG;
    }

    cursor = esh->line;

    while (*cursor != '\0')
    {
        while (*cursor == ' ' || *cursor == '\t')
        {
            cursor++;
        }

        if (*cursor == '\0')
        {
            break;
        }

        if (argc >= ESH_MAX_ARGS)
        {
            return ESH_ERR_TOO_MANY_ARGS;
        }

        esh->argv[argc++] = cursor;

        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t')
        {
            cursor++;
        }

        if (*cursor != '\0')
        {
            *cursor++ = '\0';
        }
    }

    *argc_out = argc;
    return EOS_OK;
}

static eos_result_t _esh_execute_line(esh_t *esh)
{
    const esh_command_t *command;
    esh_cmd_ctx_t context;
    size_t argc;
    eos_result_t result;

    if (esh->line_overflow)
    {
        _esh_write_string(esh, "\r\nesh: command line too long\r\n");
        _esh_reset_line(esh);
        return ESH_ERR_LINE_TOO_LONG;
    }

    result = _esh_tokenize(esh, &argc);
    if (result != EOS_OK)
    {
        _esh_write_string(esh, "\r\nesh: too many arguments\r\n");
        _esh_reset_line(esh);
        return result;
    }

    if (argc == 0U)
    {
        _esh_reset_line(esh);
        return _esh_write_prompt(esh);
    }

    context.esh = esh;
    context.frontend = esh->active_frontend;
    context.owner.frontend = esh->active_frontend;
    context.owner.generation = esh->active_generation;

    command = _esh_find_command(esh, esh->argv[0]);
    if (!command || !command->handler)
    {
        esh_printf(&context, "\r\nesh: unknown command: %s\r\n", esh->argv[0]);
        _esh_reset_line(esh);
        return _esh_write_prompt(esh);
    }

    esh->release_requested = false;

    result = (command->handler)(&context, (int)argc, esh->argv);

    if (esh->release_requested)
    {
        _esh_release_internal(esh, context.owner, ESH_CLOSE_RELEASE);
        return result;
    }

    _esh_reset_line(esh);
    if (esh->input_mode == ESH_INPUT_COMMAND)
    {
        _esh_write_prompt(esh);
    }
    return result;
}

static eos_result_t _esh_process_byte(esh_t *esh, uint8_t byte)
{
    if (byte == _ESH_ENTER_LF && esh->ignore_lf)
    {
        esh->ignore_lf = false;
        return EOS_OK;
    }

    esh->ignore_lf = false;

    if (byte == _ESH_ENTER_CR || byte == _ESH_ENTER_LF)
    {
        if (byte == _ESH_ENTER_CR)
        {
            esh->ignore_lf = true;
        }

        esh->prompt_visible = false;
        _esh_write_string(esh, _ESH_CRLF);
        return _esh_execute_line(esh);
    }

    if (byte == _ESH_BACKSPACE || byte == _ESH_DELETE)
    {
        if (esh->line_length > 0U)
        {
            esh->line_length--;
            esh->line[esh->line_length] = '\0';
            esh->line_overflow = false;
            return _esh_write_bytes(esh, (const uint8_t *)"\b \b", 3U);
        }

        return EOS_OK;
    }

    if (byte < 0x20U)
    {
        return EOS_OK;
    }

    if (esh->line_overflow)
    {
        return EOS_OK;
    }

    if (esh->line_length >= (ESH_LINE_MAX - 1U))
    {
        esh->line_overflow = true;
        return _esh_write_string(esh, _ESH_BELL);
    }

    esh->line[esh->line_length++] = (char)byte;
    esh->line[esh->line_length] = '\0';
    return _esh_write_bytes(esh, &byte, 1U);
}

static eos_result_t _esh_validate_commands(const esh_t *esh)
{
    const esh_command_t *left;
    const esh_command_t *right;

    for (left = esh->commands_begin; left < esh->commands_end; left++)
    {
        if (!left->name || !left->handler)
        {
            return EOS_ERR_INVALID_ARG;
        }

        for (right = left + 1; right < esh->commands_end; right++)
        {
            if (right->name && strcmp(left->name, right->name) == 0)
            {
                return EOS_ERR_ALREADY_EXISTS;
            }
        }
    }

    return EOS_OK;
}

eos_result_t esh_init_with_commands(esh_t *esh, const esh_command_t *commands_begin, const esh_command_t *commands_end)
{
    eos_result_t result;

    if (!esh || !commands_begin || !commands_end || commands_end < commands_begin)
    {
        return EOS_ERR_INVALID_ARG;
    }

    memset(esh, 0, sizeof(*esh));
    esh->commands_begin = commands_begin;
    esh->commands_end = commands_end;
    esh->input_mode = ESH_INPUT_COMMAND;
    esh->cwd[0] = '/';
    esh->cwd[1] = '\0';
    _esh_reset_line(esh);

    result = _esh_validate_commands(esh);
    if (result != EOS_OK)
    {
        memset(esh, 0, sizeof(*esh));
    }

    return result;
}

eos_result_t esh_init(esh_t *esh)
{
    return esh_init_with_commands(esh, esh_command_begin(), esh_command_end());
}

bool esh_is_owner(const esh_t *esh, esh_owner_token_t token)
{
    return esh && esh->owner_active && token.frontend == esh->active_frontend
           && token.generation == esh->active_generation;
}

eos_result_t esh_claim(esh_t *esh,
                       const esh_frontend_t *frontend,
                       esh_claim_action_t action,
                       esh_owner_token_t *out_token)
{
    const esh_frontend_t *old_frontend;
    uint32_t next_generation;

    if (!esh || !frontend || !frontend->write)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (action == ESH_CLAIM_CANCEL)
    {
        return ESH_ERR_CANCELLED;
    }

    if (action != ESH_CLAIM_TAKEOVER)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (esh->input_busy)
    {
        return ESH_ERR_REENTRANT;
    }

    esh->input_busy = true;

    if (esh->owner_active && esh->active_frontend == frontend)
    {
        if (out_token)
        {
            out_token->frontend = frontend;
            out_token->generation = esh->active_generation;
        }

        esh->input_busy = false;
        return EOS_OK;
    }

    old_frontend = esh->active_frontend;

    if (esh->input_mode == ESH_INPUT_YMODEM)
    {
        esh_ymodem_abort(esh);
    }

    _esh_clear_visible_line(esh);
    esh->owner_active = false;
    esh->active_frontend = NULL;
    esh->active_generation++;
    esh->interleaved_output = false;
    esh->prompt_visible = false;
    _esh_reset_line(esh);

    if (old_frontend && old_frontend->on_closed)
    {
        old_frontend->on_closed(ESH_CLOSE_TAKEOVER, old_frontend->user_data);
    }

    next_generation = esh->active_generation + 1U;
    if (next_generation == 0U)
    {
        next_generation = 1U;
    }

    esh->active_generation = next_generation;
    esh->active_frontend = frontend;
    esh->owner_active = true;

    if (out_token)
    {
        out_token->frontend = frontend;
        out_token->generation = esh->active_generation;
    }

    {
        eos_result_t result = _esh_write_prompt(esh);
        esh->input_busy = false;
        return result;
    }
}

static eos_result_t _esh_release_internal(esh_t *esh, esh_owner_token_t token, esh_close_reason_t reason)
{
    const esh_frontend_t *old_frontend;

    if (!esh)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (!esh->owner_active)
    {
        return ESH_ERR_NO_OWNER;
    }

    if (!esh_is_owner(esh, token))
    {
        return ESH_ERR_NOT_OWNER;
    }

    old_frontend = esh->active_frontend;
    if (esh->input_mode == ESH_INPUT_YMODEM)
    {
        esh_ymodem_abort(esh);
    }

    _esh_clear_visible_line(esh);
    esh->owner_active = false;
    esh->active_frontend = NULL;
    esh->active_generation++;
    esh->interleaved_output = false;
    esh->prompt_visible = false;
    _esh_reset_line(esh);

    if (old_frontend && old_frontend->on_closed)
    {
        old_frontend->on_closed(reason, old_frontend->user_data);
    }

    return EOS_OK;
}

eos_result_t esh_release(esh_t *esh, esh_owner_token_t token)
{
    eos_result_t result;

    if (!esh)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (esh->input_busy)
    {
        return ESH_ERR_REENTRANT;
    }

    esh->input_busy = true;
    result = _esh_release_internal(esh, token, ESH_CLOSE_RELEASE);
    esh->input_busy = false;

    return result;
}

eos_result_t esh_input(esh_t *esh, esh_owner_token_t token, const uint8_t *data, size_t length)
{
    eos_result_t result = EOS_OK;
    size_t i;

    if (!esh || (!data && length > 0U))
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (!esh->owner_active)
    {
        return ESH_ERR_NO_OWNER;
    }

    if (!esh_is_owner(esh, token))
    {
        return ESH_ERR_NOT_OWNER;
    }

    if (esh->input_busy)
    {
        return ESH_ERR_REENTRANT;
    }

    esh->input_busy = true;

    for (i = 0U; i < length; i++)
    {
        eos_result_t byte_result;
        bool was_ymodem = esh->input_mode == ESH_INPUT_YMODEM;

        byte_result = was_ymodem ? esh_ymodem_input(esh, &data[i], 1U) : _esh_process_byte(esh, data[i]);

        if (byte_result != EOS_OK)
        {
            result = byte_result;
        }

        if (!esh->owner_active)
        {
            break;
        }

        if (was_ymodem && esh->input_mode == ESH_INPUT_COMMAND)
        {
            _esh_reset_line(esh);
            _esh_write_prompt(esh);
            break;
        }
    }

    esh->input_busy = false;
    return result;
}

void esh_poll(esh_t *esh)
{
    bool was_ymodem;

    if (!esh || !esh->owner_active)
    {
        return;
    }

    was_ymodem = esh->input_mode == ESH_INPUT_YMODEM;
    esh_ymodem_poll(esh);
    if (was_ymodem && esh->input_mode == ESH_INPUT_COMMAND)
    {
        _esh_reset_line(esh);
        _esh_write_prompt(esh);
    }
}

eos_result_t esh_write_active(esh_t *esh, const void *data, size_t length)
{
    if (!esh || (!data && length > 0U))
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (!esh->owner_active)
    {
        return ESH_ERR_NO_OWNER;
    }

    return _esh_write_bytes(esh, data, length);
}

eos_result_t esh_interleaved_begin(esh_t *esh)
{
    eos_result_t result;

    if (!esh)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (!esh->owner_active)
    {
        return ESH_ERR_NO_OWNER;
    }

    if (esh->interleaved_output)
    {
        return ESH_ERR_REENTRANT;
    }

    esh->interleaved_output = true;
    result = _esh_clear_visible_line(esh);
    if (result != EOS_OK)
    {
        esh->interleaved_output = false;
    }

    return result;
}

eos_result_t esh_interleaved_end(esh_t *esh)
{
    eos_result_t result = EOS_OK;

    if (!esh)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (!esh->owner_active)
    {
        esh->interleaved_output = false;
        return ESH_ERR_NO_OWNER;
    }

    if (!esh->interleaved_output)
    {
        return EOS_ERR_INVALID_STATE;
    }

    esh->interleaved_output = false;
    if (esh->prompt_visible)
    {
        result = _esh_write_prompt(esh);
        if (result == EOS_OK && esh->line_length > 0U)
        {
            result = _esh_write_bytes(esh, (const uint8_t *)esh->line, esh->line_length);
        }
    }

    return result;
}

eos_result_t esh_write(esh_cmd_ctx_t *ctx, const void *data, size_t length)
{
    if (!ctx)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (!esh_is_owner(ctx->esh, ctx->owner))
    {
        return ESH_ERR_NOT_OWNER;
    }

    return esh_write_active(ctx->esh, data, length);
}

eos_result_t esh_printf(esh_cmd_ctx_t *ctx, const char *format, ...)
{
    char buffer[ESH_PRINTF_BUFFER_SIZE];
    va_list args;
    int length;

    if (!ctx || !format)
    {
        return EOS_ERR_INVALID_ARG;
    }

    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length < 0)
    {
        return EOS_ERR_IO;
    }

    if ((size_t)length >= sizeof(buffer))
    {
        return EOS_ERR_IO;
    }

    return esh_write(ctx, buffer, (size_t)length);
}

eos_result_t esh_request_release(esh_cmd_ctx_t *ctx)
{
    if (!ctx || !ctx->esh)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (!esh_is_owner(ctx->esh, ctx->owner))
    {
        return ESH_ERR_NOT_OWNER;
    }

    ctx->esh->release_requested = true;
    return EOS_OK;
}
