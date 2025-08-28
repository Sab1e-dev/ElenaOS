/**
 * @file elena_os_swipe_panel.c
 * @brief 滑动面板实现
 * @author Sab1e
 * @date 2025-08-10
 */

#include "elena_os_swipe_panel.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "elena_os_log.h"
#include "elena_os_event.h"
// Macros and Definitions
// #define DEBUG_SWIPE_PANEL
#define GESTURE_AREA_HEIGHT 50
#define SWIPE_PANEL_DEFAULT_BG_COLOR 0x111111
#define TOUCH_BAR_MARGIN 20
// Variables
static swipe_panel_t *active_swipe_panel = NULL; // 当前正在拖拽的控件 同一时刻只能拖拽一个控件 否则会出现问题
// Function Implementations
static void _swipe_panel_event_cb_pressed(lv_event_t *e)
{
    swipe_panel_t *swipe_panel = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(swipe_panel);
    eos_event_broadcast(eos_event_get_code(EOS_EVENT_SWIPE_PANEL_TOUCH_LOCK), NULL);
    if (active_swipe_panel != NULL && active_swipe_panel != swipe_panel)
    {
        return;
    }
    active_swipe_panel = swipe_panel;
    lv_point_t p;
    lv_indev_get_point(lv_indev_get_act(), &p);

    swipe_panel->touch_start_x = p.x;
    swipe_panel->touch_start_y = p.y;

    swipe_panel->start_x = lv_obj_get_x(swipe_panel->swipe_obj);
    swipe_panel->start_y = lv_obj_get_y(swipe_panel->swipe_obj);

    swipe_panel->swiping = true;
    lv_obj_move_foreground(swipe_panel->swipe_obj);
    lv_obj_move_foreground(swipe_panel->touch_area);
}

static void _swipe_panel_event_cb_pressing(lv_event_t *e)
{
    swipe_panel_t *swipe_panel = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(swipe_panel);
    if (!swipe_panel->swiping)
        return;

    if (active_swipe_panel != NULL && active_swipe_panel != swipe_panel)
    {
        return;
    }

    lv_point_t p;
    lv_indev_get_point(lv_indev_get_act(), &p);

    // 预计算常用值
    lv_coord_t touch_diff = (swipe_panel->dir == SWIPE_DIR_UP || swipe_panel->dir == SWIPE_DIR_DOWN)
                                ? (p.y - swipe_panel->touch_start_y)
                                : (p.x - swipe_panel->touch_start_x);

    lv_coord_t new_pos = (swipe_panel->dir == SWIPE_DIR_UP || swipe_panel->dir == SWIPE_DIR_DOWN)
                             ? swipe_panel->start_y
                             : swipe_panel->start_x;
    new_pos += touch_diff;

    // 边界检查
    switch (swipe_panel->dir)
    {
    case SWIPE_DIR_UP:
        new_pos = LV_CLAMP(0, new_pos, lv_display_get_vertical_resolution(NULL));
        break;
    case SWIPE_DIR_DOWN:
        new_pos = LV_CLAMP(-lv_display_get_vertical_resolution(NULL), new_pos, 0);
        break;
    case SWIPE_DIR_LEFT:
        new_pos = LV_CLAMP(0, new_pos, lv_display_get_horizontal_resolution(NULL));
        break;
    case SWIPE_DIR_RIGHT:
        new_pos = LV_CLAMP(-lv_display_get_horizontal_resolution(NULL), new_pos, 0);
        break;
    }

    // 设置新位置
    if (swipe_panel->dir == SWIPE_DIR_UP || swipe_panel->dir == SWIPE_DIR_DOWN)
    {
        lv_obj_set_y(swipe_panel->swipe_obj, new_pos);
        lv_obj_set_y(swipe_panel->touch_area, new_pos);
    }
    else
    {
        lv_obj_set_x(swipe_panel->swipe_obj, new_pos);
        lv_obj_set_x(swipe_panel->touch_area, new_pos);
    }
}

