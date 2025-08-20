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
#include "elena_os_log.h"
#include "elena_os_event.h"
// Macros and Definitions
// #define DEBUG_DRAG_ITEM
#define GESTURE_AREA_HEIGHT 50
#define DRAG_ITEM_DEFAULT_BG_COLOR 0x111111
#define SCREEN_H lv_disp_get_ver_res(NULL)
#define SCREEN_W lv_disp_get_hor_res(NULL)
#define TOUCH_BAR_MARGIN 20
// Variables
static drag_item_t *active_drag_item = NULL; // 当前正在拖拽的控件 同一时刻只能拖拽一个控件 否则会出现问题
extern uint32_t EOS_EVENT_DRAG_ITEM_TOUCH_UNLOCK;
extern uint32_t EOS_EVENT_DRAG_ITEM_TOUCH_LOCK;
// Function Implementations
static void _drag_item_event_cb_pressed(lv_event_t *e)
{
    drag_item_t *drag_item = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(drag_item);
    eos_event_broadcast(EOS_EVENT_DRAG_ITEM_TOUCH_LOCK, NULL);
    if (active_drag_item != NULL && active_drag_item != drag_item)
    {
        return;
    }
    active_drag_item = drag_item;
    lv_point_t p;
    lv_indev_get_point(lv_indev_get_act(), &p);

    drag_item->touch_start_x = p.x;
    drag_item->touch_start_y = p.y;

    drag_item->start_x = lv_obj_get_x(drag_item->drag_obj);
    drag_item->start_y = lv_obj_get_y(drag_item->drag_obj);

    drag_item->dragging = true;
    lv_obj_move_foreground(drag_item->drag_obj);
    lv_obj_move_foreground(drag_item->gesture_area);
}

static void _drag_item_event_cb_pressing(lv_event_t *e)
{
    drag_item_t *drag_item = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(drag_item);
    if (!drag_item->dragging)
        return;

    if (active_drag_item != NULL && active_drag_item != drag_item)
    {
        return;
    }

    lv_point_t p;
    lv_indev_get_point(lv_indev_get_act(), &p);

    // 预计算常用值
    lv_coord_t touch_diff = (drag_item->dir == DRAG_DIR_UP || drag_item->dir == DRAG_DIR_DOWN)
                                ? (p.y - drag_item->touch_start_y)
                                : (p.x - drag_item->touch_start_x);

    lv_coord_t new_pos = (drag_item->dir == DRAG_DIR_UP || drag_item->dir == DRAG_DIR_DOWN)
                             ? drag_item->start_y
                             : drag_item->start_x;
    new_pos += touch_diff;

    // 边界检查
    switch (drag_item->dir)
    {
    case DRAG_DIR_UP:
        new_pos = LV_CLAMP(0, new_pos, SCREEN_H);
        break;
    case DRAG_DIR_DOWN:
        new_pos = LV_CLAMP(-SCREEN_H, new_pos, 0);
        break;
    case DRAG_DIR_LEFT:
        new_pos = LV_CLAMP(0, new_pos, SCREEN_W);
        break;
    case DRAG_DIR_RIGHT:
        new_pos = LV_CLAMP(-SCREEN_W, new_pos, 0);
        break;
    }

    // 设置新位置
    if (drag_item->dir == DRAG_DIR_UP || drag_item->dir == DRAG_DIR_DOWN)
    {
        lv_obj_set_y(drag_item->drag_obj, new_pos);
        lv_obj_set_y(drag_item->gesture_area, new_pos);
    }
    else
    {
        lv_obj_set_x(drag_item->drag_obj, new_pos);
        lv_obj_set_x(drag_item->gesture_area, new_pos);
    }
}

static void _drag_item_timer_cb(lv_timer_t * timer)
{
    EOS_LOG_D("Timer Callback");
    eos_event_broadcast(EOS_EVENT_DRAG_ITEM_TOUCH_UNLOCK, NULL);
}

