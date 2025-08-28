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
#include "elena_os_clock.h"
#include "script_engine_core.h"
#include "elena_os_theme.h"
#include "script_engine_nav.h"
// Macros and Definitions
#define APP_HEADER_HEIGHT 120
#define APP_HEADER_CLOCK_UPDATE_PERIOD_MS 60000 // 一分钟
// Variables
extern script_pkg_t *script_pkg_ptr;
// Function Implementations

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

static void _app_header_delete_cb(lv_event_t *e)
{
    eos_app_header_t *header = (eos_app_header_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(header);
    if (header->clock_timer)
    {
        lv_timer_del(header->clock_timer);
        header->clock_timer = NULL;
    }
    lv_free(header);
}
// 每分钟刷新一次的回调
static void clock_update_cb(lv_timer_t *timer)
{
    lv_obj_t *label = lv_timer_get_user_data(timer);
    EOS_CHECK_PTR_RETURN(label);
    // 更新显示文字
    lv_label_set_text(label, eos_colck_get_time_str(false));
}
eos_app_header_t *eos_app_header_create(lv_obj_t *scr)
{
    EOS_CHECK_PTR_RETURN_VAL(scr, NULL);

    eos_app_header_t *header = lv_malloc(sizeof(eos_app_header_t));
    EOS_CHECK_PTR_RETURN_VAL_FREE(header, NULL, header);
    memset(header, 0, sizeof(eos_app_header_t));

    // 半透明容器
    header->container = lv_image_create(scr);
    lv_obj_set_size(header->container, lv_display_get_horizontal_resolution(NULL), APP_HEADER_HEIGHT);
    lv_obj_align(header->container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_move_foreground(header->container);
    eos_img_set_src(header->container, EOS_IMG_APP_HEADER_BG);
    lv_obj_add_event_cb(header->container, _app_header_delete_cb, LV_EVENT_DELETE, (void *)header);

    lv_coord_t header_h = APP_HEADER_HEIGHT;
    lv_coord_t header_w = lv_obj_get_width(header->container);

    // 返回按钮
    header->back_btn = eos_back_btn_create(header->container, false);
    lv_obj_set_size(header->back_btn, 64, 64);
    lv_obj_set_style_radius(header->back_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(header->back_btn, EOS_THEME_SECONDARY_COLOR, 0);
    lv_obj_set_style_bg_opa(header->back_btn, LV_OPA_80, 0);
    lv_obj_align(header->back_btn, LV_ALIGN_LEFT_MID, 20, 0);

    // 时间文字
    header->clock_label = lv_label_create(header->container);
    lv_label_set_text(header->clock_label, eos_colck_get_time_str(false));
    lv_obj_align(header->clock_label, LV_ALIGN_CENTER, 0, 0);
    header->clock_timer = lv_timer_create(clock_update_cb, APP_HEADER_CLOCK_UPDATE_PERIOD_MS, header->clock_label);

    // 标题文字
    header->title_label = lv_label_create(header->container);
    if (script_pkg_ptr && script_pkg_ptr->name)
    {
        lv_label_set_text(header->title_label, script_pkg_ptr->name);
    }
    else
    {
        lv_label_set_text(header->title_label, "");
    }
    lv_label_set_long_mode(header->title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(header->title_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(header->title_label, EOS_THEME_PRIMARY_COLOR, 0);
    lv_obj_align(header->title_label, LV_ALIGN_RIGHT_MID, -20, 0);

    return header;
}
