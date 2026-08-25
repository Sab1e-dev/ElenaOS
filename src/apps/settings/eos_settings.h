/**
 * @file eos_settings.h
 * @brief System settings
 */

#ifndef EOS_SETTINGS_H
#define EOS_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_core.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Enter settings page
 * @return eos_activity_t* Settings page activity pointer, returns NULL on failure
 */
void eos_settings_enter(void);
/**
 * @brief Enable silent mode
 */
void eos_settings_slient_mode_on(void);
/**
 * @brief Disable silent mode
 */
void eos_settings_slient_mode_off(void);
#ifdef __cplusplus
}
#endif

#endif /* EOS_SETTINGS_H */
