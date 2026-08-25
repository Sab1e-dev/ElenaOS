/**
 * @file eos_service_display.h
 * @brief Display service
 */

#ifndef EOS_SERVICE_DISPLAY_H
#define EOS_SERVICE_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/**
 * @brief Brightness animation duration presets
 */
typedef enum
{
    EOS_DISPLAY_DURATION_OFF = 0,
    EOS_DISPLAY_DURATION_FAST = 100,
    EOS_DISPLAY_DURATION_MEDIUM = 300,
    EOS_DISPLAY_DURATION_SLOW = 500,
} eos_display_duration_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Set brightness
 * @param brightness Brightness value (0-100)
 * @param duration_ms Animation duration in milliseconds,
 * use EOS_DISPLAY_DURATION_OFF for instant change,
 * can use eos_display_duration_t presets
 * @param is_temporary If true, save current brightness and set new brightness temporarily.
 * Call eos_display_restore() to restore to saved brightness.
 */
void eos_display_set_brightness(uint8_t brightness, eos_display_duration_t duration_ms, bool is_temporary);

/**
 * @brief Power on the display
 */
void eos_display_power_on(void);

/**
 * @brief Power off the display
 */
void eos_display_power_off(void);

/**
 * @brief Restore brightness to saved value
 * @param duration_ms Animation duration in milliseconds,
 * use EOS_DISPLAY_DURATION_OFF for instant change,
 * can use eos_display_duration_t presets
 */
void eos_display_restore(eos_display_duration_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SERVICE_DISPLAY_H */
