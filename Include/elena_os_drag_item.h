/**
 * @file elena_os_drag_item.h
 * @brief 拖拽控件头文件
 * @author Sab1e
 * @date 2025-08-10
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
/**
 * @brief DragItem 拖拽的方向
 */
typedef enum{
    DRAG_DIR_UP=0,      // 从下往上滑拉出 drag_obj
    DRAG_DIR_DOWN=1,    // 从上往下滑拉出 drag_obj
    DRAG_DIR_LEFT=2,    // 从右往左滑拉出 drag_obj
    DRAG_DIR_RIGHT=3    // 从左往右滑拉出 drag_obj
} drag_dir_t;
/**
 * @brief DragItem 结构体定义
 */
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
/**
 * @brief 删除 DragItem
 * @param drag_item 拖拽控件
 */
void eos_drag_item_del(drag_item_t *drag_item);

/**
 * @brief 创建 DragItem
 * @param parent 拖拽控件的父级对象
 * @return 指向创建成功的 DragItem
 * @note 不使用时需要使用 eos_drag_item_del 删除此控件，否则可能内存泄漏
 */
drag_item_t *eos_drag_item_create(lv_obj_t *parent);

/**
 * @brief 设置拖拽方向
 * @param drag_item 拖拽控件
 * @param dir 拖拽方向，例如 DRAG_DIR_DOWN 就是向下拖拽拉出 drag_obj
 */
void eos_drag_item_set_dir(drag_item_t *drag_item, const drag_dir_t dir);

/**
 * @brief 隐藏 TouchBar (小白条)
 * @param drag_item 拖拽控件
 */
void eos_drag_item_hide_touch_bar(drag_item_t* drag_item);

/**
 * @brief 显示 TouchBar (小白条)
 * @param drag_item 拖拽控件
 */
void eos_drag_item_show_touch_bar(drag_item_t* drag_item);

/**
 * @brief 外部触发拉回动画，自动拉回屏幕外面
 * @param drag_item 拖拽控件
 */
void eos_drag_item_pull_back(drag_item_t *drag_item);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_drag_item_H */
