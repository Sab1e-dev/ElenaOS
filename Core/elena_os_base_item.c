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
// Macros and Definitions

// Variables

// Function Implementations

static void _nav_back_cb(lv_event_t *e)
{
    EOS_LOG_D("NAV back");
    if (eos_nav_back_clear() != ELENA_OS_OK)
    {
        EOS_LOG_E("BACK ERR");
    }
}
lv_obj_t *eos_back_btn_create(lv_obj_t *parent, bool show_text)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_add_event_cb(btn, _nav_back_cb, LV_EVENT_CLICKED, NULL);
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