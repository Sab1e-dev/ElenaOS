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
#include "mem_mgr.h"
#include "rtthread.h"
#include "elena_os_core.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief 内存分配函数
 * @param size 要分配的内存大小
 * @return void* 分配成功则返回内存地址，否则返回 NULL
 * @note 主要用于图片内存分配
 */
void *eos_alloc(size_t size);
/**
 * @brief 释放目标内存
 * @param ptr 目标指针
 */
void eos_free(void *ptr);
/**
 * @brief 延时指定时间（非阻塞）
 * @param ms 毫秒数
 */
void eos_delay(uint32_t ms);
/**
 * @brief 系统重置
 */
void eos_cpu_reset();

#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_PORT_H */
