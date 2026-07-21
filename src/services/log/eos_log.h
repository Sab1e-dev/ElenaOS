/**
 * @file eos_log.h
 * @brief Listener-based log system
 */

#ifndef EOS_LOG_H
#define EOS_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include "eos_config.h"
#include "eos_error.h"

/* Public macros ----------------------------------------------*/

/* Log Listener Flags -----------------------------------------*/
#define EOS_LOG_FLAG_SYSTEM (1 << 0) /**< System listener, cannot be deleted */

/* Colorful Log -----------------------------------------------*/
#if EOS_LOG_USE_COLOR
#define EOS_LOG_COLOR_RESET "\033[0m"
#define EOS_LOG_COLOR_RED "\033[31m"
#define EOS_LOG_COLOR_GREEN "\033[32m"
#define EOS_LOG_COLOR_YELLOW "\033[33m"
#define EOS_LOG_COLOR_BLUE "\033[34m"
#define EOS_LOG_COLOR_CYAN "\033[36m"
#else
#define EOS_LOG_COLOR_RESET ""
#define EOS_LOG_COLOR_RED ""
#define EOS_LOG_COLOR_GREEN ""
#define EOS_LOG_COLOR_YELLOW ""
#define EOS_LOG_COLOR_BLUE ""
#define EOS_LOG_COLOR_CYAN ""
#endif /* EOS_LOG_USE_COLOR */

/* Log Macros -------------------------------------------------*/

#ifdef EOS_LOG_DISABLE

#define EOS_LOG_D(fmt, ...) \
    do                      \
    {                       \
    } while (0)
#define EOS_LOG_I(fmt, ...) \
    do                      \
    {                       \
    } while (0)
#define EOS_LOG_W(fmt, ...) \
    do                      \
    {                       \
    } while (0)
#define EOS_LOG_E(fmt, ...) \
    do                      \
    {                       \
    } while (0)

#else

#ifdef EOS_LOG_TAG
#define EOS_LOG_FMT(fmt) "[" EOS_LOG_TAG "] " fmt
#else
#define EOS_LOG_FMT(fmt) fmt
#endif /* EOS_LOG_TAG */

#define EOS_LOG_D(fmt, ...) eos_log(EOS_LOG_LEVEL_DEBUG, EOS_LOG_FMT(fmt), ##__VA_ARGS__)
#define EOS_LOG_I(fmt, ...) eos_log(EOS_LOG_LEVEL_INFO, EOS_LOG_FMT(fmt), ##__VA_ARGS__)
#define EOS_LOG_W(fmt, ...) eos_log(EOS_LOG_LEVEL_WARN, EOS_LOG_FMT(fmt), ##__VA_ARGS__)
#define EOS_LOG_E(fmt, ...) eos_log(EOS_LOG_LEVEL_ERROR, EOS_LOG_FMT(fmt), ##__VA_ARGS__)

#endif /* EOS_LOG_DISABLE */

/* Pointer Check ----------------------------------------------*/
#define EOS_CHECK_PTR_RETURN(ptr)                                                              \
    do                                                                                         \
    {                                                                                          \
        if (!(ptr))                                                                            \
        {                                                                                      \
            EOS_LOG_E("NULL pointer at %s:%d, at function: %s", __FILE__, __LINE__, __func__); \
            return;                                                                            \
        }                                                                                      \
    } while (0)

#define EOS_CHECK_PTR_RETURN_FREE(ptr, free_var)                                               \
    do                                                                                         \
    {                                                                                          \
        if (!(ptr))                                                                            \
        {                                                                                      \
            EOS_LOG_E("NULL pointer at %s:%d, at function: %s", __FILE__, __LINE__, __func__); \
            eos_free(free_var);                                                                \
            return;                                                                            \
        }                                                                                      \
    } while (0)

#define EOS_CHECK_PTR_RETURN_VAL(ptr, ret_val)                                                 \
    do                                                                                         \
    {                                                                                          \
        if (!(ptr))                                                                            \
        {                                                                                      \
            EOS_LOG_E("NULL pointer at %s:%d, at function: %s", __FILE__, __LINE__, __func__); \
            return ret_val;                                                                    \
        }                                                                                      \
    } while (0)

