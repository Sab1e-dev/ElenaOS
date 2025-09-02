/**
 * @file elena_os_clock.c
 * @brief 时钟服务
 * @author Sab1e
 * @date 2025-08-28
 */

#include "elena_os_clock.h"

// Includes
#include <stdio.h>
#include <stdlib.h>

// Macros and Definitions
#define TIME_STR_MAX 32 // 事件字符串最大字节数
typedef struct
{
    uint64_t timestamp;

    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint16_t ms;

    bool is_12h;
    bool dst; /**< 夏令时 */
} eos_clock_t;

// Variables
static eos_clock_t clock = {0};
// Function Implementations

uint8_t eos_clock_get_hour(void)
{
    return clock.hour;
}

uint8_t eos_clock_get_min(void)
{
    return clock.min;
}

uint8_t eos_clock_get_sec(void)
{
    return clock.sec;
}

uint8_t eos_clock_get_ms(void)
{
    // TODO
    return clock.ms;
}

char *eos_colck_get_time_str(bool show_sec)
{
    clock.hour = 9;
    clock.min++;
    clock.sec = 0;
    static char buf[TIME_STR_MAX];
    if(show_sec){
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             clock.hour, clock.min, clock.sec);
    }else{
        snprintf(buf, sizeof(buf), "%02d:%02d",
             clock.hour, clock.min);
    }
    
    return buf;
}


void eos_clock_calibrate_time(uint64_t timestamp_ms)
{
}

void eos_clock_save_cfg(void)
{
}