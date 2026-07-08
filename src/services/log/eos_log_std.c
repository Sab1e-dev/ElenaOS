/**
 * @file eos_log_std.c
 * @brief Standard output log listener
 */

#include "eos_log.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_error.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static const char *_log_level_to_str(eos_log_level_t level)
{
    switch (level)
    {
        case EOS_LOG_LEVEL_DEBUG:
            return "DEBUG";
        case EOS_LOG_LEVEL_INFO:
            return "INFO";
        case EOS_LOG_LEVEL_WARN:
            return "WARN";
        case EOS_LOG_LEVEL_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

static const char *_log_level_to_color(eos_log_level_t level)
{
    switch (level)
    {
        case EOS_LOG_LEVEL_DEBUG:
            return EOS_LOG_COLOR_CYAN;
        case EOS_LOG_LEVEL_INFO:
            return EOS_LOG_COLOR_GREEN;
        case EOS_LOG_LEVEL_WARN:
            return EOS_LOG_COLOR_YELLOW;
        case EOS_LOG_LEVEL_ERROR:
            return EOS_LOG_COLOR_RED;
        default:
            return EOS_LOG_COLOR_RESET;
    }
}

static void _std_log_listener(eos_log_level_t level, const char *buf, size_t len, void *user)
{
    (void)user;
    (void)len;

#if EOS_LOG_USE_COLOR
    printf("%s[%s] %s%s\n", _log_level_to_color(level), _log_level_to_str(level), buf, EOS_LOG_COLOR_RESET);
#else
    printf("[%s] %s\n", _log_level_to_str(level), buf);
#endif

    fflush(stdout);
}

eos_result_t eos_service_log_std_register(void)
{
    eos_log_listener_id_t id = eos_log_register_listener("std_log", _std_log_listener, NULL, EOS_LOG_FLAG_SYSTEM);

    return (id >= 0) ? EOS_OK : EOS_FAILED;
}