#define EOS_CHECK_PTR_RETURN_VAL_FREE(ptr, ret_val, free_var)                                  \
    do                                                                                         \
    {                                                                                          \
        if (!(ptr))                                                                            \
        {                                                                                      \
            EOS_LOG_E("NULL pointer at %s:%d, at function: %s", __FILE__, __LINE__, __func__); \
            eos_free(free_var);                                                                \
            return ret_val;                                                                    \
        }                                                                                      \
    } while (0)

/* Assertion Macros -------------------------------------------*/
#if EOS_USE_ASSERT
#define EOS_ASSERT(expr)                              \
    do                                                \
    {                                                 \
        if (!(expr))                                  \
        {                                             \
            EOS_LOG_E("Assertion failed: %s", #expr); \
            EOS_ASSERT_HANDLER                        \
        }                                             \
    } while (0)
#else
#define EOS_ASSERT(expr) ((void)0)
#endif /* EOS_USE_ASSERT */

/* Print Coordinates ------------------------------------------*/

#define EOS_DEBUG_PRINT_POS(obj) EOS_LOG_D("Obj[%p] (%d,%d)", obj, lv_obj_get_x(obj), lv_obj_get_y(obj))

/* Public typedefs --------------------------------------------*/

typedef int8_t eos_log_listener_id_t;

/**
 * @brief Log level enumeration
 */
typedef enum
{
    EOS_LOG_LEVEL_DEBUG = 0, /**< Debug level */
    EOS_LOG_LEVEL_INFO, /**< Info level */
    EOS_LOG_LEVEL_WARN, /**< Warning level */
    EOS_LOG_LEVEL_ERROR /**< Error level */
} eos_log_level_t;

/**
 * @brief Log listener callback function type
 * @param level Log level
 * @param buf Log message buffer
 * @param len Length of log message
 * @param user User data
 */
typedef void (*eos_log_listener_cb_t)(eos_log_level_t level, const char *buf, size_t len, void *user);

/**
 * @brief Log listener structure
 */
typedef struct
{
    const char *name; /**< Listener name (static string) */
    eos_log_listener_cb_t cb; /**< Callback function */
    void *user_data; /**< User data */
    uint8_t used; /**< Whether this slot is used */
    uint8_t flags; /**< Listener flags */
} eos_log_listener_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize log system
 * @return EOS_OK on success
 */
void eos_service_log_init(void);

/**
 * @brief Register a log listener
 * @param name Listener name (must be static string)
 * @param cb Callback function
 * @param user_data User data
 * @param flags Listener flags
 * @return Listener ID (>=0) on success, -1 on failure
 */
eos_log_listener_id_t eos_log_register_listener(const char *name,
                                                eos_log_listener_cb_t cb,
                                                void *user_data,
                                                uint8_t flags);

/**
 * @brief Unregister a log listener by ID
 * @param id Listener ID
 * @return EOS_OK on success
 */
eos_result_t eos_log_unregister_listener(eos_log_listener_id_t id);

/**
 * @brief Find a log listener by name
 * @param name Listener name
 * @return Listener ID (>=0) on success, -1 if not found
 */
eos_log_listener_id_t eos_log_find_listener(const char *name);

/**
 * @brief Get listener information by ID
 * @param id Listener ID
 * @param listener Output listener structure
 * @return EOS_OK on success
 */
eos_result_t eos_log_get_listener(eos_log_listener_id_t id, eos_log_listener_t *listener);

/**
 * @brief Format and dispatch log message
 * @param level Log level
 * @param fmt Format string
 * @param ... Variable arguments
 */
void eos_log(eos_log_level_t level, const char *fmt, ...);

/**
 * @brief Dispatch log message to all listeners (internal use)
 * @param level Log level
 * @param buf Log message buffer
 * @param len Length of log message
 */
void eos_log_dispatch(eos_log_level_t level, const char *buf, size_t len);

/**
 * @brief Register standard output log listener
 * @return EOS_OK on success
 */
eos_result_t eos_service_log_std_register(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_LOG_H */