static void _drag_item_anim_completed_cb(lv_anim_t *a)
{
    drag_item_t *drag_item = (drag_item_t *)lv_anim_get_user_data(a);
    EOS_CHECK_PTR_RETURN(drag_item);

    switch (drag_item->dir)
    {
    case DRAG_DIR_UP:
    {
        lv_coord_t y = lv_obj_get_y(drag_item->drag_obj);
        // 当面板移动到屏幕一半高度以下时，手势区域在底部
        if (y < SCREEN_H / 2)
        {
            lv_obj_set_y(drag_item->gesture_area, 0);
        }
        else
        {
            lv_obj_set_y(drag_item->gesture_area, SCREEN_H - GESTURE_AREA_HEIGHT);
        }
    }
    break;
    case DRAG_DIR_DOWN:
    {
        lv_coord_t y = lv_obj_get_y(drag_item->drag_obj);
        // 当面板移动到屏幕一半高度以上时，手势区域在顶部
        if (y > -SCREEN_H / 2)
        {
            lv_obj_set_y(drag_item->gesture_area, SCREEN_H - GESTURE_AREA_HEIGHT);
        }
        else
        {
            lv_obj_set_y(drag_item->gesture_area, 0);
        }
    }
    break;
    case DRAG_DIR_LEFT:
    {
        lv_coord_t x = lv_obj_get_x(drag_item->drag_obj);
        // 当面板移动到屏幕一半宽度以下时，手势区域在右侧
        if (x < SCREEN_W / 2)
        {
            lv_obj_set_x(drag_item->gesture_area, 0);
        }
        else
        {
            lv_obj_set_x(drag_item->gesture_area, SCREEN_W - GESTURE_AREA_HEIGHT);
        }
    }
    break;
    case DRAG_DIR_RIGHT:
    {
        lv_coord_t x = lv_obj_get_x(drag_item->drag_obj);
        // 当面板移动到屏幕一半宽度以上时，手势区域在左侧
        if (x > -SCREEN_W / 2)
        {
            lv_obj_set_x(drag_item->gesture_area, SCREEN_W - GESTURE_AREA_HEIGHT);
        }
        else
        {
            lv_obj_set_x(drag_item->gesture_area, 0);
        }
    }
    break;
    }
    if (active_drag_item == drag_item)
    {
        active_drag_item = NULL; // 释放活动实例
    }
    // 避免操作过快
    lv_timer_t * t = lv_timer_create(_drag_item_timer_cb, 50, NULL);
    lv_timer_set_repeat_count(t, 1);  // 只触发一次
    
}

