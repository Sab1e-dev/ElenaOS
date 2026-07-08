/**
 * @file eos_dev_battery.c
 * @brief Battery device abstraction layer implementation
 */

#include "eos_dev_battery.h"

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#define EOS_LOG_TAG "DevBattery"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/
/**
 * @brief Battery device singleton instance
 */
static eos_dev_battery_t _battery_dev = {0};

/* Function Implementations -----------------------------------*/

void eos_dev_battery_register(const eos_battery_dev_ops_t *ops, eos_charge_mAh_t design_capacity)
{
    if (!ops)
    {
        EOS_LOG_E("Register failed: ops is NULL");
        return;
    }

    if (!ops->request_update)
    {
        EOS_LOG_E("Register failed: request_update is required");
        return;
    }

    _battery_dev.ops = *ops;
    _battery_dev.design_capacity = design_capacity;

    if (design_capacity != EOS_BATTERY_CAPACITY_UNDEFINED)
    {
        EOS_LOG_I("Battery device registered, design capacity: %d mAh", design_capacity);
    }
    else
    {
        EOS_LOG_E("Battery device registered with undefined design capacity");
    }
}

eos_dev_battery_t *eos_dev_battery_get_instance(void)
{
    return &_battery_dev;
}
