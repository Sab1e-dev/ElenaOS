/**
 * @file esh.h
 * @brief Lightweight Elenix Shell core
 */

#ifndef ESH_H
#define ESH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eos_config.h"
#include "eos_error.h"
#include "esh_command.h"
#include "esh_ymodem.h"

/* Public macros ----------------------------------------------*/

/** @brief Maximum command line length including the terminator */
#ifndef ESH_LINE_MAX
#define ESH_LINE_MAX 128U
#endif

/** @brief Maximum number of command arguments */
#ifndef ESH_MAX_ARGS
#define ESH_MAX_ARGS 16U
#endif

/** @brief Size of the optional formatted output buffer */
#ifndef ESH_PRINTF_BUFFER_SIZE
#define ESH_PRINTF_BUFFER_SIZE 128U
#endif

/** @brief Default ESH prompt */
#ifndef ESH_PROMPT
#define ESH_PROMPT "ESH> "
#endif

/** @brief Input was sent by a frontend that no longer owns ESH */
#define ESH_ERR_NOT_OWNER ((eos_result_t) - 800)

/** @brief ESH has no active frontend */
#define ESH_ERR_NO_OWNER ((eos_result_t) - 801)

/** @brief An ESH API was called recursively */
#define ESH_ERR_REENTRANT ((eos_result_t) - 802)

/** @brief The caller deliberately cancelled a claim */
#define ESH_ERR_CANCELLED ((eos_result_t) - 803)

/** @brief The command line contains too many arguments */
#define ESH_ERR_TOO_MANY_ARGS ((eos_result_t) - 804)

/** @brief The command line exceeded the configured line buffer */
#define ESH_ERR_LINE_TOO_LONG ((eos_result_t) - 805)

/* Public typedefs --------------------------------------------*/

typedef struct esh esh_t;

/**
 * @brief Frontend output callback
 * @param data Bytes to write
 * @param length Number of bytes
 * @param user_data Frontend-owned context
 * @return Number of bytes accepted
 */
typedef size_t (*esh_frontend_write_cb_t)(const uint8_t *data, size_t length, void *user_data);

/**
 * @brief Reason an existing frontend lost ESH ownership
 */
typedef enum
{
    ESH_CLOSE_RELEASE = 0,
    ESH_CLOSE_TAKEOVER
} esh_close_reason_t;

/**
 * @brief Frontend close notification
 * @param reason Close reason
 * @param user_data Frontend-owned context
 */
typedef void (*esh_frontend_closed_cb_t)(esh_close_reason_t reason, void *user_data);

/**
 * @brief Static or caller-owned ESH frontend descriptor
 */
typedef struct
{
    const char *name;
    esh_frontend_write_cb_t write;
    esh_frontend_closed_cb_t on_closed;
    void *user_data;
} esh_frontend_t;

/**
 * @brief Input ownership token
 */
typedef struct
{
    const esh_frontend_t *frontend;
    uint32_t generation;
} esh_owner_token_t;

/**
 * @brief Action selected by a frontend when it wants ESH control
 */
typedef enum
{
    ESH_CLAIM_CANCEL = 0,
    ESH_CLAIM_TAKEOVER
} esh_claim_action_t;

/**
 * @brief ESH input parser mode
 */
typedef enum
{
    ESH_INPUT_COMMAND = 0,
    ESH_INPUT_YMODEM
} esh_input_mode_t;

/**
 * @brief Command execution context
 */
struct esh_cmd_ctx
{
    esh_t *esh;
    const esh_frontend_t *frontend;
    esh_owner_token_t owner;
};

/**
 * @brief ESH runtime state
 */
struct esh
{
    char line[ESH_LINE_MAX];
    char *argv[ESH_MAX_ARGS];
    char cwd[EOS_FS_PATH_MAX];
    esh_ymodem_t ymodem;

