/**
 * @file elena_os_base_item.c
 * @brief 基本控件
 * @author Sab1e
 * @date 2025-08-17
 */

#include "elena_os_base_item.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "elena_os_core.h"
#include "elena_os_lang.h"
#include "elena_os_log.h"
#include "elena_os_nav.h"
#include "elena_os_img.h"
#include "elena_os_event.h"
#include "script_engine_core.h"
#include "elena_os_theme.h"
#include "script_engine_nav.h"
#include "elena_os_port.h"
// Macros and Definitions
#define APP_HEADER_HEIGHT 120
#define APP_HEADER_CLOCK_UPDATE_PERIOD_MS 60000 // 一分钟

#define APP_HEADER_MARGIN_RIGHT 30

#define EOS_APP_CIRCLE_ICON_BUTTON_HEIGHT 100

#define APP_HEADER_TIME_STR_ARRAY_MAX 32
// Variables
extern script_pkg_t *script_pkg_ptr;
static eos_app_header_t *app_header = NULL;
// Function Implementations

/**
 * @brief 返回按钮的回调
 */
static void _back_btn_cb(lv_event_t *e)
{
    EOS_LOG_D("NAV back");
    if (script_pkg_ptr)
    {
        if (script_engine_nav_back_clean() != EOS_OK)
        {
            EOS_LOG_E("BACK ERR");
        }
    }
    else
    {
        if (eos_nav_back_clean() != EOS_OK)
        {
            EOS_LOG_E("BACK ERR");
        }
    }
}

lv_obj_t *eos_back_btn_create(lv_obj_t *parent, bool show_text)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_add_event_cb(btn, _back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *btn_label;
    if (show_text)
    {
        btn_label = eos_lang_label_create(btn, STR_ID_BASE_ITEM_BACK);
    }
    else
    {
        btn_label = lv_label_create(btn);
    }
    lv_label_set_text(btn_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(btn_label, EOS_THEME_PRIMARY_COLOR, 0);
    lv_obj_center(btn_label);

    return btn;
}

lv_obj_t *eos_list_add_button(lv_obj_t *list, const void *icon, const char *txt)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lv_list_button_class, list);
    lv_obj_class_init_obj(obj);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(obj,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    if (icon)
    {
        lv_obj_t *img = lv_image_create(obj);
        eos_img_set_src(img, icon);
    }

    if (txt)
    {
        lv_obj_t *label = lv_label_create(obj);
        lv_label_set_text(label, txt);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_flex_grow(label, 1);
    }

    return obj;
}

/**
 * @brief 更新LVGL字符串，显示当前时间
 */
static inline void _app_header_update_clock_label(lv_obj_t *label){
    eos_datetime_t dt = eos_time_get();
    char time_str[APP_HEADER_TIME_STR_ARRAY_MAX];
    snprintf(time_str,sizeof(time_str),"%02d:%02d",dt.hour,dt.minute);
    lv_label_set_text(label, time_str);
}

/**
 * @brief 时间刷新的回调，由 LVGL 的定时器触发
 */
static void clock_update_cb(lv_timer_t *timer)
{
    lv_obj_t *label = lv_timer_get_user_data(timer);
    EOS_CHECK_PTR_RETURN(label);
    // 更新显示文字
    _app_header_update_clock_label(label);
}

/**
 * @brief Header 的 screen 加载事件回调
 */
static void _screen_load_cb(lv_event_t *e)
{
    EOS_LOG_D("screen loaded");
    lv_obj_t *scr = lv_event_get_target(e);
    const char *title = (const char *)lv_event_get_user_data(e);
    if (title && *title)
    {
        EOS_LOG_D("set title: %s", title);
        eos_app_header_set_title(title);
        eos_app_header_show();
    }
    else
    {
        EOS_LOG_D("no title");
        eos_app_header_hide();
    }
}

/**
 * @brief Header 的 screen 删除事件回调
 */
static void _screen_delete_cb(lv_event_t *e)
{
    EOS_LOG_D("screen deleted");
    eos_app_header_set_title("");
    eos_app_header_hide();
}

void eos_app_header_set_title(const char *title)
{
    EOS_CHECK_PTR_RETURN(app_header);
    lv_label_set_text(app_header->title_label, title);
}

void eos_app_header_hide(void)
{
    EOS_CHECK_PTR_RETURN(app_header);
    lv_obj_add_flag(app_header->container, LV_OBJ_FLAG_HIDDEN);
}

void eos_app_header_show(void)
{
    EOS_CHECK_PTR_RETURN(app_header);
    lv_obj_remove_flag(app_header->container, LV_OBJ_FLAG_HIDDEN);
}

void eos_screen_bind_header(lv_obj_t *scr, const char *title)
{
    EOS_CHECK_PTR_RETURN(scr);

    // LVGL 会在 screen 加载时触发 LV_EVENT_SCREEN_LOADED
    // 并在 screen 被删除时触发 LV_EVENT_DELETE
    lv_obj_add_event_cb(scr, _screen_load_cb, LV_EVENT_SCREEN_LOADED, (void *)title);
    lv_obj_add_event_cb(scr, _screen_delete_cb, LV_EVENT_DELETE, NULL);
}

