/**
 * @file eos_input.h
 * @brief Input handling header file
 */

#ifndef EOS_INPUT_H
#define EOS_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/**
 * @brief Button press state
 */
typedef enum
{
    EOS_BUTTON_STATE_CLICKED,
    EOS_BUTTON_STATE_PRESSED,
    EOS_BUTTON_STATE_LONG_PRESSED,
    EOS_BUTTON_STATE_RELEASED,
    EOS_BUTTON_STATE_DOUBLE_CLICKED
} eos_button_state_t;

/* Public function prototypes ---------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* EOS_INPUT_H */