    size_t line_length;
    bool line_overflow;
    bool ignore_lf;
    bool input_busy;
    bool interleaved_output;
    bool release_requested;
    bool prompt_visible;
    esh_input_mode_t input_mode;

    const esh_command_t *commands_begin;
    const esh_command_t *commands_end;

    const esh_frontend_t *active_frontend;
    uint32_t active_generation;
    bool owner_active;
};

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize ESH using linker-discovered commands
 * @param esh ESH instance owned by the caller
 * @return EOS_OK on success
 */
eos_result_t esh_init(esh_t *esh);

/**
 * @brief Initialize ESH using an explicit static command range
 * @param esh ESH instance owned by the caller
 * @param commands_begin First command descriptor
 * @param commands_end One-past-last command descriptor
 * @return EOS_OK on success
 */
eos_result_t esh_init_with_commands(esh_t *esh, const esh_command_t *commands_begin, const esh_command_t *commands_end);

/**
 * @brief Claim exclusive ESH input ownership
 * @param esh ESH instance
 * @param frontend Requesting frontend
 * @param action TAKEOVER or CANCEL
 * @param out_token Output token for the new owner
 * @return EOS_OK, ESH_ERR_CANCELLED, or an error code
 */
eos_result_t esh_claim(esh_t *esh,
                       const esh_frontend_t *frontend,
                       esh_claim_action_t action,
                       esh_owner_token_t *out_token);

/**
 * @brief Release ESH ownership using the active owner's token
 * @param esh ESH instance
 * @param token Owner token
 * @return EOS_OK on success
 */
eos_result_t esh_release(esh_t *esh, esh_owner_token_t token);

/**
 * @brief Check whether a token is the current owner
 * @param esh ESH instance
 * @param token Token to check
 * @return true if the token owns ESH
 */
bool esh_is_owner(const esh_t *esh, esh_owner_token_t token);

/**
 * @brief Push input bytes from the current owner frontend
 * @param esh ESH instance
 * @param token Owner token
 * @param data Input bytes
 * @param length Number of input bytes
 * @return EOS_OK or an input/output error
 */
eos_result_t esh_input(esh_t *esh, esh_owner_token_t token, const uint8_t *data, size_t length);

/**
 * @brief Poll the active ESH transfer state
 * @param esh ESH instance
 */
void esh_poll(esh_t *esh);

/**
 * @brief Write bytes to the current active frontend
 * @param esh ESH instance
 * @param data Output bytes
 * @param length Number of output bytes
 * @return EOS_OK or an output error
 */
eos_result_t esh_write_active(esh_t *esh, const void *data, size_t length);

/**
 * @brief Begin a block of asynchronous output for the active frontend
 *
 * If the command prompt and an unsubmitted line are visible, ESH clears them
 * before the caller writes the block. The caller must finish the block with
 * esh_interleaved_end().
 *
 * @param esh ESH instance
 * @return EOS_OK or an ownership/output error
 */
eos_result_t esh_interleaved_begin(esh_t *esh);

/**
 * @brief Finish a block of asynchronous output and redraw the input line
 * @param esh ESH instance
 * @return EOS_OK or an ownership/output error
 */
eos_result_t esh_interleaved_end(esh_t *esh);

/**
 * @brief Write bytes from a command context
 * @param ctx Command execution context
 * @param data Output bytes
 * @param length Number of output bytes
 * @return EOS_OK or an output error
 */
eos_result_t esh_write(esh_cmd_ctx_t *ctx, const void *data, size_t length);

/**
 * @brief Write formatted text from a command context
 * @param ctx Command execution context
 * @param format printf-style format string
 * @return EOS_OK or an output error
 */
eos_result_t esh_printf(esh_cmd_ctx_t *ctx, const char *format, ...);

/**
 * @brief Request release after the current command returns
 * @param ctx Command execution context
 * @return EOS_OK on success
 */
eos_result_t esh_request_release(esh_cmd_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ESH_H */