void eos_app_header_init(void)
{
    app_header = lv_malloc(sizeof(eos_app_header_t));
    EOS_CHECK_PTR_RETURN_FREE(app_header, app_header);
    memset(app_header, 0, sizeof(eos_app_header_t));

    // 半透明容器
    app_header->container = lv_image_create(lv_layer_top());
    lv_obj_set_size(app_header->container, lv_display_get_horizontal_resolution(NULL), APP_HEADER_HEIGHT);
    lv_obj_align(app_header->container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_move_foreground(app_header->container);
    eos_img_set_src(app_header->container, EOS_IMG_APP_HEADER_BG);

    lv_coord_t header_h = APP_HEADER_HEIGHT;
    lv_coord_t header_w = lv_obj_get_width(app_header->container);

    // 返回按钮
    app_header->back_btn = eos_back_btn_create(app_header->container, false);
    lv_obj_set_size(app_header->back_btn, 64, 64);
    lv_obj_set_style_radius(app_header->back_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(app_header->back_btn, EOS_THEME_SECONDARY_COLOR, 0);
    lv_obj_align(app_header->back_btn, LV_ALIGN_LEFT_MID, 20, 0);

    // 时间文字
    app_header->clock_label = lv_label_create(app_header->container);
    _app_header_update_clock_label(app_header->clock_label);
    lv_obj_align(app_header->clock_label, LV_ALIGN_RIGHT_MID, -APP_HEADER_MARGIN_RIGHT, -15);
    app_header->clock_timer = lv_timer_create(clock_update_cb, APP_HEADER_CLOCK_UPDATE_PERIOD_MS, app_header->clock_label);

    // 标题文字
    app_header->title_label = lv_label_create(app_header->container);
    if (script_pkg_ptr && script_pkg_ptr->name)
    {
        lv_label_set_text(app_header->title_label, script_pkg_ptr->name);
    }
    else
    {
        lv_label_set_text(app_header->title_label, "");
    }
    lv_label_set_long_mode(app_header->title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(app_header->title_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(app_header->title_label, EOS_THEME_PRIMARY_COLOR, 0);
    lv_obj_align(app_header->title_label, LV_ALIGN_RIGHT_MID, -APP_HEADER_MARGIN_RIGHT, 15);

    // 默认隐藏 app_header
    lv_obj_add_flag(app_header->container, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *eos_list_add_placeholder(lv_obj_t *list, uint32_t height)
{
    lv_obj_t *ph = lv_obj_create(list);
    lv_obj_remove_style_all(ph);
    lv_obj_set_size(ph, lv_pct(100), height);
    return ph;
}

lv_obj_t *eos_list_add_circle_icon_button(lv_obj_t *list, lv_color_t circle_color, const void *icon_src, const char *txt)
{
    // 创建按钮
    lv_obj_t *btn = lv_button_create(list);
    lv_obj_set_size(btn, lv_pct(100), EOS_APP_CIRCLE_ICON_BUTTON_HEIGHT);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(btn, LV_DIR_NONE); // 禁止滚动
    lv_obj_set_style_bg_color(btn, EOS_THEME_SECONDARY_COLOR, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 18, 0);
    lv_obj_set_style_margin_bottom(btn, 20, 0);
    lv_obj_set_style_margin_hor(btn, 20, 0);
    lv_obj_set_style_align(btn, LV_ALIGN_CENTER, 0);
    lv_obj_set_style_radius(btn, 25, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW); // 水平排布
    lv_obj_set_flex_align(btn,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    // 绘制圆形背景
    lv_obj_t *circle = lv_obj_create(btn);
    lv_obj_set_style_border_width(circle, 0, 0);
    lv_obj_remove_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(circle, 50, 50);
    lv_obj_set_style_pad_all(circle, 0, 0);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, circle_color, 0);
    // 绘制图像
    lv_obj_t *icon = lv_img_create(circle);
    lv_image_set_src(icon, icon_src);
    lv_obj_center(icon);
    // 文字
    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_margin_left(label, 14, 0);
    lv_label_set_text(label, txt);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_style_margin_right(label, 18, 0);
    return btn;
}

lv_obj_t *eos_list_add_switch(lv_obj_t *list, const char *txt)
{
    // 创建按钮
    lv_obj_t *container = lv_obj_create(list);
    lv_obj_set_size(container, lv_pct(100), EOS_APP_CIRCLE_ICON_BUTTON_HEIGHT);
    lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(container, LV_DIR_NONE); // 禁止滚动
    lv_obj_set_style_bg_color(container, EOS_THEME_SECONDARY_COLOR, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 18, 0);
    lv_obj_set_style_margin_bottom(container, 20, 0);
    lv_obj_set_style_margin_hor(container, 20, 0);
    lv_obj_set_style_align(container, LV_ALIGN_CENTER, 0);
    lv_obj_set_style_radius(container, 25, 0);
    lv_obj_set_style_shadow_width(container, 0, 0);
    lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW); // 水平排布
    lv_obj_set_flex_align(container,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // 文字
    lv_obj_t *label = lv_label_create(container);
    lv_obj_set_style_margin_left(label, 18, 0);
    lv_label_set_text(label, txt);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_flex_grow(label, 1);

    // 开关
    lv_obj_t *sw = lv_switch_create(container);
    lv_obj_set_style_margin_right(sw, 18, 0);
    return sw;
}