static void _drag_item_event_cb_released(lv_event_t *e)
{
    drag_item_t *drag_item = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(drag_item);
    drag_item->dragging = false;

    if (active_drag_item != NULL && active_drag_item != drag_item)
    {
        return;
    }

    lv_coord_t target_x = 0;
    lv_coord_t target_y = 0;
    lv_coord_t cur_x = lv_obj_get_x(drag_item->drag_obj);
    lv_coord_t cur_y = lv_obj_get_y(drag_item->drag_obj);

    switch (drag_item->dir)
    {
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

    if (drag_item->dir == DRAG_DIR_UP || drag_item->dir == DRAG_DIR_DOWN)
    {
        lv_anim_set_values(&a, cur_y, target_y);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    }
    else
    {
        lv_anim_set_values(&a, cur_x, target_x);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    }

    lv_anim_set_time(&a, 120);
    lv_anim_set_user_data(&a, drag_item);
    lv_anim_set_ready_cb(&a, _drag_item_anim_completed_cb);
    lv_anim_start(&a);
}

static void _update_touch_bar_position(drag_item_t *drag_item, drag_dir_t dir)
{
    EOS_CHECK_PTR_RETURN(drag_item && drag_item->touch_bar);
    switch (dir)
    {
    case DRAG_DIR_DOWN:
        lv_obj_align(drag_item->touch_bar, LV_ALIGN_BOTTOM_MID, 0, -TOUCH_BAR_MARGIN); // 底部上方10px
        break;
    case DRAG_DIR_UP:
        lv_obj_align(drag_item->touch_bar, LV_ALIGN_TOP_MID, 0, TOUCH_BAR_MARGIN); // 顶部下方10px
        break;
    case DRAG_DIR_LEFT:
        lv_obj_align(drag_item->touch_bar, LV_ALIGN_LEFT_MID, TOUCH_BAR_MARGIN, 0); // 右侧10px
        break;
    case DRAG_DIR_RIGHT:
        lv_obj_align(drag_item->touch_bar, LV_ALIGN_RIGHT_MID, -TOUCH_BAR_MARGIN, 0); // 左侧10px
        break;
    }
}

static void _drag_item_touch_lock(lv_event_t *e)
{
    drag_item_t *drag_item = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(drag_item);

    if (active_drag_item != NULL && active_drag_item == drag_item)
    {
        // 如果是自己就直接返回
        return;
    }
    lv_obj_add_flag(drag_item->gesture_area, LV_OBJ_FLAG_HIDDEN);
}

static void _drag_item_touch_unlock(lv_event_t *e)
{
    drag_item_t *drag_item = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(drag_item);

    lv_obj_remove_flag(drag_item->gesture_area, LV_OBJ_FLAG_HIDDEN);
}


void eos_drag_item_pull_back(drag_item_t *drag_item)
{
    if (!drag_item)
        return;

    // 检查当前是否在屏幕中
    lv_coord_t cur_x = lv_obj_get_x(drag_item->drag_obj);
    lv_coord_t cur_y = lv_obj_get_y(drag_item->drag_obj);

    // 禁用触摸交互
    lv_obj_remove_flag(drag_item->gesture_area, LV_OBJ_FLAG_CLICKABLE);

    // 设置动画目标位置
    lv_coord_t target_x = 0;
    lv_coord_t target_y = 0;

    switch (drag_item->dir)
    {
    case DRAG_DIR_UP:
        target_y = SCREEN_H; // 向上拉回时完全隐藏
        break;
    case DRAG_DIR_DOWN:
        target_y = -SCREEN_H; // 向下拉回时完全隐藏
        break;
    case DRAG_DIR_LEFT:
        target_x = SCREEN_W; // 向左拉回时完全隐藏
        break;
    case DRAG_DIR_RIGHT:
        target_x = -SCREEN_W; // 向右拉回时完全隐藏
        break;
    }

    // 创建动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, drag_item->drag_obj);

    if (drag_item->dir == DRAG_DIR_UP || drag_item->dir == DRAG_DIR_DOWN)
    {
        lv_anim_set_values(&a, cur_y, target_y);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    }
    else
    {
        lv_anim_set_values(&a, cur_x, target_x);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    }

    lv_anim_set_time(&a, 250);
    lv_anim_set_user_data(&a, drag_item);
    lv_anim_set_ready_cb(&a, _drag_item_anim_completed_cb);
    lv_anim_start(&a);

    // 同时移动gesture_area
    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_var(&b, drag_item->gesture_area);

    if (drag_item->dir == DRAG_DIR_UP || drag_item->dir == DRAG_DIR_DOWN)
    {
        lv_anim_set_values(&b, cur_y, target_y);
        lv_anim_set_exec_cb(&b, (lv_anim_exec_xcb_t)lv_obj_set_y);
    }
    else
    {
        lv_anim_set_values(&b, cur_x, target_x);
        lv_anim_set_exec_cb(&b, (lv_anim_exec_xcb_t)lv_obj_set_x);
    }

    lv_anim_set_time(&b, 250);
    lv_anim_start(&b);
    lv_obj_add_flag(drag_item->gesture_area, LV_OBJ_FLAG_CLICKABLE);
}

void eos_drag_item_hide_touch_bar(drag_item_t *drag_item)
{
    if (!drag_item || !drag_item->touch_bar)
        return;

    lv_obj_add_flag(drag_item->touch_bar, LV_OBJ_FLAG_HIDDEN);
}

void eos_drag_item_show_touch_bar(drag_item_t *drag_item)
{
    if (!drag_item || !drag_item->touch_bar)
        return;

    lv_obj_remove_flag(drag_item->touch_bar, LV_OBJ_FLAG_HIDDEN);
}

void eos_drag_item_set_dir(drag_item_t *drag_item, const drag_dir_t dir)
{
    if (!drag_item)
        return;

    drag_item->dir = dir;

    // 根据方向调整手势区域尺寸
    switch (dir)
    {
    case DRAG_DIR_UP:
    case DRAG_DIR_DOWN:
        lv_obj_set_size(drag_item->gesture_area, SCREEN_W, GESTURE_AREA_HEIGHT);
        break;
    case DRAG_DIR_LEFT:
    case DRAG_DIR_RIGHT:
        lv_obj_set_size(drag_item->gesture_area, GESTURE_AREA_HEIGHT, SCREEN_H);
        break;
    }

    // 更新主对象位置
    switch (dir)
    {
    case DRAG_DIR_UP:
        lv_obj_set_pos(drag_item->drag_obj, 0, SCREEN_H);
        lv_obj_set_pos(drag_item->gesture_area, 0, SCREEN_H - GESTURE_AREA_HEIGHT);
        break;
    case DRAG_DIR_DOWN:
        lv_obj_set_pos(drag_item->drag_obj, 0, -SCREEN_H);
        lv_obj_set_pos(drag_item->gesture_area, 0, 0);
        break;
    case DRAG_DIR_LEFT:
        lv_obj_set_pos(drag_item->drag_obj, SCREEN_W, 0);
        lv_obj_set_pos(drag_item->gesture_area, SCREEN_W - GESTURE_AREA_HEIGHT, 0);
        break;
    case DRAG_DIR_RIGHT:
        lv_obj_set_pos(drag_item->drag_obj, -SCREEN_W, 0);
        lv_obj_set_pos(drag_item->gesture_area, 0, 0);
        break;
    }

    // 更新touch_bar位置
    _update_touch_bar_position(drag_item, dir);
}

void eos_drag_item_del(drag_item_t *drag_item)
{
    if (!drag_item)
        return;
    lv_obj_del(drag_item->drag_obj);
    lv_obj_del(drag_item->gesture_area);
    lv_mem_free(drag_item);
}

drag_item_t *eos_drag_item_create(lv_obj_t *parent)
{
    drag_item_t *drag_item = lv_mem_alloc(sizeof(drag_item_t));
    if (!drag_item || !parent)
        return NULL;
    drag_item->dragging = false;

    // 初始化 drag_obj
    drag_item->drag_obj = lv_obj_create(parent);
    lv_obj_set_size(drag_item->drag_obj, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(drag_item->drag_obj, lv_color_hex(DRAG_ITEM_DEFAULT_BG_COLOR), 0);
    // lv_obj_set_style_bg_opa(drag_item->drag_obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(drag_item->drag_obj, 0, 0);
    lv_obj_set_style_shadow_width(drag_item->drag_obj, 0, 0);
    lv_obj_set_style_radius(drag_item->drag_obj, 0, 0);
    lv_obj_set_style_pad_all(drag_item->drag_obj, 0, 0);
    // 默认是下拉栏
    lv_obj_set_y(drag_item->drag_obj, -SCREEN_H);
    lv_obj_move_foreground(drag_item->drag_obj);

    drag_item->touch_bar = lv_obj_create(drag_item->drag_obj);
    lv_obj_set_size(drag_item->touch_bar, 80, 10);
    lv_obj_set_style_radius(drag_item->touch_bar, 5, 0);
    lv_obj_set_style_bg_color(drag_item->touch_bar, lv_color_hex(0xA6A6A6), 0);
    lv_obj_set_style_border_width(drag_item->touch_bar, 0, 0);

    // 默认设置为下拉模式（位于底部）
    _update_touch_bar_position(drag_item, DRAG_DIR_DOWN);

    // 初始化 gesture_area
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
    lv_obj_add_event_cb(drag_item->gesture_area, _drag_item_event_cb_pressed, LV_EVENT_PRESSED, drag_item);
    lv_obj_add_event_cb(drag_item->gesture_area, _drag_item_event_cb_pressing, LV_EVENT_PRESSING, drag_item);
    lv_obj_add_event_cb(drag_item->gesture_area, _drag_item_event_cb_released, LV_EVENT_RELEASED, drag_item);
    eos_event_add_cb(drag_item->gesture_area, _drag_item_touch_lock, EOS_EVENT_DRAG_ITEM_TOUCH_LOCK, drag_item);
    eos_event_add_cb(drag_item->gesture_area, _drag_item_touch_unlock, EOS_EVENT_DRAG_ITEM_TOUCH_UNLOCK, drag_item);

    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(drag_item->drag_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(drag_item->gesture_area, LV_OBJ_FLAG_SCROLLABLE);

    return drag_item;
}
