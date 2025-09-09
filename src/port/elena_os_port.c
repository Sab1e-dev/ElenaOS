/**
 * @file elena_os_port.c
 * @brief ElenaOS 移植
 * @author Sab1e
 * @date 2025-08-21
 */

#include "elena_os_port.h"

// Includes
#include <stdio.h>
#include <stdlib.h>

// Macros and Definitions

// Variables

// Function Implementations

EOS_WEAK void *eps_malloc_large(size_t size)
{
    return malloc(size);
}

EOS_WEAK void eps_free_large(void *ptr)
{
    free(ptr);
}

EOS_WEAK void eos_delay(uint32_t ms)
{
    return;
}

EOS_WEAK void eos_cpu_reset(void)
{
    return;
}

EOS_WEAK void eos_bluetooth_enable(void)
{
    return;
}

EOS_WEAK void eos_bluetooth_disable(void)
{
    return;
}

EOS_WEAK eos_datetime_t eos_time_get(void)
{
    /* EXAMPLE */
    eos_datetime_t dt = {0};
    dt.year = 2025;
    dt.month = 9;
    dt.day = 7;
    dt.hour = 18;
    dt.minute = 26;
    dt.second = 13;
    dt.day_of_week = 7;
    return dt;
}