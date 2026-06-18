/**
 * @file eos_audio_effects.c
 * @brief VAR audio effects registry - links enum IDs to descriptors
 */

#include "eos_audio_effects.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "AudioEffects"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

extern const eos_audio_dsc_t eos_audio_effect_click;
extern const eos_audio_dsc_t eos_audio_effect_notification;
extern const eos_audio_dsc_t eos_audio_effect_alarm;
extern const eos_audio_dsc_t eos_audio_effect_record_start;
extern const eos_audio_dsc_t eos_audio_effect_record_stop;
extern const eos_audio_dsc_t eos_audio_effect_correct;

static const eos_audio_dsc_t *_s_effects[EOS_AUDIO_EFFECT_COUNT] = {
    [EOS_AUDIO_EFFECT_CLICK]        = &eos_audio_effect_click,
    [EOS_AUDIO_EFFECT_NOTIFICATION] = &eos_audio_effect_notification,
    [EOS_AUDIO_EFFECT_ALARM]        = &eos_audio_effect_alarm,
    [EOS_AUDIO_EFFECT_RECORD_START] = &eos_audio_effect_record_start,
    [EOS_AUDIO_EFFECT_RECORD_STOP]  = &eos_audio_effect_record_stop,
    [EOS_AUDIO_EFFECT_CORRECT]      = &eos_audio_effect_correct,
};

static const char *_s_effect_names[EOS_AUDIO_EFFECT_COUNT] = {
    [EOS_AUDIO_EFFECT_CLICK]        = "Click",
    [EOS_AUDIO_EFFECT_NOTIFICATION] = "Notification",
    [EOS_AUDIO_EFFECT_ALARM]        = "Alarm",
    [EOS_AUDIO_EFFECT_RECORD_START] = "Record Start",
    [EOS_AUDIO_EFFECT_RECORD_STOP]  = "Record Stop",
    [EOS_AUDIO_EFFECT_CORRECT]      = "Correct Answer",
};

/* Function Implementations -----------------------------------*/

const eos_audio_dsc_t *eos_audio_effect_get(eos_audio_effect_id_t id)
{
    if (id >= EOS_AUDIO_EFFECT_COUNT)
    {
        return NULL;
    }
    return _s_effects[id];
}

const char *eos_audio_effect_get_name(eos_audio_effect_id_t id)
{
    if (id >= EOS_AUDIO_EFFECT_COUNT)
    {
        return "Unknown";
    }
    return _s_effect_names[id];
}
