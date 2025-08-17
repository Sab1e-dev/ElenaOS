/**
 * @file elena_os_core.h
 * @brief Elena OS 核心头文件
 * @author Sab1e
 * @date 2025-08-10
 */

#ifndef ELENA_OS_CORE_H
#define ELENA_OS_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
typedef enum {
    ELENA_OS_OK = 0,
    ELENA_OS_ERR_MEM = -1,
    ELENA_OS_ERR_STACK_EMPTY = -2,
    ELENA_OS_ERR_STACK_FULL = -3,
    ELENA_OS_ERR_VAR_NOT_NULL = -4,
    ELENA_OS_ERR_VAR_NULL = -5,
    ELENA_OS_ERR_NOT_INITIALIZED = -6,
    ELENA_OS_ERR_ALREADY_INITIALIZED = -7,
    ELENA_OS_ERR_UNKNOWN = -99,
}ElenaOSResult_t;
/* Public function prototypes --------------------------------*/
/**
 * @brief ElenaOS 入口函数
 * @return ElenaOSResult_t 返回运行结果
 */
ElenaOSResult_t eos_run();
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_CORE_H */
