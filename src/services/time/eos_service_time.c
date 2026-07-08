/**
 * @file eos_service_time.c
 * @brief Time service
 */

#include "eos_service_time.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#define EOS_LOG_TAG "ServiceTime"
#include "eos_log.h"
#include "eos_dev_time.h"
#include "eos_core.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

eos_result_t eos_service_time_init(void)
{
    EOS_LOG_I("Initialized");
    return EOS_OK;
}

eos_datetime_t eos_time_get(void)
{
    static eos_datetime_t last_sec_time;
    static uint32_t sec_base_tick = 0;
    static uint8_t initialized = 0;

    eos_dev_time_t *dev = eos_dev_time_get_instance();
    if (dev->ops == NULL || dev->ops->get_datetime == NULL)
    {
        EOS_LOG_E("Time device OPS not available");
        eos_datetime_t dt = {0};
        return dt;
    }

    eos_datetime_t now = dev->ops->get_datetime();
    uint32_t tick = eos_tick_get();

    if (!initialized || now.sec != last_sec_time.sec || now.min != last_sec_time.min || now.hour != last_sec_time.hour
        || now.day != last_sec_time.day || now.month != last_sec_time.month || now.year != last_sec_time.year)
    {
        sec_base_tick = tick;
        last_sec_time = now;
        initialized = 1;
    }

    uint32_t ms = tick - sec_base_tick;
    if (ms >= 1000)
    {
        ms = 999;
    }

    now.ms = (uint16_t)ms;
    return now;
}
