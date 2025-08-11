/**
 * @file elena_os_drag_item.c
 * @brief 拖拽控件实现
 * @author Sab1e
 * @date 2025-08-10
 */

#include "elena_os_drag_item.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
// Macros and Definitions
#define DEBUG_DRAG_ITEM
#define GESTURE_AREA_HEIGHT 50

#define SCREEN_H lv_disp_get_ver_res(NULL)
#define SCREEN_W lv_disp_get_hor_res(NULL)
// Static Variables

// Function Implementations
static void drag_item_event_cb_pressed(lv_event_t *e)
{
    drag_item_t *drag_item = lv_event_get_user_data(e);
    lv_point_t p;
    lv_indev_get_point(lv_indev_get_act(), &p);
    
    drag_item->touch_start_x = p.x;
    drag_item->touch_start_y = p.y;
    
    drag_item->start_x = lv_obj_get_x(drag_item->drag_obj);
    drag_item->start_y = lv_obj_get_y(drag_item->drag_obj);
    
    drag_item->dragging = true;
}

static void drag_item_event_cb_pressing(lv_event_t *e)
{
    drag_item_t *drag_item = lv_event_get_user_data(e);
    if (!drag_item->dragging)
        return;

    lv_point_t p;
    lv_indev_get_point(lv_indev_get_act(), &p);
    
    switch (drag_item->dir) {
    case DRAG_DIR_UP:
    case DRAG_DIR_DOWN:
        {
            lv_coord_t diff_y = p.y - drag_item->touch_start_y;
            lv_coord_t new_y = drag_item->start_y + diff_y;
            
            // 根据方向调整限制范围
            if (drag_item->dir == DRAG_DIR_UP) {
                // UP方向：从下方滑出，限制在0到SCREEN_H之间
                if (new_y < 0) new_y = 0;
                if (new_y > SCREEN_H) new_y = SCREEN_H;
            } else { // DRAG_DIR_DOWN
                // DOWN方向：从上方滑出，限制在-SCREEN_H到0之间
                if (new_y > 0) new_y = 0;
                if (new_y < -SCREEN_H) new_y = -SCREEN_H;
            }
            
            lv_obj_set_y(drag_item->drag_obj, new_y);
            lv_obj_set_y(drag_item->gesture_area, new_y);
        }
        break;
        
    case DRAG_DIR_LEFT:
    case DRAG_DIR_RIGHT:
        {
            lv_coord_t diff_x = p.x - drag_item->touch_start_x;
            lv_coord_t new_x = drag_item->start_x + diff_x;
            
            // 根据方向调整限制范围
            if (drag_item->dir == DRAG_DIR_LEFT) {
                // LEFT方向：从右侧滑出，限制在0到SCREEN_W之间
                if (new_x < 0) new_x = 0;
                if (new_x > SCREEN_W) new_x = SCREEN_W;
            } else { // DRAG_DIR_RIGHT
                // RIGHT方向：从左侧滑出，限制在-SCREEN_W到0之间
                if (new_x > 0) new_x = 0;
                if (new_x < -SCREEN_W) new_x = -SCREEN_W;
            }
            
            lv_obj_set_x(drag_item->drag_obj, new_x);
            lv_obj_set_x(drag_item->gesture_area, new_x);
        }
        break;
    }
}

