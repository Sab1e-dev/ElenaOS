/**
 * @file esh_command.h
 * @brief Static ESH command registration
 */

#ifndef ESH_COMMAND_H
#define ESH_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stddef.h>

/* Public macros ----------------------------------------------*/

/**
 * @brief Enable linker-section command discovery on supported native targets
 */
#if !defined(ESH_USE_LINKER_SECTION)
#if (defined(__ELF__) && !defined(__EMSCRIPTEN__)) || (defined(__APPLE__) && defined(__MACH__))
#define ESH_USE_LINKER_SECTION 1
#else
#define ESH_USE_LINKER_SECTION 0
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#if defined(__APPLE__)
#define ESH_DETAIL_CMD_SECTION __attribute__((section("__DATA,esh_cmd"), used))
#else
#define ESH_DETAIL_CMD_SECTION __attribute__((section(".esh_cmd"), used))
#endif

#define ESH_DETAIL_CAT_(a, b) a##b
#define ESH_DETAIL_CAT(a, b) ESH_DETAIL_CAT_(a, b)

#if defined(__COUNTER__)
#define ESH_DETAIL_CMD_UNIQUE_ID __COUNTER__
#else
#define ESH_DETAIL_CMD_UNIQUE_ID __LINE__
#endif

/**
 * @brief Export a command descriptor into the ESH command section
 * @param name Command name token
 * @param handler Command handler function
 * @param description Static command description
 */
#define ESH_CMD_EXPORT(name, handler, description)                                 \
    static const esh_command_t ESH_DETAIL_CAT(_esh_cmd_, ESH_DETAIL_CMD_UNIQUE_ID) \
        ESH_DETAIL_CMD_SECTION = {#name, handler, description}
#else
#define ESH_CMD_EXPORT(name, handler, description) \
    static const esh_command_t _esh_cmd_unavailable_##name = {#name, handler, description}
#endif

/* Public typedefs --------------------------------------------*/

typedef struct esh_cmd_ctx esh_cmd_ctx_t;

/**
 * @brief ESH command handler function
 * @param ctx Command execution context
 * @param argc Argument count
 * @param argv Argument vector
 * @return Command-specific result code
 */
typedef int (*esh_command_handler_t)(esh_cmd_ctx_t *ctx, int argc, char *argv[]);

/**
 * @brief Static ESH command descriptor
 */
typedef struct
{
    const char *name;
    esh_command_handler_t handler;
    const char *description;
} esh_command_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Get the linker-discovered command table start
 * @return First command descriptor
 */
const esh_command_t *esh_command_begin(void);

/**
 * @brief Get the linker-discovered command table end
 * @return One-past-last command descriptor
 */
const esh_command_t *esh_command_end(void);

#ifdef __cplusplus
}
#endif

#endif /* ESH_COMMAND_H */
