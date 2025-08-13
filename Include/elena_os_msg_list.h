/*
 * @file       elena_os_msg_list.h
 * @brief      下拉消息列表
 * @author     Sab1e
 * @date       2025-08-13
 */

#ifndef ELENA_OS_MSG_LIST_H
#define ELENA_OS_MSG_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "elena_os_drag_item.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
// 消息列表项结构体
typedef struct {
    lv_obj_t* container;
    lv_obj_t* icon_area;
    lv_obj_t* title_label;
    lv_obj_t* msg_label;
    lv_obj_t* time_label;
    lv_obj_t* unread_mark;
} msg_list_item_t;

// 消息列表结构体
typedef struct {
    drag_item_t* drag_item;
    lv_obj_t* list;
} msg_list_t;

/* Public function prototypes --------------------------------*/
// 创建消息项
msg_list_item_t* elena_os_msg_list_item_create(lv_obj_t* parent);

// 删除消息项
void elena_os_msg_list_item_delete(msg_list_item_t* item);

// 设置消息内容
void elena_os_msg_list_item_set_msg(msg_list_item_t* item, const char* msg);

// 设置标题
void elena_os_msg_list_item_set_title(msg_list_item_t* item, const char* title);

// 设置时间文本
void elena_os_msg_list_item_set_time(msg_list_item_t* item, const char* time);

// 设置图标（传入图像描述符）
void elena_os_msg_list_item_set_icon_dsc(msg_list_item_t* item, const lv_img_dsc_t* icon_dsc);
void elena_os_msg_list_item_set_icon_obj(msg_list_item_t* item, lv_obj_t* icon_obj);
// 设置是否未读状态
void elena_os_msg_list_item_set_unread(msg_list_item_t* item, bool unread);
void elena_os_msg_list_delete(msg_list_t* list);
msg_list_t* elena_os_msg_list_create(lv_obj_t* parent);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_MSG_LIST_H */
