/**
 * @file eos_device.h
 * @brief Device management header file
 */

#ifndef EOS_DEVICE_H
#define EOS_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef enum
{
    DEV_STATE_NONE = 0, // Uninitialized
    DEV_STATE_READY, // Ready to use
    DEV_STATE_BUSY, // Busy with ongoing operations
    DEV_STATE_ERROR, // Error state, needs attention
} eos_dev_state_t;

/* Public function prototypes --------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* EOS_DEVICE_H */
