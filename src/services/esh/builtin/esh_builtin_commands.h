/**
 * @file esh_builtin_commands.h
 * @brief Categorized ESH built-in command handlers
 */

#ifndef ESH_BUILTIN_COMMANDS_H
#define ESH_BUILTIN_COMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esh.h"

/* Public function prototypes ---------------------------------*/

bool esh_builtin_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size);
bool esh_builtin_resolve_path(const esh_t *esh, const char *path, char *resolved, size_t resolved_size);

/* Debug commands ---------------------------------------------*/
int esh_builtin_cmd_log(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_mem(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_stack(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_crashlog(esh_cmd_ctx_t *ctx, int argc, char *argv[]);

/* Hardware commands ------------------------------------------*/
int esh_builtin_cmd_sensor(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_battery(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_power(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_display(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_touch(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_ui(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_time(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_vibrator(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_audio(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_ble(esh_cmd_ctx_t *ctx, int argc, char *argv[]);

/* System and application commands ----------------------------*/
int esh_builtin_cmd_apps(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_recent(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_app(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_js(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_config(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_state(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_pkg(esh_cmd_ctx_t *ctx, int argc, char *argv[]);

/* Platform-independent utility commands ----------------------*/
int esh_builtin_cmd_find(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_grep(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_head(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_tail(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_wc(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_crc32(esh_cmd_ctx_t *ctx, int argc, char *argv[]);
int esh_builtin_cmd_sha256(esh_cmd_ctx_t *ctx, int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* ESH_BUILTIN_COMMANDS_H */
