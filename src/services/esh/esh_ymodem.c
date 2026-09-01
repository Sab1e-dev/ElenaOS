/**
 * @file esh_ymodem.c
 * @brief YMODEM transport for ESH
 */

#include "esh_ymodem.h"

/* Includes ---------------------------------------------------*/
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "eos_core.h"
#include "eos_service_storage.h"
#include "esh.h"

/* Macros and Definitions -------------------------------------*/
#define _ESH_YMODEM_SOH 0x01U
#define _ESH_YMODEM_STX 0x02U
#define _ESH_YMODEM_EOT 0x04U
#define _ESH_YMODEM_ACK 0x06U
#define _ESH_YMODEM_NAK 0x15U
#define _ESH_YMODEM_CAN 0x18U
#define _ESH_YMODEM_CRC 'C'
#define _ESH_YMODEM_DATA_SIZE 128U
#define _ESH_YMODEM_BLOCK_SIZE 1024U
#define _ESH_YMODEM_HEADER_BLOCK 0U
#define _ESH_YMODEM_TIMEOUT_MS 3000U
#define _ESH_YMODEM_MAX_RETRIES 10U
#define _ESH_YMODEM_PADDING 0x1AU

/* Variables --------------------------------------------------*/

/* Function Prototypes ----------------------------------------*/
static uint16_t _esh_ymodem_crc16(const uint8_t *data, size_t length);
static eos_result_t _esh_ymodem_write_control(esh_t *esh, uint8_t control);
static eos_result_t _esh_ymodem_send_packet(esh_t *esh);
static eos_result_t _esh_ymodem_send_header(esh_t *esh);
static eos_result_t _esh_ymodem_send_data(esh_t *esh);
static eos_result_t _esh_ymodem_finish(esh_t *esh, bool success, bool send_cancel);
static eos_result_t _esh_ymodem_process_byte(esh_t *esh, uint8_t byte);
static eos_result_t _esh_ymodem_process_packet(esh_t *esh);
static bool _esh_ymodem_parse_size(const uint8_t *data, size_t length, uint32_t *size);

/* Function Implementations -----------------------------------*/

static uint16_t _esh_ymodem_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0U;
    size_t index;
    uint8_t bit;

    for (index = 0U; index < length; index++)
    {
        crc ^= (uint16_t)data[index] << 8U;
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (crc & 0x8000U) != 0U ? (uint16_t)((crc << 1U) ^ 0x1021U) : (uint16_t)(crc << 1U);
        }
    }

    return crc;
}

static eos_result_t _esh_ymodem_write_control(esh_t *esh, uint8_t control)
{
    return esh_write_active(esh, &control, sizeof(control));
}

static eos_result_t _esh_ymodem_send_packet(esh_t *esh)
{
    uint16_t crc;
    size_t data_size;

    data_size = esh->ymodem.packet[0] == _ESH_YMODEM_STX ? _ESH_YMODEM_BLOCK_SIZE : _ESH_YMODEM_DATA_SIZE;
    crc = _esh_ymodem_crc16(&esh->ymodem.packet[3], data_size);
    esh->ymodem.packet[3U + data_size] = (uint8_t)(crc >> 8U);
    esh->ymodem.packet[4U + data_size] = (uint8_t)crc;
    esh->ymodem.packet_length = 5U + data_size;
    return esh_write_active(esh, esh->ymodem.packet, esh->ymodem.packet_length);
}

static eos_result_t _esh_ymodem_send_header(esh_t *esh)
{
    const char *filename = strrchr(esh->ymodem.path, '/');
    size_t filename_length;
    char size_text[16];
    int size_text_length;

    filename = filename ? filename + 1 : esh->ymodem.path;
    filename_length = strlen(filename);
    if (filename_length >= (_ESH_YMODEM_DATA_SIZE - 1U))
    {
        filename_length = _ESH_YMODEM_DATA_SIZE - 2U;
    }

    memset(esh->ymodem.packet, 0, sizeof(esh->ymodem.packet));
    esh->ymodem.packet[0] = _ESH_YMODEM_SOH;
    esh->ymodem.packet[1] = _ESH_YMODEM_HEADER_BLOCK;
    esh->ymodem.packet[2] = (uint8_t)~_ESH_YMODEM_HEADER_BLOCK;
    memcpy(&esh->ymodem.packet[3], filename, filename_length);
    esh->ymodem.packet[3U + filename_length] = '\0';

    size_text_length = snprintf(size_text, sizeof(size_text), "%" PRIu32, esh->ymodem.file_size);
    if (size_text_length < 0 || (size_t)size_text_length >= sizeof(size_text)
        || filename_length + 1U + (size_t)size_text_length >= _ESH_YMODEM_DATA_SIZE)
    {
        return EOS_ERR_PATH_TOO_LONG;
    }

    memcpy(&esh->ymodem.packet[4U + filename_length], size_text, (size_t)size_text_length + 1U);
    esh->ymodem.packet_length = 0U;
    return _esh_ymodem_send_packet(esh);
}

