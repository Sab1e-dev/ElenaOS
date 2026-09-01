/**
 * @file esh_ymodem.h
 * @brief YMODEM transport for ESH
 */

#ifndef ESH_YMODEM_H
#define ESH_YMODEM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eos_config.h"
#include "eos_fs_port.h"
#include "eos_error.h"

struct esh;
struct esh_cmd_ctx;

/* Macros and Definitions -------------------------------------*/
#define ESH_YMODEM_PACKET_MAX_SIZE 1029U

/* Public typedefs --------------------------------------------*/
/**
 * @brief YMODEM transfer state
 */
typedef enum
{
    ESH_YMODEM_IDLE = 0,
    ESH_YMODEM_SEND_WAIT_C,
    ESH_YMODEM_SEND_WAIT_HEADER_ACK,
    ESH_YMODEM_SEND_WAIT_DATA_C,
    ESH_YMODEM_SEND_WAIT_DATA_ACK,
    ESH_YMODEM_SEND_WAIT_EOT_NAK,
    ESH_YMODEM_SEND_WAIT_EOT_ACK,
    ESH_YMODEM_RECEIVE_WAIT_HEADER,
    ESH_YMODEM_RECEIVE_DATA,
    ESH_YMODEM_RECEIVE_WAIT_EOT
} esh_ymodem_state_t;

/**
 * @brief YMODEM state owned by an ESH instance
 */
typedef struct
{
    esh_ymodem_state_t state;
    eos_file_t file;
    bool file_open;
    uint8_t packet[ESH_YMODEM_PACKET_MAX_SIZE];
    size_t packet_length;
    size_t packet_expected;
    uint8_t expected_block;
    uint32_t file_size;
    uint32_t file_offset;
    uint32_t last_activity_tick;
    uint8_t retry_count;
    char path[EOS_FS_PATH_MAX];
} esh_ymodem_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Start a YMODEM file send
 * @param ctx ESH command context
 * @param path Resolved file path
 * @return EOS_OK when the transfer is started
 */
eos_result_t esh_ymodem_start_send(struct esh_cmd_ctx *ctx, const char *path);

/**
 * @brief Start a YMODEM file receive
 * @param ctx ESH command context
 * @param path Resolved destination path
 * @return EOS_OK when the transfer is started
 */
eos_result_t esh_ymodem_start_receive(struct esh_cmd_ctx *ctx, const char *path);

/**
 * @brief Process bytes received during a YMODEM transfer
 * @param esh ESH instance
 * @param data Received bytes
 * @param length Number of bytes
 * @return EOS_OK or a transfer error
 */
eos_result_t esh_ymodem_input(struct esh *esh, const uint8_t *data, size_t length);

/**
 * @brief Poll a transfer for handshake and retransmission timeouts
 * @param esh ESH instance
 */
void esh_ymodem_poll(struct esh *esh);

/**
 * @brief Abort an active transfer and close its file
 * @param esh ESH instance
 */
void esh_ymodem_abort(struct esh *esh);

#ifdef __cplusplus
}
#endif

#endif /* ESH_YMODEM_H */
