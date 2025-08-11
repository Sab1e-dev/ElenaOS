/*
 * @file       elena_os_core.h
 * @brief      Elena OS 核心头文件
 * @author     Sab1e
 * @date       2025-08-10
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
    ELENA_OS_ERR_UNKNOWN = -99,
}ElenaOSResult_t;
/* Public function prototypes --------------------------------*/
ElenaOSResult_t elena_os_run();
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_CORE_H */
