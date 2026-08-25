/**
 * @file eos_side_button.h
 * @brief Side button
 */

#ifndef EOS_SIDE_BUTTON_H
#define EOS_SIDE_BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_core.h"
#include "input/eos_input.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/
/**
 * @brief Report side button state
 * @param state State value
 */
void eos_side_button_report(eos_button_state_t state);
#ifdef __cplusplus
}
#endif

#endif /* EOS_SIDE_BUTTON_H */
