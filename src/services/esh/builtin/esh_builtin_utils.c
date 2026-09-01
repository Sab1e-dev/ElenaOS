/**
 * @file esh_builtin_utils.c
 * @brief Platform-neutral file utility ESH commands
 */

#include "esh_builtin_commands.h"

/* Includes ---------------------------------------------------*/
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_service_storage.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/
#define _ESH_UTIL_BUFFER_SIZE 256U

/* Variables --------------------------------------------------*/

/* Function Prototypes ----------------------------------------*/
static bool _join_path(const char *base, const char *name, char *path, size_t path_size);
static int _find_path(esh_cmd_ctx_t *ctx, const char *path);
static bool _parse_count(const char *text, uint32_t *count);

/* Function Implementations -----------------------------------*/

static bool _join_path(const char *base, const char *name, char *path, size_t path_size)
{
    int written;

    if (!base || !name || !path || path_size == 0U)
    {
        return false;
    }

    written = snprintf(path, path_size, "%s%s%s", base, strcmp(base, "/") == 0 ? "" : "/", name);
    return written >= 0 && (size_t)written < path_size;
}

static int _find_path(esh_cmd_ctx_t *ctx, const char *path)
{
    eos_dir_t directory;
    char name[EOS_FS_PATH_MAX];
    char child[EOS_FS_PATH_MAX];
    eos_result_t result;

    if (eos_storage_is_file(path))
    {
        return (int)esh_printf(ctx, "%s\r\n", path);
    }

    if (!eos_storage_is_dir(path))
    {
        return (int)esh_printf(ctx, "find: no such file or directory: %s\r\n", path);
    }

    directory = eos_storage_dir_open(path);
    if (!directory)
    {
        return (int)esh_printf(ctx, "find: cannot open directory: %s\r\n", path);
    }

    result = EOS_OK;
    while (eos_storage_dir_read(directory, name, sizeof(name)) == EOS_OK)
    {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        {
            continue;
        }

        if (!_join_path(path, name, child, sizeof(child)))
        {
            result = esh_printf(ctx, "find: path too long: %s\r\n", name);
            break;
        }

        result = _find_path(ctx, child);
        if (result != EOS_OK)
        {
            break;
        }
    }

    eos_storage_dir_close(directory);
    return (int)result;
}

int esh_builtin_cmd_find(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char resolved[EOS_FS_PATH_MAX];
    const char *target;

    if (!ctx || !ctx->esh || !argv || argc > 2)
    {
        return (int)esh_printf(ctx, "find: usage: find [path]\r\n");
    }

    target = argc == 1 ? ctx->esh->cwd : argv[1];
    if (!esh_builtin_resolve_path(ctx->esh, target, resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "find: path too long: %s\r\n", target);
    }

    return _find_path(ctx, resolved);
}

int esh_builtin_cmd_grep(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    bool with_line_number = false;
    const char *pattern;
    const char *path;
    char resolved[EOS_FS_PATH_MAX];
    char *content;
    char *line;
    char *line_end;
    uint32_t line_number = 0U;
    eos_result_t result;

    if (!ctx || !ctx->esh || !argv || (argc != 3 && argc != 4))
    {
        return (int)esh_printf(ctx, "grep: usage: grep [-n] <pattern> <file>\r\n");
    }

    if (argc == 4)
    {
        if (strcmp(argv[1], "-n") != 0)
        {
            return (int)esh_printf(ctx, "grep: usage: grep [-n] <pattern> <file>\r\n");
        }
        with_line_number = true;
        pattern = argv[2];
        path = argv[3];
    }
    else
    {
        pattern = argv[1];
        path = argv[2];
    }

    if (!esh_builtin_resolve_path(ctx->esh, path, resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "grep: path too long: %s\r\n", path);
    }

    content = eos_storage_read_file(resolved);
    if (!content)
    {
        return (int)esh_printf(ctx, "grep: cannot read: %s\r\n", path);
    }

    result = EOS_OK;
    line = content;
    while (*line != '\0')
    {
        line_number++;
        line_end = strchr(line, '\n');
        if (!line_end)
        {
            line_end = line + strlen(line);
        }

        bool has_newline = *line_end != '\0';
        *line_end = '\0';
        if (strstr(line, pattern))
        {
            result = esh_printf(ctx, with_line_number ? "%" PRIu32 ":%s\r\n" : "%s\r\n", line_number, line);
            if (result != EOS_OK)
            {
                break;
            }
        }

        if (!has_newline)
        {
            break;
        }
        line = line_end + 1;
    }

    eos_free(content);
    return (int)result;
}

