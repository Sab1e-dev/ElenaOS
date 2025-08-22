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
#include "elena_os_log.h"
// Macros and Definitions

// Variables

// Function Implementations

void *eos_mem_alloc(size_t size)
{
    return mem_mgr_alloc(size);
}

void eos_mem_free(void *ptr)
{
    mem_mgr_free(ptr);
}

void eos_delay(uint32_t ms)
{
    rt_thread_mdelay(ms);
}

void eos_cpu_reset()
{
    // rt_hw_cpu_reset();
}