static eos_result_t _esh_ymodem_send_data(esh_t *esh)
{
    ssize_t read_length;

    memset(esh->ymodem.packet, _ESH_YMODEM_PADDING, sizeof(esh->ymodem.packet));
    read_length = eos_storage_file_read(esh->ymodem.file, &esh->ymodem.packet[3], _ESH_YMODEM_BLOCK_SIZE);
    if (read_length < 0)
    {
        return _esh_ymodem_finish(esh, false, true);
    }

    if (read_length == 0)
    {
        esh->ymodem.packet_length = 0U;
        esh->ymodem.state = ESH_YMODEM_SEND_WAIT_EOT_NAK;
        return _esh_ymodem_write_control(esh, _ESH_YMODEM_EOT);
    }

    esh->ymodem.packet[0] = _ESH_YMODEM_STX;
    esh->ymodem.packet[1] = esh->ymodem.expected_block;
    esh->ymodem.packet[2] = (uint8_t)~esh->ymodem.expected_block;
    esh->ymodem.file_offset += (uint32_t)read_length;
    esh->ymodem.state = ESH_YMODEM_SEND_WAIT_DATA_ACK;
    return _esh_ymodem_send_packet(esh);
}

static eos_result_t _esh_ymodem_finish(esh_t *esh, bool success, bool send_cancel)
{
    char message[ESH_PRINTF_BUFFER_SIZE];
    int message_length;

    if (send_cancel)
    {
        uint8_t cancel[2] = {_ESH_YMODEM_CAN, _ESH_YMODEM_CAN};
        (void)esh_write_active(esh, cancel, sizeof(cancel));
    }

    if (esh->ymodem.file_open)
    {
        eos_storage_file_close(esh->ymodem.file);
        esh->ymodem.file_open = false;
    }

    message_length = snprintf(message,
                              sizeof(message),
                              "ymodem: %s %s\r\n",
                              success ? "transfer complete:" : "transfer failed:",
                              esh->ymodem.path);
    if (message_length > 0 && (size_t)message_length < sizeof(message))
    {
        (void)esh_write_active(esh, message, (size_t)message_length);
    }

    esh->ymodem.state = ESH_YMODEM_IDLE;
    esh->ymodem.packet_length = 0U;
    esh->ymodem.packet_expected = 0U;
    esh->input_mode = ESH_INPUT_COMMAND;
    return success ? EOS_OK : EOS_ERR_IO;
}

static bool _esh_ymodem_parse_size(const uint8_t *data, size_t length, uint32_t *size)
{
    const uint8_t *separator;
    const uint8_t *cursor;
    uint32_t value = 0U;
    uint8_t digit;
    bool has_digit = false;

    separator = memchr(data, '\0', length);
    if (!separator || separator == data)
    {
        return false;
    }

    cursor = separator + 1U;
    while ((size_t)(cursor - data) < length && *cursor != '\0' && *cursor != ' ')
    {
        if (*cursor < '0' || *cursor > '9')
        {
            return false;
        }

        digit = (uint8_t)(*cursor - '0');
        if (value > (UINT32_MAX - digit) / 10U)
        {
            return false;
        }

        value = value * 10U + digit;
        has_digit = true;
        cursor++;
    }

    if (!has_digit)
    {
        return false;
    }

    *size = value;
    return true;
}

