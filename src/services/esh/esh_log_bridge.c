/**
 * @file esh_log_bridge.c
 * @brief EOS_LOG bridge for the active ESH frontend
 */

#include "esh_log_bridge.h"

/* Includes ---------------------------------------------------*/
#include <string.h>

#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/
static esh_t *s_esh = NULL;
static eos_log_listener_id_t s_listener_id = -1;
static eos_log_listener_id_t s_std_listener_id = -1;
static bool s_std_listener_enabled = false;

/* Function Implementations -----------------------------------*/

static void _esh_log_listener(eos_log_level_t level, const char *buf, size_t len, void *user_data)
{
    esh_t *esh = user_data;
    const char *level_name;

    switch (level)
    {
        case EOS_LOG_LEVEL_DEBUG:
            level_name = "DEBUG";
            break;
        case EOS_LOG_LEVEL_INFO:
            level_name = "INFO";
            break;
        case EOS_LOG_LEVEL_WARN:
            level_name = "WARN";
            break;
        case EOS_LOG_LEVEL_ERROR:
            level_name = "ERROR";
            break;
        default:
            level_name = "UNKNOWN";
            break;
    }

    if (esh && esh == s_esh && esh->owner_active && buf && len > 0U && esh_interleaved_begin(esh) == EOS_OK)
    {
        esh_write_active(esh, "[", 1U);
        esh_write_active(esh, level_name, strlen(level_name));
        esh_write_active(esh, "] ", 2U);
        esh_write_active(esh, buf, len);
        esh_write_active(esh, "\r\n", 2U);
        esh_interleaved_end(esh);
    }
}

eos_result_t esh_log_bridge_attach(esh_t *esh)
{
    if (!esh)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (!esh->owner_active)
    {
        return ESH_ERR_NO_OWNER;
    }

    if (s_listener_id >= 0)
    {
        return (s_esh == esh) ? EOS_OK : EOS_ERR_BUSY;
    }

    s_std_listener_id = eos_log_find_listener("std_log");
    if (s_std_listener_id >= 0)
    {
        eos_log_listener_t listener;

        if (eos_log_get_listener(s_std_listener_id, &listener) != EOS_OK)
        {
            s_std_listener_id = -1;
            return EOS_ERR_BUSY;
        }

        s_std_listener_enabled = listener.enabled != 0U;
        if (eos_log_set_listener_enabled(s_std_listener_id, false) != EOS_OK)
        {
            s_std_listener_id = -1;
            return EOS_ERR_BUSY;
        }
    }

    s_listener_id = eos_log_register_listener("esh_frontend", _esh_log_listener, esh, 0);
    if (s_listener_id < 0)
    {
        if (s_std_listener_id >= 0)
        {
            eos_log_set_listener_enabled(s_std_listener_id, s_std_listener_enabled);
            s_std_listener_id = -1;
        }
        return EOS_ERR_BUSY;
    }

    s_esh = esh;
    return EOS_OK;
}

eos_result_t esh_log_bridge_detach(void)
{
    eos_result_t result;

    if (s_listener_id < 0)
    {
        s_esh = NULL;
        return EOS_OK;
    }

    result = eos_log_unregister_listener(s_listener_id);
    if (result == EOS_OK)
    {
        if (s_std_listener_id >= 0)
        {
            eos_log_set_listener_enabled(s_std_listener_id, s_std_listener_enabled);
        }
        s_listener_id = -1;
        s_std_listener_id = -1;
        s_std_listener_enabled = false;
        s_esh = NULL;
    }

    return result;
}
