/**
 * @file elena_os_theme.c
 * @brief 主题色
 * @author Sab1e
 * @date 2025-08-27
 */

#include "elena_os_theme.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "lv_theme_private.h"
#include "elena_os_log.h"
// Macros and Definitions
#define TEXT_COLOR lv_color_hex(0xffffff)

#define BACKGROUND_COLOR lv_color_hex(0x000000)

#define SWITCH_BG_COLOR lv_color_hex(0x34C759)
// Variables
lv_style_t style_screen;
static lv_style_t style_label;
static lv_style_t style_list;
static lv_style_t style_switch;
// Function Implementations

void _init_style_screen_bg(void)
{
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, BACKGROUND_COLOR);
}

void _init_style_label_color(void)
{
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, TEXT_COLOR);
}

void _init_style_switch_color(void)
{
    lv_style_init(&style_switch);
    lv_style_set_bg_color(&style_switch, SWITCH_BG_COLOR);
}

void _init_style_list_color(void)
{
    lv_style_init(&style_list);
    lv_style_set_bg_color(&style_list, BACKGROUND_COLOR);
    lv_style_set_border_width(&style_list, 0);
}

static void _theme_apply_cb(lv_theme_t *th, lv_obj_t *obj)
{
    LV_UNUSED(th);
    if (lv_obj_check_type(obj, &lv_button_class))
    {
        // lv_obj_add_style(obj, &style_btn, 0);
    }
    else if (lv_obj_check_type(obj, &lv_label_class))
    {
        lv_obj_add_style(obj, &style_label, 0);
    }
    else if (lv_obj_check_type(obj, &lv_list_class))
    {
        lv_obj_add_style(obj, &style_list, 0);
    }
    else if (lv_obj_check_type(obj, &lv_switch_class))
    {
        // 开关开启的状态
        lv_obj_add_style(obj, &style_switch, LV_PART_INDICATOR | LV_STATE_CHECKED);
    }
}

void eos_theme_set(lv_color_t primary_color, lv_color_t secondary_color, const lv_font_t *font)
{

    _init_style_screen_bg();
    _init_style_label_color();
    _init_style_list_color();
    _init_style_switch_color();

    lv_theme_t *th_act = lv_theme_default_init(lv_display_get_default(),
                                               primary_color,
                                               secondary_color,
                                               true,
                                               font);

    static lv_theme_t th_new;
    th_new = *th_act;

    lv_theme_set_parent(&th_new, th_act);
    lv_theme_set_apply_cb(&th_new, _theme_apply_cb);

    lv_display_set_theme(lv_display_get_default(), &th_new);
}