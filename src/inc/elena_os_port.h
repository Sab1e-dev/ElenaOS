/**
 * @file elena_os_port.h
 * @brief ElenaOS 移植
 * @author Sab1e
 * @date 2025-08-21
 */

#ifndef ELENA_OS_PORT_H
#define ELENA_OS_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "elena_os_core.h"
/* Public macros ----------------------------------------------*/
/**
 * @brief 函数弱定义宏
 */
#ifdef __CC_ARM /* ARM Compiler */
    #define EOS_WEAK __weak
#elif defined(__IAR_SYSTEMS_ICC__) /* for IAR Compiler */
    #define EOS_WEAK __weak
#elif defined(__GNUC__) /* GNU GCC Compiler */
    #define EOS_WEAK __attribute__((weak))
#elif defined(__ADSPBLACKFIN__) /* for VisualDSP++ Compiler */
    #define EOS_WEAK __attribute__((weak))
#elif defined(_MSC_VER)
    #define EOS_WEAK
#elif defined(__TI_COMPILER_VERSION__)
    #define EOS_WEAK
#else
    #error not supported tool chain
#endif
/**
 * @brief UNUSED 宏
 */
#define EOS_UNUSED(x) (void)(x)

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief 内存分配函数
 * @param size 要分配的内存大小
 * @return void* 分配成功则返回内存地址，否则返回 NULL
 * @note 主要用于图片内存分配
 */
void *eps_malloc_large(size_t size);
/**
 * @brief 释放目标内存
 * @param ptr 目标指针
 */
void eps_free_large(void *ptr);
/**
 * @brief 延时指定时间（非阻塞）
 * @param ms 毫秒数
 */
void eos_delay(uint32_t ms);
/**
 * @brief 系统重置
 */
void eos_cpu_reset();
/**
 * @brief 启用蓝牙
 */
void eos_bluetooth_enable(void);
/**
 * @brief 关闭蓝牙
 */
void eos_bluetooth_disable(void);
/**
 * @brief 获取当前时间结构体
 * @return eos_datetime_t 时间结构体
 * @note 推荐使用RTC获取时间
 * @warning 请自行同步时间，确保获取的是准确时间
 */
eos_datetime_t eos_time_get(void);
/**
 * @brief 设置屏幕亮度
 * @param brightness 亮度值（0~100）
 */
void eos_display_set_brightness(uint8_t brightness);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_PORT_H */
