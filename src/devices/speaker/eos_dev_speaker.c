/**
 * @file eos_dev_speaker.c
 * @brief Speaker device
 */

#include "eos_dev_speaker.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "DevSpeaker"
#include "eos_log.h"
#include "eos_error.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

static eos_dev_speaker_t _dev_speaker = {
    .ops = NULL,
    ._state = DEV_STATE_NONE,
};

/* Function Implementations -----------------------------------*/

eos_dev_speaker_t *eos_dev_speaker_get_instance(void)
{
    return &_dev_speaker;
}

eos_result_t eos_dev_speaker_register(const eos_dev_speaker_ops_t *ops)
{
    if (ops == NULL)
    {
        EOS_LOG_E("OPS is NULL");
        return EOS_ERR_INVALID_ARG;
    }

    if (_dev_speaker._state != DEV_STATE_NONE)
    {
        EOS_LOG_W("Already registered");
        return EOS_ERR_ALREADY_EXISTS;
    }

    if (ops->open == NULL || ops->borrow == NULL || ops->enqueue == NULL ||
        ops->stop == NULL || ops->set_volume == NULL ||
        ops->is_available == NULL)
    {
        EOS_LOG_E("OPS incomplete: open, borrow, enqueue, stop, set_volume, is_available are required");
        return EOS_ERR_INVALID_ARG;
    }

    _dev_speaker.ops = ops;
    _dev_speaker._state = DEV_STATE_READY;
    EOS_LOG_I("Speaker registered");
    return EOS_OK;
}

eos_dev_state_t eos_dev_speaker_get_state(void)
{
    return _dev_speaker._state;
}

void eos_dev_speaker_report_state(eos_dev_state_t state)
{
    if (_dev_speaker._state == state)
    {
        return;
    }
    EOS_LOG_I("State: %d -> %d", _dev_speaker._state, state);
    _dev_speaker._state = state;
}
