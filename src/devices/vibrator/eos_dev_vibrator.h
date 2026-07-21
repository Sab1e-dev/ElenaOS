/**
 * @file eos_dev_vibrator.h
 * @brief Vibrator device
 */

#ifndef EOS_DEV_VIBRATOR_H
#define EOS_DEV_VIBRATOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_device.h"
#include "eos_error.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef struct
{
    void (*on)(uint8_t strength); /**< Strength range: 0-255 */
    void (*off)(void);
} eos_dev_vibrator_ops_t;

typedef struct
{
    const eos_dev_vibrator_ops_t *ops;
    eos_dev_state_t _state;
} eos_dev_vibrator_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Get vibrator device instance
 * @return Vibrator device instance
 */
eos_dev_vibrator_t *eos_dev_vibrator_get_instance(void);

/**
 * @brief Register vibrator device with OPS
 * @param ops Pointer to vibrator OPS structure
 * @return EOS_OK if successful, error code otherwise
 */
eos_result_t eos_dev_vibrator_register(const eos_dev_vibrator_ops_t *ops);

/**
 * @brief Get vibrator device state
 * @return Current device state
 */
eos_dev_state_t eos_dev_vibrator_get_state(void);

/**
 * @brief Report vibrator device state (called by driver)
 * @param state New device state
 */
void eos_dev_vibrator_report_state(eos_dev_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DEV_VIBRATOR_H */