static eos_result_t _esh_ymodem_process_packet(esh_t *esh)
{
    size_t data_size;
    uint16_t received_crc;
    uint16_t calculated_crc;
    uint8_t block;
    size_t write_size;
    ssize_t written;

    data_size = esh->ymodem.packet[0] == _ESH_YMODEM_STX ? _ESH_YMODEM_BLOCK_SIZE : _ESH_YMODEM_DATA_SIZE;
    block = esh->ymodem.packet[1];
    received_crc = ((uint16_t)esh->ymodem.packet[3U + data_size] << 8U) | esh->ymodem.packet[4U + data_size];
    calculated_crc = _esh_ymodem_crc16(&esh->ymodem.packet[3], data_size);

    if ((uint8_t)(block + esh->ymodem.packet[2]) != 0xFFU || received_crc != calculated_crc)
    {
        esh->ymodem.packet_length = 0U;
        return _esh_ymodem_write_control(esh, _ESH_YMODEM_NAK);
    }

    if (esh->ymodem.state == ESH_YMODEM_RECEIVE_WAIT_HEADER)
    {
        if (block != _ESH_YMODEM_HEADER_BLOCK)
        {
            esh->ymodem.packet_length = 0U;
            return _esh_ymodem_write_control(esh, _ESH_YMODEM_NAK);
        }

        if (!_esh_ymodem_parse_size(&esh->ymodem.packet[3], _ESH_YMODEM_DATA_SIZE, &esh->ymodem.file_size))
        {
            esh->ymodem.packet_length = 0U;
            return _esh_ymodem_write_control(esh, _ESH_YMODEM_NAK);
        }

        esh->ymodem.file_offset = 0U;
        esh->ymodem.expected_block = 1U;
        esh->ymodem.state = ESH_YMODEM_RECEIVE_DATA;
        esh->ymodem.packet_length = 0U;
        {
            uint8_t response[2] = {_ESH_YMODEM_ACK, _ESH_YMODEM_CRC};
            return esh_write_active(esh, response, sizeof(response));
        }
    }

    if (block == esh->ymodem.expected_block)
    {
        write_size =
            esh->ymodem.file_size > esh->ymodem.file_offset ? esh->ymodem.file_size - esh->ymodem.file_offset : 0U;
        if (write_size > data_size)
        {
            write_size = data_size;
        }

        if (write_size > 0U)
        {
            written = eos_storage_file_write(esh->ymodem.file, &esh->ymodem.packet[3], write_size);
            if (written != (ssize_t)write_size)
            {
                return _esh_ymodem_finish(esh, false, true);
            }
            esh->ymodem.file_offset += (uint32_t)write_size;
        }

        esh->ymodem.expected_block++;
        esh->ymodem.packet_length = 0U;
        return _esh_ymodem_write_control(esh, _ESH_YMODEM_ACK);
    }

    esh->ymodem.packet_length = 0U;
    if (block == (uint8_t)(esh->ymodem.expected_block - 1U))
    {
        return _esh_ymodem_write_control(esh, _ESH_YMODEM_ACK);
    }

    return _esh_ymodem_write_control(esh, _ESH_YMODEM_NAK);
}

