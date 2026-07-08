/**
 * @file eos_dev_microphone.c
 * @brief Microphone device
 */
#include "eos_dev_microphone.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "DevMicrophone"
#include "eos_log.h"
#include "eos_error.h"

/* Variables --------------------------------------------------*/

static eos_dev_microphone_t _dev_microphone = {
    .ops = NULL,
    ._state = DEV_STATE_NONE,
};

/* Function Implementations -----------------------------------*/

eos_dev_microphone_t *eos_dev_microphone_get_instance(void)
{
    return &_dev_microphone;
}

eos_result_t eos_dev_microphone_register(const eos_dev_microphone_ops_t *ops)
{
    if (ops == NULL)
    {
        EOS_LOG_E("OPS is NULL");
        return EOS_ERR_INVALID_ARG;
    }

    if (_dev_microphone._state != DEV_STATE_NONE)
    {
        EOS_LOG_W("Already registered");
        return EOS_ERR_ALREADY_EXISTS;
    }

    if (ops->open == NULL || ops->close == NULL || ops->start == NULL || ops->stop == NULL || ops->set_buffer == NULL
        || ops->get_write_offset == NULL || ops->is_available == NULL)
    {
        EOS_LOG_E("OPS incomplete: open/close/start/stop/set_buffer/get_write_offset/is_available required");
        return EOS_ERR_INVALID_ARG;
    }

    _dev_microphone.ops = ops;
    _dev_microphone._state = DEV_STATE_READY;
    EOS_LOG_I("Microphone registered");
    return EOS_OK;
}

eos_dev_state_t eos_dev_microphone_get_state(void)
{
    return _dev_microphone._state;
}

void eos_dev_microphone_report_state(eos_dev_state_t state)
{
    if (_dev_microphone._state == state)
    {
        return;
    }
    EOS_LOG_I("State: %d -> %d", _dev_microphone._state, state);
    _dev_microphone._state = state;
}