static void _swipe_panel_timer_cb(lv_timer_t * timer)
{
    EOS_LOG_D("Timer Callback");
    eos_event_broadcast(eos_event_get_code(EOS_EVENT_SWIPE_PANEL_TOUCH_UNLOCK), NULL);
}

static void _swipe_panel_anim_completed_cb(lv_anim_t *a)
{
    swipe_panel_t *swipe_panel = (swipe_panel_t *)lv_anim_get_user_data(a);
    EOS_CHECK_PTR_RETURN(swipe_panel);

    switch (swipe_panel->dir)
    {
    case SWIPE_DIR_UP:
    {
        lv_coord_t y = lv_obj_get_y(swipe_panel->swipe_obj);
        // 当面板移动到屏幕一半高度以下时，手势区域在底部
        if (y < lv_display_get_vertical_resolution(NULL) / 2)
        {
            lv_obj_set_y(swipe_panel->touch_area, 0);
        }
        else
        {
            lv_obj_set_y(swipe_panel->touch_area, lv_display_get_vertical_resolution(NULL) - GESTURE_AREA_HEIGHT);
        }
    }
    break;
    case SWIPE_DIR_DOWN:
    {
        lv_coord_t y = lv_obj_get_y(swipe_panel->swipe_obj);
        // 当面板移动到屏幕一半高度以上时，手势区域在顶部
        if (y > -lv_display_get_vertical_resolution(NULL) / 2)
        {
            lv_obj_set_y(swipe_panel->touch_area, lv_display_get_vertical_resolution(NULL) - GESTURE_AREA_HEIGHT);
        }
        else
        {
            lv_obj_set_y(swipe_panel->touch_area, 0);
        }
    }
    break;
    case SWIPE_DIR_LEFT:
    {
        lv_coord_t x = lv_obj_get_x(swipe_panel->swipe_obj);
        // 当面板移动到屏幕一半宽度以下时，手势区域在右侧
        if (x < lv_display_get_horizontal_resolution(NULL) / 2)
        {
            lv_obj_set_x(swipe_panel->touch_area, 0);
        }
        else
        {
            lv_obj_set_x(swipe_panel->touch_area, lv_display_get_horizontal_resolution(NULL) - GESTURE_AREA_HEIGHT);
        }
    }
    break;
    case SWIPE_DIR_RIGHT:
    {
        lv_coord_t x = lv_obj_get_x(swipe_panel->swipe_obj);
        // 当面板移动到屏幕一半宽度以上时，手势区域在左侧
        if (x > -lv_display_get_horizontal_resolution(NULL) / 2)
        {
            lv_obj_set_x(swipe_panel->touch_area, lv_display_get_horizontal_resolution(NULL) - GESTURE_AREA_HEIGHT);
        }
        else
        {
            lv_obj_set_x(swipe_panel->touch_area, 0);
        }
    }
    break;
    }
    if (active_swipe_panel == swipe_panel)
    {
        active_swipe_panel = NULL; // 释放活动实例
    }
    // 避免操作过快
    lv_timer_t * t = lv_timer_create(_swipe_panel_timer_cb, 50, NULL);
    lv_timer_set_repeat_count(t, 1);  // 只触发一次
    
}