static eos_result_t _esh_ymodem_process_byte(esh_t *esh, uint8_t byte)
{
    if (byte == _ESH_YMODEM_CAN)
    {
        return _esh_ymodem_finish(esh, false, false);
    }

    if (esh->ymodem.state == ESH_YMODEM_SEND_WAIT_C)
    {
        if (byte == _ESH_YMODEM_CRC || byte == _ESH_YMODEM_NAK)
        {
            eos_result_t result;

            esh->ymodem.state = ESH_YMODEM_SEND_WAIT_HEADER_ACK;
            result = _esh_ymodem_send_header(esh);
            return result == EOS_OK ? EOS_OK : _esh_ymodem_finish(esh, false, true);
        }
        return EOS_OK;
    }

    if (esh->ymodem.state == ESH_YMODEM_SEND_WAIT_HEADER_ACK)
    {
        if (byte == _ESH_YMODEM_ACK)
        {
            esh->ymodem.state = ESH_YMODEM_SEND_WAIT_DATA_C;
        }
        else if (byte == _ESH_YMODEM_NAK)
        {
            return _esh_ymodem_send_packet(esh);
        }
        return EOS_OK;
    }

    if (esh->ymodem.state == ESH_YMODEM_SEND_WAIT_DATA_C)
    {
        if (byte == _ESH_YMODEM_CRC || byte == _ESH_YMODEM_NAK)
        {
            return _esh_ymodem_send_data(esh);
        }
        return EOS_OK;
    }

    if (esh->ymodem.state == ESH_YMODEM_SEND_WAIT_DATA_ACK)
    {
        if (byte == _ESH_YMODEM_ACK)
        {
            esh->ymodem.expected_block++;
            return _esh_ymodem_send_data(esh);
        }
        if (byte == _ESH_YMODEM_NAK)
        {
            return _esh_ymodem_send_packet(esh);
        }
        return EOS_OK;
    }

    if (esh->ymodem.state == ESH_YMODEM_SEND_WAIT_EOT_NAK)
    {
        if (byte == _ESH_YMODEM_NAK)
        {
            esh->ymodem.state = ESH_YMODEM_SEND_WAIT_EOT_ACK;
            return _esh_ymodem_write_control(esh, _ESH_YMODEM_EOT);
        }
        if (byte == _ESH_YMODEM_ACK)
        {
            return _esh_ymodem_finish(esh, true, false);
        }
        return EOS_OK;
    }

    if (esh->ymodem.state == ESH_YMODEM_SEND_WAIT_EOT_ACK)
    {
        if (byte == _ESH_YMODEM_ACK)
        {
            return _esh_ymodem_finish(esh, true, false);
        }
        if (byte == _ESH_YMODEM_NAK)
        {
            return _esh_ymodem_write_control(esh, _ESH_YMODEM_EOT);
        }
        return EOS_OK;
    }

    if (esh->ymodem.state == ESH_YMODEM_RECEIVE_WAIT_EOT)
    {
        if (byte == _ESH_YMODEM_EOT)
        {
            eos_result_t result = _esh_ymodem_write_control(esh, _ESH_YMODEM_ACK);

            if (result != EOS_OK)
            {
                return _esh_ymodem_finish(esh, false, true);
            }
            return _esh_ymodem_finish(esh, true, false);
        }
        return EOS_OK;
    }

    if ((esh->ymodem.state == ESH_YMODEM_RECEIVE_WAIT_HEADER || esh->ymodem.state == ESH_YMODEM_RECEIVE_DATA)
        && esh->ymodem.packet_length == 0U)
    {
        if (byte == _ESH_YMODEM_EOT && esh->ymodem.state == ESH_YMODEM_RECEIVE_DATA)
        {
            esh->ymodem.state = ESH_YMODEM_RECEIVE_WAIT_EOT;
            return _esh_ymodem_write_control(esh, _ESH_YMODEM_NAK);
        }

        if (byte == _ESH_YMODEM_SOH || byte == _ESH_YMODEM_STX)
        {
            esh->ymodem.packet[0] = byte;
            esh->ymodem.packet_length = 1U;
            esh->ymodem.packet_expected = byte == _ESH_YMODEM_STX ? 1029U : 133U;
        }
        return EOS_OK;
    }

    if (esh->ymodem.state == ESH_YMODEM_RECEIVE_WAIT_HEADER || esh->ymodem.state == ESH_YMODEM_RECEIVE_DATA)
    {
        if (esh->ymodem.packet_length >= sizeof(esh->ymodem.packet))
        {
            esh->ymodem.packet_length = 0U;
            return _esh_ymodem_write_control(esh, _ESH_YMODEM_NAK);
        }

        esh->ymodem.packet[esh->ymodem.packet_length++] = byte;
        if (esh->ymodem.packet_length == esh->ymodem.packet_expected)
        {
            return _esh_ymodem_process_packet(esh);
        }
    }

    return EOS_OK;
}

eos_result_t esh_ymodem_start_send(struct esh_cmd_ctx *ctx, const char *path)
{
    eos_file_t file;
    uint32_t file_size;

    if (!ctx || !ctx->esh || !path)
    {
        return EOS_ERR_INVALID_ARG;
    }

    file = eos_storage_file_open_read(path);
    if (file == EOS_FILE_INVALID)
    {
        return esh_printf(ctx, "ymodem: cannot open file: %s\r\n", path);
    }

    if (eos_storage_file_size(file, &file_size) != EOS_OK)
    {
        eos_storage_file_close(file);
        return esh_printf(ctx, "ymodem: cannot determine file size: %s\r\n", path);
    }

    memset(&ctx->esh->ymodem, 0, sizeof(ctx->esh->ymodem));
    ctx->esh->ymodem.file = file;
    ctx->esh->ymodem.file_open = true;
    ctx->esh->ymodem.file_size = file_size;
    ctx->esh->ymodem.state = ESH_YMODEM_SEND_WAIT_C;
    ctx->esh->ymodem.expected_block = 1U;
    ctx->esh->ymodem.last_activity_tick = eos_tick_get();
    snprintf(ctx->esh->ymodem.path, sizeof(ctx->esh->ymodem.path), "%s", path);
    ctx->esh->input_mode = ESH_INPUT_YMODEM;
    if (esh_printf(ctx, "ymodem: sending %s, waiting for receiver\r\n", path) != EOS_OK)
    {
        return _esh_ymodem_finish(ctx->esh, false, true);
    }

    return EOS_OK;
}

