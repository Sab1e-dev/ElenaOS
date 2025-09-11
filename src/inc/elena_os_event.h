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
/**
 * @brief 全局广播事件类型定义
 * @note 此处可添加新的事件
 */
typedef enum{
    EOS_EVENT_SWIPE_PANEL_TOUCH_LOCK=0,
    EOS_EVENT_SWIPE_PANEL_TOUCH_UNLOCK,
    EOS_EVENT_THEME_UPDATED,
    EOS_EVENT_APP_DELETED,
    EOS_EVENT_APP_INSTALLED,
    /* 此处添加新的事件 */
    EOS_EVENT_MAX_NUMBER
} eos_event_t;
/* Public function prototypes --------------------------------*/

/**
 * @brief 初始化事件系统
 */
void eos_event_init(void);

/**
 * @brief 根据事件枚举获取事件码（由LVGL分配的事件码）
 * @param e 事件枚举类型
 * @return uint32_t 事件码
 */
uint32_t eos_event_get_code(eos_event_t e);

/**
 * @brief 添加事件回调
 * @param obj 对象指针
 * @param event 事件类型
 * @param cb 回调函数
 * @param user_data 用户数据
 * @note 如果事件类型是由 LVGL 分配的，则直接传入即可，
 * 例如`LV_EVENT_ALL`；如果事件类型是 ElenaOS 分配的，则需要使用
 * `eos_event_get_code`获取事件号才能传入。
 * 
 * 示例：
 * 
 * `eos_event_add_cb(obj,cb,LV_EVENT_ALL,NULL);`
 * 
 * `eos_event_add_cb(obj,cb,eos_event_get_code(EOS_EVENT_THEME_UPDATED),NULL)`
 * 
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
