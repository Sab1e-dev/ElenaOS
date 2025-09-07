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
    EOS_FAILED,
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
    EOS_ERR_VALUE_MISMATCH,
    EOS_ERR_UNKNOWN,
} eos_result_t;

typedef enum{
    SIDE_BTN_CLICKED,
    SIDE_BTN_PRESSED,
    SIDE_BTN_LONG_PRESSED,
    SIDE_BTN_RELEASED,
    SIDE_BTN_DOUBLE_CLICKED
} eos_side_btn_state_t;

/**
 * @brief 时间结构体定义
 */
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day_of_week;
}eos_datetime_t;
/* Public function prototypes --------------------------------*/
/**
 * @brief ElenaOS 入口函数
 * @return eos_result_t 返回运行结果
 */
eos_result_t eos_run();
void eos_side_btn_handler(eos_side_btn_state_t state);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_CORE_H */
