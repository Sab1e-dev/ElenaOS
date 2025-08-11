
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

#include <stdint.h>
#include <stdbool.h>

// 类型声明

// 函数声明
void script_engine_register_natives();

#ifdef __cplusplus
}
#endif

#endif // SCRIPT_ENGINE_NATIVE_FUNC_H
