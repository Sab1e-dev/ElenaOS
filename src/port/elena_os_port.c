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
#include <string.h>
#include "elena_os_log.h"
#include "ble_mgr.h"
// Macros and Definitions

// Variables

// Function Implementations

void *eos_malloc(size_t size)
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

void eos_cpu_reset(void)
{
    // rt_hw_cpu_reset();
}

void eos_ble_enable(void)
{
    // TODO: 启动BLE广播
}

void eos_ble_disable(void)
{
    // TODO: 断开所有连接，关闭BLE广播
}

// TODO: 设置定时器自动校准RTC，通过蓝牙校准

eos_datetime_t eos_time_get(void)
{
    // TODO: 通过RTC获取时间
    // time.year = 2025;
    // time.month = 9;
    // time.day = 7;
    // time.hour = 18;
    // time.minute = 26;
    // time.second = 13;
    // time.day_of_week = 7;
    return ble_mgr_get_current_time();
}