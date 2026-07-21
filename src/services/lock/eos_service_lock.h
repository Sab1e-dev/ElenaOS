/**
 * @file eos_service_lock.h
 * @brief Lock screen service - manages lock screen lifecycle
 */

#ifndef EOS_SERVICE_LOCK_H
#define EOS_SERVICE_LOCK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize lock screen service
 * Subscribes to display-on events and shows lock screen when password is set
 */
void eos_service_lock_init(void);

/**
 * @brief Check if lock screen is currently active
 * @return true if lock screen overlay is displayed
 */
bool eos_lock_screen_is_active(void);

/**
 * @brief Dismiss lock screen after successful unlock
 */
void eos_lock_screen_dismiss(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SERVICE_LOCK_H */
