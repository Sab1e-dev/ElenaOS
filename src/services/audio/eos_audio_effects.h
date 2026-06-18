/**
 * @file eos_audio_effects.h
 * @brief VAR audio effects registry - enum and descriptor array
 */

#ifndef EOS_AUDIO_EFFECTS_H
#define EOS_AUDIO_EFFECTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_audio_decoder.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef enum
{
    EOS_AUDIO_EFFECT_CLICK = 0,
    EOS_AUDIO_EFFECT_NOTIFICATION,
    EOS_AUDIO_EFFECT_ALARM,
    EOS_AUDIO_EFFECT_RECORD_START,
    EOS_AUDIO_EFFECT_RECORD_STOP,
    EOS_AUDIO_EFFECT_CORRECT,
    /* Add new effects above this line */
    EOS_AUDIO_EFFECT_COUNT
} eos_audio_effect_id_t;

/* Public function prototypes --------------------------------*/

/**
 * @brief Get VAR audio descriptor by effect ID
 * @param id Effect ID from eos_audio_effect_id_t
 * @return Pointer to descriptor, or NULL if not available
 */
const eos_audio_dsc_t *eos_audio_effect_get(eos_audio_effect_id_t id);

/**
 * @brief Get human-readable name for an effect
 * @param id Effect ID
 * @return Name string, or "Unknown" if invalid
 */
const char *eos_audio_effect_get_name(eos_audio_effect_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* EOS_AUDIO_EFFECTS_H */
