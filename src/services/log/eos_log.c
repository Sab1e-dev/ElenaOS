/**
 * @file eos_log.c
 * @brief Listener-based log system implementation
 */

#include "eos_log.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "eos_error.h"
#include "eos_config.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_BUFFER_SIZE 1024

/* Variables --------------------------------------------------*/
static eos_log_listener_t s_listeners[EOS_LOG_MAX_LISTENERS] = {0};
static bool _initialized = false;

/* Function Implementations -----------------------------------*/

void eos_service_log_init(void)
{
    if (_initialized)
    {
        return;
    }

    memset(s_listeners, 0, sizeof(s_listeners));
    _initialized = true;
#if EOS_LOG_ENABLE_STD
    eos_service_log_std_register();
#endif
}

eos_log_listener_id_t eos_log_register_listener(const char *name,
                                                eos_log_listener_cb_t cb,
                                                void *user_data,
                                                uint8_t flags)
{
    if (!_initialized || !name || !cb)
    {
        return -1;
    }

    for (int i = 0; i < EOS_LOG_MAX_LISTENERS; i++)
    {
        if (!s_listeners[i].used)
        {
            s_listeners[i].name = name;
            s_listeners[i].cb = cb;
            s_listeners[i].user_data = user_data;
            s_listeners[i].flags = flags;
            s_listeners[i].used = 1;
            s_listeners[i].enabled = 1;
            return i;
        }
    }

    return -1;
}

eos_result_t eos_log_unregister_listener(eos_log_listener_id_t id)
{
    if (!_initialized || id < 0 || id >= EOS_LOG_MAX_LISTENERS)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (!s_listeners[id].used)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (s_listeners[id].flags & EOS_LOG_FLAG_SYSTEM)
    {
        return EOS_ERR_INVALID_ARG;
    }

    memset(&s_listeners[id], 0, sizeof(eos_log_listener_t));
    return EOS_OK;
}

eos_result_t eos_log_set_listener_enabled(eos_log_listener_id_t id, bool enabled)
{
    if (!_initialized || id < 0 || id >= EOS_LOG_MAX_LISTENERS)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (!s_listeners[id].used)
    {
        return EOS_ERR_INVALID_ARG;
    }

    s_listeners[id].enabled = enabled ? 1U : 0U;
    return EOS_OK;
}

eos_log_listener_id_t eos_log_find_listener(const char *name)
{
    if (!_initialized || !name)
    {
        return -1;
    }

    for (int i = 0; i < EOS_LOG_MAX_LISTENERS; i++)
    {
        if (s_listeners[i].used && s_listeners[i].name && strcmp(s_listeners[i].name, name) == 0)
        {
            return i;
        }
    }

    return -1;
}

eos_result_t eos_log_get_listener(eos_log_listener_id_t id, eos_log_listener_t *listener)
{
    if (!_initialized || !listener || id < 0 || id >= EOS_LOG_MAX_LISTENERS)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (!s_listeners[id].used)
    {
        return EOS_ERR_INVALID_ARG;
    }

    memcpy(listener, &s_listeners[id], sizeof(eos_log_listener_t));
    return EOS_OK;
}

void eos_log_dispatch(eos_log_level_t level, const char *buf, size_t len)
{
    if (!_initialized || !buf || len == 0)
    {
        return;
    }

    eos_log_listener_t snapshot[EOS_LOG_MAX_LISTENERS];
    memcpy(snapshot, s_listeners, sizeof(snapshot));

    for (int i = 0; i < EOS_LOG_MAX_LISTENERS; i++)
    {
        if (snapshot[i].used && snapshot[i].enabled && snapshot[i].cb)
        {
            snapshot[i].cb(level, buf, len, snapshot[i].user_data);
        }
    }
}

void eos_log(eos_log_level_t level, const char *fmt, ...)
{
    if (!_initialized || !fmt)
    {
        return;
    }

    static char buffer[EOS_LOG_BUFFER_SIZE];
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(buffer, EOS_LOG_BUFFER_SIZE, fmt, args);
    va_end(args);

    if (len > 0 && len < EOS_LOG_BUFFER_SIZE)
    {
        eos_log_dispatch(level, buffer, (size_t)len);
    }
}
