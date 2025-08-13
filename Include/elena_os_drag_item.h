/*
 * @file       elena_os_drag_item.h
 * @brief      拖拽控件头文件
 * @author     Sab1e
 * @date       2025-08-10
 */

#ifndef ELENA_OS_drag_item_H
#define ELENA_OS_drag_item_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "elena_os_core.h"
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
typedef enum{
    DRAG_DIR_UP=0,      // 从下往上滑
    DRAG_DIR_DOWN=1,    // 从上往下滑
    DRAG_DIR_LEFT=2,    // 从右往左滑
    DRAG_DIR_RIGHT=3    // 从左往右滑
} drag_dir_t;

typedef struct {
    lv_obj_t* drag_obj;
    lv_obj_t* gesture_area;
    lv_obj_t* touch_bar;
    lv_coord_t start_x;
    lv_coord_t start_y;
    lv_coord_t touch_start_x;
    lv_coord_t touch_start_y;
    drag_dir_t dir;
    bool dragging;
} drag_item_t;
/* Public function prototypes --------------------------------*/
drag_item_t *elena_os_drag_item_create(lv_obj_t *parent);
void elena_os_drag_item_set_dir(drag_item_t *drag_item, const drag_dir_t dir);
void elena_os_drag_item_hide_touch_bar(drag_item_t* drag_item);
void elena_os_drag_item_show_touch_bar(drag_item_t* drag_item);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_drag_item_H */