static void _swipe_panel_event_cb_released(lv_event_t *e)
{
    swipe_panel_t *swipe_panel = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(swipe_panel);
    swipe_panel->swiping = false;

    if (active_swipe_panel != NULL && active_swipe_panel != swipe_panel)
    {
        return;
    }

    lv_coord_t target_x = 0;
    lv_coord_t target_y = 0;
    lv_coord_t cur_x = lv_obj_get_x(swipe_panel->swipe_obj);
    lv_coord_t cur_y = lv_obj_get_y(swipe_panel->swipe_obj);

    switch (swipe_panel->dir)
    {
    case SWIPE_DIR_UP:
        // UP方向：当面板位置小于屏幕一半高度时，完全显示（0），否则隐藏（lv_display_get_vertical_resolution(NULL)）
        target_y = (cur_y < lv_display_get_vertical_resolution(NULL) / 2) ? 0 : lv_display_get_vertical_resolution(NULL);
        break;
    case SWIPE_DIR_DOWN:
        // DOWN方向：当面板位置大于屏幕一半高度的负值时，完全显示（0），否则隐藏（-lv_display_get_vertical_resolution(NULL)）
        target_y = (cur_y > -lv_display_get_vertical_resolution(NULL) / 2) ? 0 : -lv_display_get_vertical_resolution(NULL);
        break;
    case SWIPE_DIR_LEFT:
        // LEFT方向：当面板位置小于屏幕一半宽度时，完全显示（0），否则隐藏（lv_display_get_horizontal_resolution(NULL)）
        target_x = (cur_x < lv_display_get_horizontal_resolution(NULL) / 2) ? 0 : lv_display_get_horizontal_resolution(NULL);
        break;
    case SWIPE_DIR_RIGHT:
        // RIGHT方向：当面板位置大于屏幕一半宽度的负值时，完全显示（0），否则隐藏（-lv_display_get_horizontal_resolution(NULL)）
        target_x = (cur_x > -lv_display_get_horizontal_resolution(NULL) / 2) ? 0 : -lv_display_get_horizontal_resolution(NULL);
        break;
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, swipe_panel->swipe_obj);

    if (swipe_panel->dir == SWIPE_DIR_UP || swipe_panel->dir == SWIPE_DIR_DOWN)
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
    lv_anim_set_user_data(&a, swipe_panel);
    lv_anim_set_ready_cb(&a, _swipe_panel_anim_completed_cb);
    lv_anim_start(&a);
}

static void _update_handle_bar_position(swipe_panel_t *swipe_panel, swipe_dir_t dir)
{
    EOS_CHECK_PTR_RETURN(swipe_panel && swipe_panel->handle_bar);
    switch (dir)
    {
    case SWIPE_DIR_DOWN:
        lv_obj_align(swipe_panel->handle_bar, LV_ALIGN_BOTTOM_MID, 0, -TOUCH_BAR_MARGIN); // 底部上方10px
        break;
    case SWIPE_DIR_UP:
        lv_obj_align(swipe_panel->handle_bar, LV_ALIGN_TOP_MID, 0, TOUCH_BAR_MARGIN); // 顶部下方10px
        break;
    case SWIPE_DIR_LEFT:
        lv_obj_align(swipe_panel->handle_bar, LV_ALIGN_LEFT_MID, TOUCH_BAR_MARGIN, 0); // 右侧10px
        break;
    case SWIPE_DIR_RIGHT:
        lv_obj_align(swipe_panel->handle_bar, LV_ALIGN_RIGHT_MID, -TOUCH_BAR_MARGIN, 0); // 左侧10px
        break;
    }
}

static void _swipe_panel_touch_lock(lv_event_t *e)
{
    swipe_panel_t *swipe_panel = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(swipe_panel);

    if (active_swipe_panel != NULL && active_swipe_panel == swipe_panel)
    {
        // 如果是自己就直接返回
        return;
    }
    lv_obj_add_flag(swipe_panel->touch_area, LV_OBJ_FLAG_HIDDEN);
}

static void _swipe_panel_touch_unlock(lv_event_t *e)
{
    swipe_panel_t *swipe_panel = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(swipe_panel);

    lv_obj_remove_flag(swipe_panel->touch_area, LV_OBJ_FLAG_HIDDEN);
}