static void drag_item_anim_cb(lv_anim_t *a)
{
    drag_item_t *drag_item = (drag_item_t *)lv_anim_get_user_data(a);
    
    switch (drag_item->dir) {
    case DRAG_DIR_UP:
        {
            lv_coord_t y = lv_obj_get_y(drag_item->drag_obj);
            // 当面板移动到屏幕一半高度以下时，手势区域在底部
            if (y < SCREEN_H / 2) {
                lv_obj_set_y(drag_item->gesture_area, 0);
            } else {
                lv_obj_set_y(drag_item->gesture_area, SCREEN_H - GESTURE_AREA_HEIGHT);
            }
        }
        break;
    case DRAG_DIR_DOWN:
        {
            lv_coord_t y = lv_obj_get_y(drag_item->drag_obj);
            // 当面板移动到屏幕一半高度以上时，手势区域在顶部
            if (y > -SCREEN_H / 2) {
                lv_obj_set_y(drag_item->gesture_area, SCREEN_H - GESTURE_AREA_HEIGHT);
            } else {
                lv_obj_set_y(drag_item->gesture_area, 0);
            }
        }
        break;
    case DRAG_DIR_LEFT:
        {
            lv_coord_t x = lv_obj_get_x(drag_item->drag_obj);
            // 当面板移动到屏幕一半宽度以下时，手势区域在右侧
            if (x < SCREEN_W / 2) {
                lv_obj_set_x(drag_item->gesture_area, 0);
            } else {
                lv_obj_set_x(drag_item->gesture_area, SCREEN_W - GESTURE_AREA_HEIGHT);
            }
        }
        break;
    case DRAG_DIR_RIGHT:
        {
            lv_coord_t x = lv_obj_get_x(drag_item->drag_obj);
            // 当面板移动到屏幕一半宽度以上时，手势区域在左侧
            if (x > -SCREEN_W / 2) {
                lv_obj_set_x(drag_item->gesture_area, SCREEN_W - GESTURE_AREA_HEIGHT);
            } else {
                lv_obj_set_x(drag_item->gesture_area, 0);
            }
        }
        break;
    }
}

static void drag_item_event_cb_released(lv_event_t *e)
{
    drag_item_t *drag_item = lv_event_get_user_data(e);
    drag_item->dragging = false;
    
    lv_coord_t target_x = 0;
    lv_coord_t target_y = 0;
    lv_coord_t cur_x = lv_obj_get_x(drag_item->drag_obj);
    lv_coord_t cur_y = lv_obj_get_y(drag_item->drag_obj);
    
    switch (drag_item->dir) {
    case DRAG_DIR_UP:
        // UP方向：当面板位置小于屏幕一半高度时，完全显示（0），否则隐藏（SCREEN_H）
        target_y = (cur_y < SCREEN_H / 2) ? 0 : SCREEN_H;
        break;
    case DRAG_DIR_DOWN:
        // DOWN方向：当面板位置大于屏幕一半高度的负值时，完全显示（0），否则隐藏（-SCREEN_H）
        target_y = (cur_y > -SCREEN_H / 2) ? 0 : -SCREEN_H;
        break;
    case DRAG_DIR_LEFT:
        // LEFT方向：当面板位置小于屏幕一半宽度时，完全显示（0），否则隐藏（SCREEN_W）
        target_x = (cur_x < SCREEN_W / 2) ? 0 : SCREEN_W;
        break;
    case DRAG_DIR_RIGHT:
        // RIGHT方向：当面板位置大于屏幕一半宽度的负值时，完全显示（0），否则隐藏（-SCREEN_W）
        target_x = (cur_x > -SCREEN_W / 2) ? 0 : -SCREEN_W;
        break;
    }
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, drag_item->drag_obj);
    
    if (drag_item->dir == DRAG_DIR_UP || drag_item->dir == DRAG_DIR_DOWN) {
        lv_anim_set_values(&a, cur_y, target_y);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    } else {
        lv_anim_set_values(&a, cur_x, target_x);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    }
    
    lv_anim_set_time(&a, 250);
    lv_anim_set_user_data(&a, drag_item);
    lv_anim_set_ready_cb(&a, drag_item_anim_cb);
    lv_anim_start(&a);
}

