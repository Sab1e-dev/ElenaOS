/**
 * @file elena_os_swipe_panel.h
 * @brief 拖拽控件头文件
 * @author Sab1e
 * @date 2025-08-10
 */

#ifndef ELENA_OS_SWIPE_PANEL_H
#define ELENA_OS_SWIPE_PANEL_H

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
 * @brief SwipePanel 拖拽的方向
 */
typedef enum{
    SWIPE_DIR_UP=0,      // 从下往上滑拉出 swipe_obj
    SWIPE_DIR_DOWN=1,    // 从上往下滑拉出 swipe_obj
    SWIPE_DIR_LEFT=2,    // 从右往左滑拉出 swipe_obj
    SWIPE_DIR_RIGHT=3    // 从左往右滑拉出 swipe_obj
} swipe_dir_t;
/**
 * @brief SwipePanel 结构体定义
 */
typedef struct {
    lv_obj_t* swipe_obj;
    lv_obj_t* touch_area;
    lv_obj_t* handle_bar;
    lv_coord_t start_x;
    lv_coord_t start_y;
    lv_coord_t touch_start_x;
    lv_coord_t touch_start_y;
    swipe_dir_t dir;
    bool swiping;
} swipe_panel_t;
/* Public function prototypes --------------------------------*/
/**
 * @brief 删除 SwipePanel
 * @param swipe_panel 拖拽控件
 */
void eos_swipe_panel_delete(swipe_panel_t *swipe_panel);

/**
 * @brief 创建 SwipePanel
 * @param parent 拖拽控件的父级对象
 * @return 指向创建成功的 SwipePanel
 * @note 不使用时需要使用 eos_swipe_panel_delete 删除此控件，否则可能内存泄漏
 */
swipe_panel_t *eos_swipe_panel_create(lv_obj_t *parent);

/**
 * @brief 设置拖拽方向
 * @param swipe_panel 拖拽控件
 * @param dir 拖拽方向，例如 SWIPE_DIR_DOWN 就是向下拖拽拉出 swipe_obj
 */
void eos_swipe_panel_set_dir(swipe_panel_t *swipe_panel, const swipe_dir_t dir);

/**
 * @brief 隐藏 HandleBar (小白条)
 * @param swipe_panel 拖拽控件
 */
void eos_swipe_panel_hide_handle_bar(swipe_panel_t* swipe_panel);

/**
 * @brief 显示 HandleBar (小白条)
 * @param swipe_panel 拖拽控件
 */
void eos_swipe_panel_show_handle_bar(swipe_panel_t* swipe_panel);

/**
 * @brief 外部触发拉回动画，自动拉回屏幕外面
 * @param swipe_panel 拖拽控件
 */
void eos_swipe_panel_pull_back(swipe_panel_t *swipe_panel);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_SWIPE_PANEL_H */