void eos_swipe_panel_pull_back(swipe_panel_t *swipe_panel)
{
    if (!swipe_panel)
        return;

    // 检查当前是否在屏幕中
    lv_coord_t cur_x = lv_obj_get_x(swipe_panel->swipe_obj);
    lv_coord_t cur_y = lv_obj_get_y(swipe_panel->swipe_obj);

    // 禁用触摸交互
    lv_obj_remove_flag(swipe_panel->touch_area, LV_OBJ_FLAG_CLICKABLE);

    // 设置动画目标位置
    lv_coord_t target_x = 0;
    lv_coord_t target_y = 0;

    switch (swipe_panel->dir)
    {
    case SWIPE_DIR_UP:
        target_y = lv_display_get_vertical_resolution(NULL); // 向上拉回时完全隐藏
        break;
    case SWIPE_DIR_DOWN:
        target_y = -lv_display_get_vertical_resolution(NULL); // 向下拉回时完全隐藏
        break;
    case SWIPE_DIR_LEFT:
        target_x = lv_display_get_horizontal_resolution(NULL); // 向左拉回时完全隐藏
        break;
    case SWIPE_DIR_RIGHT:
        target_x = -lv_display_get_horizontal_resolution(NULL); // 向右拉回时完全隐藏
        break;
    }

    // 创建动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, swipe_panel->swipe_obj);

    if (swipe_panel->dir == SWIPE_DIR_UP || swipe_panel->dir == SWIPE_DIR_DOWN)
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
    lv_anim_set_user_data(&a, swipe_panel);
    lv_anim_set_ready_cb(&a, _swipe_panel_anim_completed_cb);
    lv_anim_start(&a);

    // 同时移动touch_area
    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_var(&b, swipe_panel->touch_area);

    if (swipe_panel->dir == SWIPE_DIR_UP || swipe_panel->dir == SWIPE_DIR_DOWN)
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
    lv_obj_add_flag(swipe_panel->touch_area, LV_OBJ_FLAG_CLICKABLE);
}

void eos_swipe_panel_hide_handle_bar(swipe_panel_t *swipe_panel)
{
    if (!swipe_panel || !swipe_panel->handle_bar)
        return;

    lv_obj_add_flag(swipe_panel->handle_bar, LV_OBJ_FLAG_HIDDEN);
}

void eos_swipe_panel_show_handle_bar(swipe_panel_t *swipe_panel)
{
    if (!swipe_panel || !swipe_panel->handle_bar)
        return;

    lv_obj_remove_flag(swipe_panel->handle_bar, LV_OBJ_FLAG_HIDDEN);
}

void eos_swipe_panel_set_dir(swipe_panel_t *swipe_panel, const swipe_dir_t dir)
{
    if (!swipe_panel)
        return;

    swipe_panel->dir = dir;

    // 根据方向调整手势区域尺寸
    switch (dir)
    {
    case SWIPE_DIR_UP:
    case SWIPE_DIR_DOWN:
        lv_obj_set_size(swipe_panel->touch_area, lv_display_get_horizontal_resolution(NULL), GESTURE_AREA_HEIGHT);
        break;
    case SWIPE_DIR_LEFT:
    case SWIPE_DIR_RIGHT:
        lv_obj_set_size(swipe_panel->touch_area, GESTURE_AREA_HEIGHT, lv_display_get_vertical_resolution(NULL));
        break;
    }

    // 更新主对象位置
    switch (dir)
    {
    case SWIPE_DIR_UP:
        lv_obj_set_pos(swipe_panel->swipe_obj, 0, lv_display_get_vertical_resolution(NULL));
        lv_obj_set_pos(swipe_panel->touch_area, 0, lv_display_get_vertical_resolution(NULL) - GESTURE_AREA_HEIGHT);
        break;
    case SWIPE_DIR_DOWN:
        lv_obj_set_pos(swipe_panel->swipe_obj, 0, -lv_display_get_vertical_resolution(NULL));
        lv_obj_set_pos(swipe_panel->touch_area, 0, 0);
        break;
    case SWIPE_DIR_LEFT:
        lv_obj_set_pos(swipe_panel->swipe_obj, lv_display_get_horizontal_resolution(NULL), 0);
        lv_obj_set_pos(swipe_panel->touch_area, lv_display_get_horizontal_resolution(NULL) - GESTURE_AREA_HEIGHT, 0);
        break;
    case SWIPE_DIR_RIGHT:
        lv_obj_set_pos(swipe_panel->swipe_obj, -lv_display_get_horizontal_resolution(NULL), 0);
        lv_obj_set_pos(swipe_panel->touch_area, 0, 0);
        break;
    }

    // 更新handle_bar位置
    _update_handle_bar_position(swipe_panel, dir);
}

void eos_swipe_panel_delete(swipe_panel_t *swipe_panel)
{
    if (!swipe_panel)
        return;
    lv_obj_del(swipe_panel->swipe_obj);
    lv_obj_del(swipe_panel->touch_area);
    lv_free(swipe_panel);
}

