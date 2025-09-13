/**
 * @file elena_os_log.h
 * @brief 日志系统
 * @author Sab1e
 * @date 2025-08-14
 */

#ifndef ELENA_OS_LOG_H
#define ELENA_OS_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "msh.h"
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

// 是否启用断言
#ifndef EOS_USE_ASSERT
#define EOS_USE_ASSERT    1
#endif

// 是否启用彩色日志
#ifndef EOS_LOG_USE_COLOR
#define EOS_LOG_USE_COLOR 1
#endif

/************************** 彩色日志 **************************/
#if EOS_LOG_USE_COLOR
#define EOS_COLOR_RESET   "\033[0m"
#define EOS_COLOR_RED     "\033[31m"
#define EOS_COLOR_GREEN   "\033[32m"
#define EOS_COLOR_YELLOW  "\033[33m"
#define EOS_COLOR_BLUE    "\033[34m"
#define EOS_COLOR_CYAN    "\033[36m"
#else
#define EOS_COLOR_RESET   ""
#define EOS_COLOR_RED     ""
#define EOS_COLOR_GREEN   ""
#define EOS_COLOR_YELLOW  ""
#define EOS_COLOR_BLUE    ""
#define EOS_COLOR_CYAN    ""
#endif


#define EOS_LOG_BASE(level, color, fmt, ...) \
    printf("[%s] " fmt "\n", \
           level, ##__VA_ARGS__)

#define EOS_LOG_ALL(level, color, fmt, ...) \
    printf(color "[%s:%d %s()] %s: " fmt EOS_COLOR_RESET "\n", \
           __FILE__, __LINE__, __func__, level, ##__VA_ARGS__)

/************************** 日志宏 **************************/

#define EOS_LOG_D(fmt, ...) EOS_LOG_BASE("DEBUG", EOS_COLOR_CYAN, fmt, ##__VA_ARGS__)
#define EOS_LOG_I(fmt, ...) EOS_LOG_BASE("INFO",  EOS_COLOR_GREEN, fmt, ##__VA_ARGS__)
#define EOS_LOG_W(fmt, ...) EOS_LOG_BASE("WARN",  EOS_COLOR_YELLOW, fmt, ##__VA_ARGS__)
#define EOS_LOG_E(fmt, ...) EOS_LOG_ALL("ERROR", EOS_COLOR_RED, fmt, ##__VA_ARGS__)

/************************** 内存检查 **************************/

#define EOS_MEM(tag) \
    do { \
        printf("[MEM] %s\n", tag); \
        msh_exec("list_mem", 8); \
        msh_exec("list_memheap", 12); \
    } while(0)

/************************** 指针检查 **************************/    
#define EOS_CHECK_PTR_RETURN(ptr) \
    do { \
        if (!(ptr)) { \
            EOS_LOG_E("NULL pointer at %s:%d, at function: %s", __FILE__, __LINE__, __func__); \
            return; \
        } \
    } while(0)

#define EOS_CHECK_PTR_RETURN_FREE(ptr,free_var) \
    do { \
        if (!(ptr)) { \
            EOS_LOG_E("NULL pointer at %s:%d, at function: %s", __FILE__, __LINE__, __func__); \
            lv_free(free_var); \
            return; \
        } \
    } while(0)

#define EOS_CHECK_PTR_RETURN_VAL(ptr, ret_val) \
    do { \
        if (!(ptr)) { \
            EOS_LOG_E("NULL pointer at %s:%d, at function: %s", __FILE__, __LINE__, __func__); \
            return ret_val; \
        } \
    } while(0)


#define EOS_CHECK_PTR_RETURN_VAL_FREE(ptr, ret_val, free_var) \
    do { \
        if (!(ptr)) { \
            EOS_LOG_E("NULL pointer at %s:%d, at function: %s", __FILE__, __LINE__, __func__); \
            lv_free(free_var); \
            return ret_val; \
        } \
    } while(0)

/************************** 断言宏 **************************/
#if EOS_USE_ASSERT
#define EOS_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            EOS_LOG_E("Assertion failed: %s", #expr); \
            while(1); \
        } \
    } while(0)
#else
#define EOS_ASSERT(expr) ((void)0)
#endif

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_LOG_H */