static bool _parse_count(const char *text, uint32_t *count)
{
    char *end;
    unsigned long value;

    if (!text || !count)
    {
        return false;
    }

    value = strtoul(text, &end, 10);
    if (*text == '\0' || *end != '\0' || value > UINT32_MAX)
    {
        return false;
    }

    *count = (uint32_t)value;
    return true;
}

static uint32_t _line_count(const char *content)
{
    uint32_t count = 0U;
    const char *cursor = content;

    while (*cursor)
    {
        if (*cursor++ == '\n')
        {
            count++;
        }
    }

    if (cursor != content && cursor[-1] != '\n')
    {
        count++;
    }
    return count;
}

static int _print_line_range(esh_cmd_ctx_t *ctx, const char *content, uint32_t first, uint32_t last)
{
    const char *line = content;
    const char *line_end;
    uint32_t line_number = 0U;

    while (*line && line_number < last)
    {
        line_number++;
        line_end = strchr(line, '\n');
        if (!line_end)
        {
            line_end = line + strlen(line);
        }

        if (line_number >= first)
        {
            if (esh_write(ctx, line, (size_t)(line_end - line)) != EOS_OK || esh_write(ctx, "\r\n", 2U) != EOS_OK)
            {
                return EOS_ERR_IO;
            }
        }

        if (*line_end == '\0')
        {
            break;
        }
        line = line_end + 1;
    }
    return EOS_OK;
}

static int _head_tail(esh_cmd_ctx_t *ctx, int argc, char *argv[], bool tail)
{
    uint32_t count = 10U;
    uint32_t lines;
    uint32_t first;
    uint32_t last;
    char resolved[EOS_FS_PATH_MAX];
    char *content;

    if (!ctx || !ctx->esh || !argv || (argc != 2 && argc != 3))
    {
        return (int)esh_printf(ctx, "%s: usage: %s <file> [lines]\r\n", tail ? "tail" : "head", tail ? "tail" : "head");
    }

    if (argc == 3 && !_parse_count(argv[2], &count))
    {
        return (int)esh_printf(ctx, "%s: invalid line count\r\n", tail ? "tail" : "head");
    }

    if (!esh_builtin_resolve_path(ctx->esh, argv[1], resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "%s: path too long: %s\r\n", tail ? "tail" : "head", argv[1]);
    }

    content = eos_storage_read_file(resolved);
    if (!content)
    {
        return (int)esh_printf(ctx, "%s: cannot read: %s\r\n", tail ? "tail" : "head", argv[1]);
    }

    lines = _line_count(content);
    if (tail)
    {
        first = count >= lines ? 1U : lines - count + 1U;
        last = lines;
    }
    else
    {
        first = 1U;
        last = count < lines ? count : lines;
    }

    int result = _print_line_range(ctx, content, first, last);
    eos_free(content);
    return result;
}

int esh_builtin_cmd_head(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    return _head_tail(ctx, argc, argv, false);
}

int esh_builtin_cmd_tail(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    return _head_tail(ctx, argc, argv, true);
}