swipe_panel_t *eos_swipe_panel_create(lv_obj_t *parent)
{
    swipe_panel_t *swipe_panel = lv_malloc(sizeof(swipe_panel_t));
    if (!swipe_panel || !parent)
        return NULL;
    swipe_panel->swiping = false;

    // 初始化 swipe_obj
    swipe_panel->swipe_obj = lv_obj_create(parent);
    lv_obj_set_size(swipe_panel->swipe_obj, lv_display_get_horizontal_resolution(NULL), lv_display_get_vertical_resolution(NULL));
    lv_obj_set_style_bg_color(swipe_panel->swipe_obj, lv_color_hex(SWIPE_PANEL_DEFAULT_BG_COLOR), 0);
    // lv_obj_set_style_bg_opa(swipe_panel->swipe_obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(swipe_panel->swipe_obj, 0, 0);
    lv_obj_set_style_shadow_width(swipe_panel->swipe_obj, 0, 0);
    lv_obj_set_style_radius(swipe_panel->swipe_obj, 0, 0);
    lv_obj_set_style_pad_all(swipe_panel->swipe_obj, 0, 0);
    // 默认是下拉栏
    lv_obj_set_y(swipe_panel->swipe_obj, -lv_display_get_vertical_resolution(NULL));
    lv_obj_move_foreground(swipe_panel->swipe_obj);

    swipe_panel->handle_bar = lv_obj_create(swipe_panel->swipe_obj);
    lv_obj_set_size(swipe_panel->handle_bar, 80, 10);
    lv_obj_set_style_radius(swipe_panel->handle_bar, 5, 0);
    lv_obj_set_style_bg_color(swipe_panel->handle_bar, lv_color_hex(0xA6A6A6), 0);
    lv_obj_set_style_border_width(swipe_panel->handle_bar, 0, 0);

    // 默认设置为下拉模式（位于底部）
    _update_handle_bar_position(swipe_panel, SWIPE_DIR_DOWN);

    // 初始化 touch_area
    swipe_panel->touch_area = lv_obj_create(parent);
    lv_obj_set_size(swipe_panel->touch_area, lv_display_get_horizontal_resolution(NULL), GESTURE_AREA_HEIGHT);
#ifdef DEBUG_SWIPE_PANEL
    lv_obj_set_style_bg_color(swipe_panel->touch_area, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(swipe_panel->touch_area, LV_OPA_80, 0);
#else
    lv_obj_set_style_bg_opa(swipe_panel->touch_area, LV_OPA_TRANSP, 0);
#endif
    lv_obj_set_style_border_opa(swipe_panel->touch_area, LV_OPA_TRANSP, 0);
    lv_obj_set_y(swipe_panel->touch_area, 0);
    lv_obj_set_style_radius(swipe_panel->touch_area, 0, 0);
    lv_obj_move_foreground(swipe_panel->touch_area);

    // 把swipe_panel指针作为用户数据绑定给touch_area
    lv_obj_add_event_cb(swipe_panel->touch_area, _swipe_panel_event_cb_pressed, LV_EVENT_PRESSED, swipe_panel);
    lv_obj_add_event_cb(swipe_panel->touch_area, _swipe_panel_event_cb_pressing, LV_EVENT_PRESSING, swipe_panel);
    lv_obj_add_event_cb(swipe_panel->touch_area, _swipe_panel_event_cb_released, LV_EVENT_RELEASED, swipe_panel);
    eos_event_add_cb(swipe_panel->touch_area, _swipe_panel_touch_lock, eos_event_get_code(EOS_EVENT_SWIPE_PANEL_TOUCH_LOCK), swipe_panel);
    eos_event_add_cb(swipe_panel->touch_area, _swipe_panel_touch_unlock, eos_event_get_code(EOS_EVENT_SWIPE_PANEL_TOUCH_UNLOCK), swipe_panel);

    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(swipe_panel->swipe_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(swipe_panel->touch_area, LV_OBJ_FLAG_SCROLLABLE);

    return swipe_panel;
}
