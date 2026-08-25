/**
 * @file eos_service_haptic.h
 * @brief Haptic service
 */

#ifndef EOS_SERVICE_HAPTIC_H
#define EOS_SERVICE_HAPTIC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_dev_vibrator.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef enum
{
    EOS_HAPTIC_STRENGTH_OFF = 0,
    EOS_HAPTIC_STRENGTH_NORMAL = 128,
    EOS_HAPTIC_STRENGTH_INTENSE = 255,
} eos_haptic_strength_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Short and light tick vibration
 */
void eos_haptic_tick(void);

/**
 * @brief Slightly stronger and longer vibration
 */
void eos_haptic_buzz(void);

/**
 * @brief Strongest and longest vibration
 */
void eos_haptic_vibrate_long(void);

/**
 * @brief Haptic service initialization
 */
void eos_service_haptic_init(void);

/**
 * @brief Set global haptic strength
 */
void eos_haptic_set_strength(eos_haptic_strength_t s);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SERVICE_HAPTIC_H */