eos_result_t esh_ymodem_start_receive(struct esh_cmd_ctx *ctx, const char *path)
{
    eos_file_t file;
    uint8_t crc_request = _ESH_YMODEM_CRC;
    eos_result_t result;

    if (!ctx || !ctx->esh || !path)
    {
        return EOS_ERR_INVALID_ARG;
    }

    file = eos_storage_file_open_write(path);
    if (file == EOS_FILE_INVALID)
    {
        return esh_printf(ctx, "ymodem: cannot open destination: %s\r\n", path);
    }

    memset(&ctx->esh->ymodem, 0, sizeof(ctx->esh->ymodem));
    ctx->esh->ymodem.file = file;
    ctx->esh->ymodem.file_open = true;
    ctx->esh->ymodem.state = ESH_YMODEM_RECEIVE_WAIT_HEADER;
    ctx->esh->ymodem.last_activity_tick = eos_tick_get();
    snprintf(ctx->esh->ymodem.path, sizeof(ctx->esh->ymodem.path), "%s", path);
    ctx->esh->input_mode = ESH_INPUT_YMODEM;

    result = esh_printf(ctx, "ymodem: receiving %s, waiting for sender\r\n", path);
    if (result == EOS_OK)
    {
        result = esh_write_active(ctx->esh, &crc_request, sizeof(crc_request));
    }

    if (result != EOS_OK)
    {
        return _esh_ymodem_finish(ctx->esh, false, true);
    }

    return result;
}

eos_result_t esh_ymodem_input(struct esh *esh, const uint8_t *data, size_t length)
{
    eos_result_t result = EOS_OK;
    size_t index;

    if (!esh || (!data && length > 0U))
    {
        return EOS_ERR_INVALID_ARG;
    }

    for (index = 0U; index < length && esh->input_mode == ESH_INPUT_YMODEM; index++)
    {
        esh->ymodem.last_activity_tick = eos_tick_get();
        esh->ymodem.retry_count = 0U;
        result = _esh_ymodem_process_byte(esh, data[index]);
        if (result != EOS_OK && esh->input_mode == ESH_INPUT_YMODEM)
        {
            break;
        }
    }

    return result;
}

void esh_ymodem_poll(struct esh *esh)
{
    uint32_t now;
    eos_result_t result = EOS_OK;

    if (!esh || esh->input_mode != ESH_INPUT_YMODEM)
    {
        return;
    }

    now = eos_tick_get();
    if ((uint32_t)(now - esh->ymodem.last_activity_tick) < _ESH_YMODEM_TIMEOUT_MS)
    {
        return;
    }

    esh->ymodem.last_activity_tick = now;
    if (esh->ymodem.retry_count++ >= _ESH_YMODEM_MAX_RETRIES)
    {
        (void)_esh_ymodem_finish(esh, false, true);
        return;
    }

    switch (esh->ymodem.state)
    {
        case ESH_YMODEM_RECEIVE_WAIT_HEADER:
            result = _esh_ymodem_write_control(esh, _ESH_YMODEM_CRC);
            break;
        case ESH_YMODEM_RECEIVE_WAIT_EOT:
            result = _esh_ymodem_write_control(esh, _ESH_YMODEM_NAK);
            break;
        case ESH_YMODEM_SEND_WAIT_HEADER_ACK:
        case ESH_YMODEM_SEND_WAIT_DATA_ACK:
            result = _esh_ymodem_send_packet(esh);
            break;
        case ESH_YMODEM_SEND_WAIT_EOT_NAK:
        case ESH_YMODEM_SEND_WAIT_EOT_ACK:
            result = _esh_ymodem_write_control(esh, _ESH_YMODEM_EOT);
            break;
        default:
            break;
    }

    if (result != EOS_OK)
    {
        (void)_esh_ymodem_finish(esh, false, true);
    }
}

void esh_ymodem_abort(struct esh *esh)
{
    if (!esh)
    {
        return;
    }

    if (esh->ymodem.file_open)
    {
        eos_storage_file_close(esh->ymodem.file);
    }

    memset(&esh->ymodem, 0, sizeof(esh->ymodem));
    esh->input_mode = ESH_INPUT_COMMAND;
}
