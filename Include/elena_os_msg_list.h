/**
 * @file elena_os_msg_list.h
 * @brief 下拉消息列表
 * @author Sab1e
 * @date 2025-08-13
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

typedef struct msg_list_t msg_list_t;   // 预定义

/**
 * @brief 滑动状态判断
 */
typedef enum {
    SWIPE_UNKNOWN = 0,  /**< 未知类型 */
    SWIPE_HORIZONTAL,   /**< 水平滑动 */
    SWIPE_VERTICAL,     /**< 垂直滑动 */
    SWIPE_CLICK         /**< 点击 */
} swipe_type_t;

/**
 * @brief 滑动删除数据结构
 */
typedef struct {
    int32_t start_x;
    int32_t start_y;
    int32_t start_translate_x;
    swipe_type_t swipe_type;
} swipe_data_t;
/**
 * @brief 消息列表项结构体
 * 层级结构：
 * container{
 * row1[icon_area title_label   time_label]
 *      msg_label
 * }
 */
typedef struct {
    msg_list_t *msg_list;
    lv_obj_t *container;
    lv_obj_t *row1;
    lv_obj_t *icon_area;
    lv_obj_t *title_label;
    lv_obj_t *msg_label;
    lv_obj_t *time_label;
    char *msg_str;     /**< 消息字符串 */
    swipe_data_t swipe_data;
    bool is_deleted;
    lv_point_t press_pos;
} msg_list_item_t;

/**
 * @brief 消息列表结构体
 * 层级结构：
 * drag_obj{
 *      list{
 *          clear_all_btn
 *          msg_item
 *          ...
 *          msg_item
 *      }
 *      no_msg_label
 * }
 */
struct msg_list_t {
    drag_item_t *drag_item;     /**< 拖拽对象指针 */
    lv_obj_t *list;             /**< 列表对象指针 */
    lv_obj_t *clear_all_btn;    /**< 清除所有消息按钮指针 */
    lv_obj_t *no_msg_label;     /**< 无消息时的提示标签 */
    uint16_t animating_count;   /**< 一键清除按下后，动画中的消息数量 */ 
};

/* Public function prototypes --------------------------------*/
/**
 * @brief 创建消息项
 * @param list 消息项的父消息列表
 * @return msg_list_item_t* 指向创建的消息项指针（动态内存分配）
 */
msg_list_item_t *eos_msg_list_item_create(msg_list_t *list);
/**
 * @brief 删除消息项
 * @param item 要删除的消息项指针
 */
void eos_msg_list_item_delete(msg_list_item_t *item);
/**
 * @brief 设置消息内容
 * @param item 目标消息项
 * @param msg 消息字符串
 */
void eos_msg_list_item_set_msg(msg_list_item_t *item, const char *msg);

/**
 * @brief 设置标题
 * @param item 目标消息项
 * @param title 消息标题（APP）字符串
 */
void eos_msg_list_item_set_title(msg_list_item_t *item, const char *title);

/**
 * @brief 设置时间文本
 * @param item 目标消息项
 * @param time 消息接收时间字符串（例：“12:30”、“一小时前”）
 */
void eos_msg_list_item_set_time(msg_list_item_t *item, const char *time);

/**
 * @brief 设置图标
 * @param item 目标消息项
 * @param icon_dsc LVGL 图片描述符
 */
void eos_msg_list_item_set_icon_dsc(msg_list_item_t *item, const lv_img_dsc_t *icon_dsc);

/**
 * @brief 设置图标
 * @param item 目标消息项
 * @param icon_obj LVGL 图片对象指针
 */
void eos_msg_list_item_set_icon_obj(msg_list_item_t *item, lv_obj_t *icon_obj);
/**
 * @brief 删除消息列表
 * @param list 目标列表
 */
void eos_msg_list_delete(msg_list_t *list);
/**
 * @brief 创建消息列表
 * @param parent 消息列表的父级对象指针
 * @return msg_list_t* 返回创建的消息列表指针（动态内存分配）
 */
msg_list_t *eos_msg_list_create(lv_obj_t *parent);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_MSG_LIST_H */