int esh_builtin_cmd_wc(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    char resolved[EOS_FS_PATH_MAX];
    char *content;
    size_t index;
    size_t bytes;
    uint32_t lines = 0U;
    uint32_t words = 0U;
    bool in_word = false;

    if (!ctx || !ctx->esh || !argv || argc != 2)
    {
        return (int)esh_printf(ctx, "wc: usage: wc <file>\r\n");
    }

    if (!esh_builtin_resolve_path(ctx->esh, argv[1], resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "wc: path too long: %s\r\n", argv[1]);
    }

    content = eos_storage_read_file(resolved);
    if (!content)
    {
        return (int)esh_printf(ctx, "wc: cannot read: %s\r\n", argv[1]);
    }

    bytes = strlen(content);
    for (index = 0U; index < bytes; index++)
    {
        if (content[index] == '\n')
        {
            lines++;
        }
        if (isspace((unsigned char)content[index]))
        {
            in_word = false;
        }
        else if (!in_word)
        {
            words++;
            in_word = true;
        }
    }
    if (bytes > 0U && content[bytes - 1U] != '\n')
    {
        lines++;
    }

    eos_free(content);
    return (int)esh_printf(ctx, "%" PRIu32 " lines, %" PRIu32 " words, %zu bytes\r\n", lines, words, bytes);
}

static uint32_t _crc32_update(uint32_t crc, const uint8_t *data, size_t size)
{
    size_t index;
    unsigned bit;

    for (index = 0U; index < size; index++)
    {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (crc & 1U) ? (crc >> 1U) ^ 0xEDB88320U : crc >> 1U;
        }
    }
    return crc;
}

int esh_builtin_cmd_crc32(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    uint8_t buffer[_ESH_UTIL_BUFFER_SIZE];
    char resolved[EOS_FS_PATH_MAX];
    eos_file_t file;
    ssize_t read_size;
    uint32_t crc = UINT32_MAX;

    if (!ctx || !ctx->esh || !argv || argc != 2)
    {
        return (int)esh_printf(ctx, "crc32: usage: crc32 <file>\r\n");
    }
    if (!esh_builtin_resolve_path(ctx->esh, argv[1], resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "crc32: path too long: %s\r\n", argv[1]);
    }

    file = eos_storage_file_open_read(resolved);
    if (file == EOS_FILE_INVALID)
    {
        return (int)esh_printf(ctx, "crc32: cannot open: %s\r\n", argv[1]);
    }
    while ((read_size = eos_storage_file_read(file, buffer, sizeof(buffer))) > 0)
    {
        crc = _crc32_update(crc, buffer, (size_t)read_size);
    }
    eos_storage_file_close(file);
    if (read_size < 0)
    {
        return (int)esh_printf(ctx, "crc32: read failed: %s\r\n", argv[1]);
    }
    return (int)esh_printf(ctx, "%08" PRIx32 "  %s\r\n", ~crc, argv[1]);
}

typedef struct
{
    uint32_t state[8];
    uint64_t bit_length;
    uint8_t block[64];
    size_t block_length;
} _sha256_ctx_t;

static uint32_t _sha_rotr(uint32_t value, unsigned count)
{
    return (value >> count) | (value << (32U - count));
}

