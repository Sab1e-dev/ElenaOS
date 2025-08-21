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
    EOS_OK = 0,
    EOS_ERR_MEM,
    EOS_ERR_STACK_EMPTY,
    EOS_ERR_STACK_FULL,
    EOS_ERR_VAR_NOT_NULL,
    EOS_ERR_VAR_NULL,
    EOS_ERR_NOT_INITIALIZED,
    EOS_ERR_ALREADY_INITIALIZED,
    EOS_ERR_BUSY,
    EOS_ERR_FILE_ERROR,
    EOS_ERR_JSON_ERROR,
    EOS_ERR_UNKNOWN,
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
