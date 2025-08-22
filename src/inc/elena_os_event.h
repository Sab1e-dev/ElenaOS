/**
 * @file elena_os_event.h
 * @brief 事件系统
 * @author Sab1e
 * @date 2025-08-17
 */

#ifndef ELENA_OS_EVENT_H
#define ELENA_OS_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief 初始化事件系统
 */
void eos_event_init(void);

/**
 * @brief 添加事件回调
 * @param obj 对象指针
 * @param event 事件类型
 * @param cb 回调函数
 * @param user_data 用户数据
 */
void eos_event_add_cb(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t event, void *user_data);

/**
 * @brief 移除事件回调
 * @param obj 对象指针
 * @param event 事件类型
 * @param cb 回调函数
 */
void eos_event_remove_cb(lv_obj_t *obj, lv_event_code_t event, lv_event_cb_t cb);

/**
 * @brief 广播事件
 * @param event 要广播的事件类型
 * @param param 事件参数
 */
void eos_event_broadcast(lv_event_code_t event, void *param);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_EVENT_H */
