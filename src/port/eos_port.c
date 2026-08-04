/**
 * @file eos_port.c
 * @brief ElenixOS porting
 */

#include "eos_port.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_log.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/
EOS_WEAK void eos_delay(uint32_t ms)
{
    LV_UNUSED(ms);
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

EOS_WEAK void eos_locate_phone(void)
{
    return;
}

EOS_WEAK size_t eos_port_get_free_mem(void)
{
    return 0;
}