static void _sha256_transform(_sha256_ctx_t *ctx, const uint8_t block[64])
{
    static const uint32_t constants[64] = {
        0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
        0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U, 0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
        0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
        0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U, 0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
        0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U, 0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
        0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U, 0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
        0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
        0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U, 0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U};
    uint32_t words[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t temp1, temp2;
    unsigned index;

    for (index = 0U; index < 16U; index++)
    {
        words[index] = ((uint32_t)block[index * 4U] << 24U) | ((uint32_t)block[index * 4U + 1U] << 16U)
                       | ((uint32_t)block[index * 4U + 2U] << 8U) | block[index * 4U + 3U];
    }
    for (index = 16U; index < 64U; index++)
    {
        uint32_t s0 =
            _sha_rotr(words[index - 15U], 7U) ^ _sha_rotr(words[index - 15U], 18U) ^ (words[index - 15U] >> 3U);
        uint32_t s1 =
            _sha_rotr(words[index - 2U], 17U) ^ _sha_rotr(words[index - 2U], 19U) ^ (words[index - 2U] >> 10U);
        words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    for (index = 0U; index < 64U; index++)
    {
        uint32_t s1 = _sha_rotr(e, 6U) ^ _sha_rotr(e, 11U) ^ _sha_rotr(e, 25U);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t s0 = _sha_rotr(a, 2U) ^ _sha_rotr(a, 13U) ^ _sha_rotr(a, 22U);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        temp1 = h + s1 + choice + constants[index] + words[index];
        temp2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void _sha256_init(_sha256_ctx_t *ctx)
{
    static const uint32_t initial[8] =
        {0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU, 0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U};
    memcpy(ctx->state, initial, sizeof(initial));
    ctx->bit_length = 0U;
    ctx->block_length = 0U;
}

static void _sha256_update(_sha256_ctx_t *ctx, const uint8_t *data, size_t size)
{
    while (size > 0U)
    {
        size_t copy_size = sizeof(ctx->block) - ctx->block_length;
        if (copy_size > size)
        {
            copy_size = size;
        }
        memcpy(ctx->block + ctx->block_length, data, copy_size);
        ctx->block_length += copy_size;
        ctx->bit_length += (uint64_t)copy_size * 8U;
        data += copy_size;
        size -= copy_size;
        if (ctx->block_length == sizeof(ctx->block))
        {
            _sha256_transform(ctx, ctx->block);
            ctx->block_length = 0U;
        }
    }
}

static void _sha256_final(_sha256_ctx_t *ctx, uint8_t digest[32])
{
    size_t index;

    ctx->block[ctx->block_length++] = 0x80U;
    if (ctx->block_length > 56U)
    {
        while (ctx->block_length < sizeof(ctx->block))
        {
            ctx->block[ctx->block_length++] = 0U;
        }
        _sha256_transform(ctx, ctx->block);
        ctx->block_length = 0U;
    }
    while (ctx->block_length < 56U)
    {
        ctx->block[ctx->block_length++] = 0U;
    }
    for (index = 0U; index < 8U; index++)
    {
        ctx->block[56U + index] = (uint8_t)(ctx->bit_length >> (56U - index * 8U));
    }
    _sha256_transform(ctx, ctx->block);
    for (index = 0U; index < 8U; index++)
    {
        digest[index * 4U] = (uint8_t)(ctx->state[index] >> 24U);
        digest[index * 4U + 1U] = (uint8_t)(ctx->state[index] >> 16U);
        digest[index * 4U + 2U] = (uint8_t)(ctx->state[index] >> 8U);
        digest[index * 4U + 3U] = (uint8_t)ctx->state[index];
    }
}

int esh_builtin_cmd_sha256(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    static const char hex[] = "0123456789abcdef";
    _sha256_ctx_t sha;
    uint8_t buffer[_ESH_UTIL_BUFFER_SIZE];
    uint8_t digest[32];
    char resolved[EOS_FS_PATH_MAX];
    char text[65];
    eos_file_t file;
    ssize_t read_size;
    size_t index;

    if (!ctx || !ctx->esh || !argv || argc != 2)
    {
        return (int)esh_printf(ctx, "sha256: usage: sha256 <file>\r\n");
    }
    if (!esh_builtin_resolve_path(ctx->esh, argv[1], resolved, sizeof(resolved)))
    {
        return (int)esh_printf(ctx, "sha256: path too long: %s\r\n", argv[1]);
    }
    file = eos_storage_file_open_read(resolved);
    if (file == EOS_FILE_INVALID)
    {
        return (int)esh_printf(ctx, "sha256: cannot open: %s\r\n", argv[1]);
    }

    _sha256_init(&sha);
    while ((read_size = eos_storage_file_read(file, buffer, sizeof(buffer))) > 0)
    {
        _sha256_update(&sha, buffer, (size_t)read_size);
    }
    eos_storage_file_close(file);
    if (read_size < 0)
    {
        return (int)esh_printf(ctx, "sha256: read failed: %s\r\n", argv[1]);
    }

    _sha256_final(&sha, digest);
    for (index = 0U; index < sizeof(digest); index++)
    {
        text[index * 2U] = hex[digest[index] >> 4U];
        text[index * 2U + 1U] = hex[digest[index] & 0x0FU];
    }
    text[sizeof(digest) * 2U] = '\0';
    return (int)esh_printf(ctx, "%s  %s\r\n", text, argv[1]);
}
