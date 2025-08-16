
/**
 * @file script_engine_native_func.h
 * @brief 注册原生函数的头文件
 * @author Sab1e
 * @date 2025-07-26
 */
#ifndef SCRIPT_ENGINE_NATIVE_FUNC_H
#define SCRIPT_ENGINE_NATIVE_FUNC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/
/**
 * @brief 注册 Native 函数
 * 
 */
void script_engine_register_natives();

#ifdef __cplusplus
}
#endif

#endif // SCRIPT_ENGINE_NATIVE_FUNC_H
