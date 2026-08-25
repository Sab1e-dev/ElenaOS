/**
 * @file eos_dev_power.h
 * @brief Power device
 */

#ifndef EOS_DEV_POWER_H
#define EOS_DEV_POWER_H

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
typedef enum
{
    DEV_POWER_STATE_OFF = 0,
    DEV_POWER_STATE_ON,
    DEV_POWER_STATE_SLEEP,
    DEV_POWER_STATE_AOD,
} dev_power_state_t;

typedef struct
{
    int (*set_power)(dev_power_state_t state);
} eos_dev_power_ops_t;

typedef struct
{
    const eos_dev_power_ops_t *ops;
    eos_dev_state_t _state;
} eos_dev_power_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Get power device instance
 * @return Power device instance
 */
eos_dev_power_t *eos_dev_power_get_instance(void);

/**
 * @brief Register power device with OPS
 * @param ops Pointer to power OPS structure
 * @return EOS_OK if successful, error code otherwise
 */
eos_result_t eos_dev_power_register(const eos_dev_power_ops_t *ops);

/**
 * @brief Get power device state
 * @return Current device state
 */
eos_dev_state_t eos_dev_power_get_state(void);

/**
 * @brief Report power device state (called by driver)
 * @param state New device state
 */
void eos_dev_power_report(eos_dev_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DEV_POWER_H */
