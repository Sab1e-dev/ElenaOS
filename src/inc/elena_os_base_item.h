/**
 * @file elena_os_base_item.h
 * @brief 基本控件
 * @author Sab1e
 * @date 2025-08-17
 */

#ifndef ELENA_OS_BASE_ITEM_H
#define ELENA_OS_BASE_ITEM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
typedef struct{
    lv_obj_t *container;
    lv_obj_t *clock_label;
    lv_obj_t *title_label;
    lv_obj_t *back_btn;
    lv_timer_t *clock_timer;
}eos_app_header_t;
/* Public function prototypes --------------------------------*/

/**
 * @brief 创建一个返回按钮
 * @param parent 父对象
 * @param show_text 是否显示返回文字
 * @return lv_obj_t* 创建成功则返回 btn 对象，否则返回 NULL
 */
lv_obj_t *eos_back_btn_create(lv_obj_t *parent, bool show_text);
lv_obj_t * eos_list_add_button(lv_obj_t * list, const void * icon, const char * txt);
void eos_app_header_set_title(const char *title);
void eos_app_header_hide(void);
void eos_app_header_show(void);
void eos_app_header_init(void);
void eos_screen_bind_header(lv_obj_t *scr, const char *title);
lv_obj_t *eos_list_add_placeholder(lv_obj_t *list, uint32_t height);
lv_obj_t *eos_list_add_switch(lv_obj_t *list, const char *txt);
lv_obj_t *eos_list_add_circle_icon_button(lv_obj_t *list, lv_color_t circle_color, const void *icon, const char *txt);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_BASE_ITEM_H */
