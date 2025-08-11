/**
 * @file elena_os_core.c
 * @brief Elena OS 核心代码实现
 * @author Sab1e
 * @date 2025-08-10
 */

#include "elena_os_core.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "elena_os_drag_item.h"
#include "lvgl.h"
// Macros and Definitions

// Static Variables

// Function Implementations
ElenaOSResult_t elena_os_run(){
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_align(btn,LV_ALIGN_CENTER,0,0);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label,"09:15");
    lv_obj_center(label);
    drag_item_t *drag_item = elena_os_drag_item_create(scr);
    elena_os_drag_item_set_dir(drag_item,DRAG_DIR_LEFT);
    while(1){
        uint32_t d = lv_timer_handler();
        lv_delay_ms(d);
        // 注册 LVGL 事件
        // 表盘主页面
    }
}