void elena_os_drag_item_set_dir(drag_item_t *drag_item, const drag_dir_t dir)
{
    drag_item->dir = dir;
    
    // 根据方向调整手势区域尺寸
    switch (dir) {
    case DRAG_DIR_UP:
    case DRAG_DIR_DOWN:
        // 垂直方向：手势区域宽度为屏幕宽，高度为固定值
        lv_obj_set_size(drag_item->gesture_area, SCREEN_W, GESTURE_AREA_HEIGHT);
        break;
    case DRAG_DIR_LEFT:
    case DRAG_DIR_RIGHT:
        // 水平方向：手势区域高度为屏幕高，宽度为固定值
        lv_obj_set_size(drag_item->gesture_area, GESTURE_AREA_HEIGHT, SCREEN_H);
        break;
    }
    
    switch (dir) {
    case DRAG_DIR_UP:
        lv_obj_set_x(drag_item->drag_obj, 0);
        lv_obj_set_x(drag_item->gesture_area, 0);
        lv_obj_set_y(drag_item->drag_obj, SCREEN_H);
        lv_obj_set_y(drag_item->gesture_area, SCREEN_H - GESTURE_AREA_HEIGHT);
        break;
    case DRAG_DIR_DOWN:
        lv_obj_set_x(drag_item->drag_obj, 0);
        lv_obj_set_x(drag_item->gesture_area, 0);
        lv_obj_set_y(drag_item->drag_obj, -SCREEN_H);
        lv_obj_set_y(drag_item->gesture_area, 0);
        break;
    case DRAG_DIR_LEFT:
        lv_obj_set_y(drag_item->drag_obj, 0);
        lv_obj_set_y(drag_item->gesture_area, 0);
        lv_obj_set_x(drag_item->drag_obj, SCREEN_W);
        lv_obj_set_x(drag_item->gesture_area, SCREEN_W - GESTURE_AREA_HEIGHT);
        break;
    case DRAG_DIR_RIGHT:
        lv_obj_set_y(drag_item->drag_obj, 0);
        lv_obj_set_y(drag_item->gesture_area, 0);
        lv_obj_set_x(drag_item->drag_obj, -SCREEN_W);
        lv_obj_set_x(drag_item->gesture_area, 0);
        break;
    default:
        break;
    }
}
drag_item_t *elena_os_drag_item_create(lv_obj_t *parent)
{
    drag_item_t *drag_item = malloc(sizeof(drag_item_t));
    if (!drag_item)
        return NULL;
    drag_item->dragging = false;

    drag_item->drag_obj = lv_obj_create(parent);
    lv_obj_set_size(drag_item->drag_obj, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(drag_item->drag_obj, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(drag_item->drag_obj, 0, 0);
    lv_obj_set_style_shadow_width(drag_item->drag_obj, 0, 0);
    lv_obj_set_style_radius(drag_item->drag_obj, 0, 0);
    lv_obj_set_style_text_color(drag_item->drag_obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(drag_item->drag_obj, LV_OPA_80, 0);
    // 默认是下拉栏
    lv_obj_set_y(drag_item->drag_obj, -SCREEN_H);


    lv_obj_move_foreground(drag_item->drag_obj);

    drag_item->gesture_area = lv_obj_create(parent);
    lv_obj_set_size(drag_item->gesture_area, SCREEN_W, GESTURE_AREA_HEIGHT);
#ifdef DEBUG_DRAG_ITEM
    lv_obj_set_style_bg_color(drag_item->gesture_area, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(drag_item->gesture_area, LV_OPA_80, 0);
#else
    lv_obj_set_style_bg_opa(drag_item->gesture_area, LV_OPA_TRANSP, 0);
#endif
    lv_obj_set_style_border_opa(drag_item->gesture_area, LV_OPA_TRANSP, 0);
    lv_obj_set_y(drag_item->gesture_area, 0);
    lv_obj_set_style_radius(drag_item->gesture_area, 0, 0);
    lv_obj_move_foreground(drag_item->gesture_area);

    // 把drag_item指针作为用户数据绑定给gesture_area
    lv_obj_add_event_cb(drag_item->gesture_area, drag_item_event_cb_pressed, LV_EVENT_PRESSED, drag_item);
    lv_obj_add_event_cb(drag_item->gesture_area, drag_item_event_cb_pressing, LV_EVENT_PRESSING, drag_item);
    lv_obj_add_event_cb(drag_item->gesture_area, drag_item_event_cb_released, LV_EVENT_RELEASED, drag_item);
    
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(drag_item->drag_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(drag_item->gesture_area, LV_OBJ_FLAG_SCROLLABLE);

    return drag_item;
}
