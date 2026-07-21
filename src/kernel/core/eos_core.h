/**
 * @file eos_core.h
 * @brief Elenix OS core header file
 */

#ifndef EOS_CORE_H
#define EOS_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_error.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief ElenixOS initialization function
 */
void eos_init(void);
/**
 * @brief Check whether ElenixOS core has completed initialization
 */
bool eos_is_initialized(void);
/**
 * @brief ElenixOS main loop function
 */
uint32_t eos_main_loop(void);
/**
 * @brief Get the elapsed milliseconds since start up
 * @return Elapsed milliseconds
 */
uint32_t eos_tick_get(void);
/**
 * @brief Start ElenixOS Logo page
 * @param anim Whether to display Logo fade animation
 * @note Will only be displayed once and will remain until the system is fully started.
 */
void eos_logo_play(bool anim);
#ifdef __cplusplus
}
#endif

#endif /* EOS_CORE_H */
