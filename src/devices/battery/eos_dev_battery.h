/**
 * @file eos_dev_battery.h
 * @brief Battery device abstraction layer
 *
 * This module provides the device layer interface for battery hardware.
 * The battery service (eos_service_battery) depends on this interface
 * to communicate with battery hardware without direct hardware access.
 *
 * @note Application layer should NOT access this module directly.
 *       Use eos_service_battery API instead.
 */

#ifndef EOS_DEV_BATTERY_H
#define EOS_DEV_BATTERY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_error.h"

/* Public macros ----------------------------------------------*/

#define EOS_BATTERY_CAPACITY_UNDEFINED (UINT32_MAX)

/* Public typedefs --------------------------------------------*/

typedef uint32_t eos_charge_mAh_t;

/**
 * @brief Battery device operation callbacks
 *
 * This structure defines the interface that battery hardware
 * implementation must provide.
 */
typedef struct
{
    void (*request_update)(void); /**< Request battery data update */
} eos_battery_dev_ops_t;

typedef struct
{
    eos_battery_dev_ops_t ops; /**< Battery device operation callbacks */
    eos_charge_mAh_t design_capacity; /**< Battery design capacity in mAh */
} eos_dev_battery_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Register battery device operations
 *
 * Registers the battery hardware implementation with the device layer.
 * This function should be called during system initialization before
 * the battery service is initialized.
 *
 * @param ops Pointer to battery device operations structure
 * @param design_capacity_mah Battery design capacity in mAh
 * @return None
 */
void eos_dev_battery_register(const eos_battery_dev_ops_t *ops, eos_charge_mAh_t design_capacity);

/**
 * @brief Get battery device operations
 *
 * Retrieves the currently registered battery device operations.
 *
 * @return Pointer to battery device operations, or NULL if not registered
 */
eos_dev_battery_t *eos_dev_battery_get_instance(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DEV_BATTERY_H */
