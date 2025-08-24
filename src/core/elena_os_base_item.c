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
#include "elena_os_core.h"
#include "elena_os_lang.h"
#include "elena_os_log.h"
#include "elena_os_nav.h"
#include "elena_os_img.h"
// Macros and Definitions

// Variables

// Function Implementations

static void _back_btn_cb(lv_event_t *e)
{
    EOS_LOG_D("NAV back");
    if (eos_nav_back_clean() != EOS_OK)
    {
        EOS_LOG_E("BACK ERR");
    }
}

lv_obj_t *eos_back_btn_create(lv_obj_t *parent, bool show_text)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_add_event_cb(btn, _back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);

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