/**
 * @file eos_panel.c
 * @brief Generic panel component
 */

#include "eos_panel.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_theme.h"
#include "eos_basic_widgets.h"
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_mem.h"
#include "eos_log.h"
#include "eos_font.h"

/* Macros and Definitions -------------------------------------*/

#define _PANEL_PAD_TOP 60
#define _PANEL_PAD_BOTTOM 40
#define _PANEL_BUTTON_RADIUS LV_RADIUS_CIRCLE

/* Function Implementations -----------------------------------*/

static void _panel_default_cancel_cb(lv_event_t *e)
{
    (void)e;
    eos_activity_back();
}

static void _eos_panel_container_delete_cb(lv_event_t *e)
{
    eos_panel_t *panel = (eos_panel_t *)lv_event_get_user_data(e);

    if (panel)
    {
        panel->container = NULL;
        eos_free(panel);
        EOS_LOG_D("Panel auto-freed on container delete: %p", (void *)panel);
    }
}

static void _eos_panel_build_content(eos_panel_t *panel, const eos_panel_cfg_t *cfg)
{
    lv_obj_t *container = panel->container;

    if (cfg->icon_type != EOS_PANEL_ICON_TYPE_NONE && cfg->icon_src != NULL)
    {
        panel->icon = lv_obj_create(container);
        lv_obj_remove_style_all(panel->icon);
        lv_obj_set_size(panel->icon, EOS_PANEL_ICON_SIZE, EOS_PANEL_ICON_SIZE);
        lv_obj_set_style_radius(panel->icon, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(panel->icon, cfg->icon_bg_color, 0);
        lv_obj_set_style_bg_opa(panel->icon, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(panel->icon, 0, 0);
        lv_obj_align(panel->icon, LV_ALIGN_TOP_MID, 0, 0);

        if (cfg->icon_type == EOS_PANEL_ICON_TYPE_SYMBOL)
        {
            lv_obj_t *icon_label = lv_label_create(panel->icon);
            lv_label_set_text(icon_label, cfg->icon_src);
            lv_obj_align(icon_label, LV_ALIGN_CENTER, 0, 3);
        }
        else if (cfg->icon_type == EOS_PANEL_ICON_TYPE_IMAGE)
        {
            lv_obj_t *icon_img = lv_image_create(panel->icon);
            lv_image_set_src(icon_img, cfg->icon_src);
            lv_obj_center(icon_img);
        }
    }

    if (cfg->title_id != 0 || cfg->title_text != NULL)
    {
        panel->title = lv_label_create(container);
        if (cfg->title_id != 0)
        {
            eos_label_set_text_id(panel->title, cfg->title_id);
        }
        else
        {
            lv_label_set_text(panel->title, cfg->title_text);
        }
        eos_label_set_font_size(panel->title, EOS_FONT_SIZE_LARGE);
        lv_obj_set_width(panel->title, EOS_PANEL_CONTENT_WIDTH);
        lv_label_set_long_mode(panel->title, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(panel->title, LV_TEXT_ALIGN_CENTER, 0);

        if (panel->icon != NULL)
        {
            lv_obj_align_to(panel->title, panel->icon, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
        }
        else
        {
            lv_obj_align(panel->title, LV_ALIGN_TOP_MID, 0, 0);
        }
    }

    if (cfg->message_id != 0 || cfg->message_text != NULL)
    {
        panel->message = lv_label_create(container);
        if (cfg->message_id != 0)
        {
            eos_label_set_text_id(panel->message, cfg->message_id);
        }
        else
        {
            lv_label_set_text(panel->message, cfg->message_text);
        }
        lv_obj_set_width(panel->message, EOS_PANEL_CONTENT_WIDTH);
        lv_label_set_long_mode(panel->message, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(panel->message, LV_TEXT_ALIGN_CENTER, 0);

        if (panel->title != NULL)
        {
            lv_obj_align_to(panel->message, panel->title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
        }
        else if (panel->icon != NULL)
        {
            lv_obj_align_to(panel->message, panel->icon, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
        }
        else
        {
            lv_obj_align(panel->message, LV_ALIGN_TOP_MID, 0, 0);
        }
    }

    panel->extra_slot = lv_obj_create(container);
    lv_obj_remove_style_all(panel->extra_slot);
    lv_obj_set_size(panel->extra_slot, EOS_PANEL_CONTENT_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(panel->extra_slot, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel->extra_slot, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel->extra_slot, 0, 0);
    lv_obj_set_style_border_width(panel->extra_slot, 0, 0);
    lv_obj_set_scrollbar_mode(panel->extra_slot, LV_SCROLLBAR_MODE_OFF);

    if (panel->message != NULL)
    {
        lv_obj_align_to(panel->extra_slot, panel->message, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    }
    else if (panel->title != NULL)
    {
        lv_obj_align_to(panel->extra_slot, panel->title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    }
    else if (panel->icon != NULL)
    {
        lv_obj_align_to(panel->extra_slot, panel->icon, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    }
    else
    {
        lv_obj_align(panel->extra_slot, LV_ALIGN_TOP_MID, 0, 0);
    }

    panel->actions = lv_obj_create(container);
    lv_obj_remove_style_all(panel->actions);
    lv_obj_set_size(panel->actions, EOS_PANEL_CONTENT_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(panel->actions, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel->actions, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(panel->actions, 10, 0);
    lv_obj_set_style_border_width(panel->actions, 0, 0);
    lv_obj_set_style_pad_row(panel->actions, 10, 0);

    lv_obj_align_to(panel->actions, panel->extra_slot, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    /* Confirm button (top) */
    if (cfg->confirm_btn_id != 0 || cfg->confirm_btn_text != NULL)
    {
        panel->confirm_btn = lv_button_create(panel->actions);
        lv_obj_set_size(panel->confirm_btn, LV_PCT(100), EOS_THEME_BUTTON_HEIGHT);
        lv_obj_set_style_radius(panel->confirm_btn, _PANEL_BUTTON_RADIUS, 0);

        lv_obj_t *confirm_label = lv_label_create(panel->confirm_btn);
        if (cfg->confirm_btn_id != 0)
        {
            eos_label_set_text_id(confirm_label, cfg->confirm_btn_id);
        }
        else
        {
            lv_label_set_text(confirm_label, cfg->confirm_btn_text);
        }
        lv_obj_center(confirm_label);

        if (cfg->confirm_cb != NULL)
        {
            lv_obj_add_event_cb(panel->confirm_btn, cfg->confirm_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    /* Cancel button (bottom) */
    if (cfg->cancel_btn_id != 0 || cfg->cancel_btn_text != NULL)
    {
        panel->cancel_btn = lv_button_create(panel->actions);
        lv_obj_set_size(panel->cancel_btn, LV_PCT(100), EOS_THEME_BUTTON_HEIGHT);
        lv_obj_set_style_radius(panel->cancel_btn, _PANEL_BUTTON_RADIUS, 0);

        lv_obj_t *cancel_label = lv_label_create(panel->cancel_btn);
        if (cfg->cancel_btn_id != 0)
        {
            eos_label_set_text_id(cancel_label, cfg->cancel_btn_id);
        }
        else
        {
            lv_label_set_text(cancel_label, cfg->cancel_btn_text);
        }
        lv_obj_center(cancel_label);

        if (cfg->cancel_cb != NULL)
        {
            lv_obj_add_event_cb(panel->cancel_btn, cfg->cancel_cb, LV_EVENT_CLICKED, NULL);
        }
        else
        {
            lv_obj_add_event_cb(panel->cancel_btn, _panel_default_cancel_cb, LV_EVENT_CLICKED, panel);
        }
    }
}

eos_panel_t *eos_panel_create_on_activity(eos_activity_t *activity, const eos_panel_cfg_t *cfg)
{
    EOS_CHECK_PTR_RETURN_VAL(cfg, NULL);

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return NULL;
    }

    /* Create panel as a fullscreen overlay on top of the existing view,
     * so that deleting it restores the original content underneath */
    eos_activity_set_app_header_visible(activity, false);

    eos_panel_t *panel = eos_panel_create(view, cfg);
    if (panel && panel->container)
    {
        lv_obj_set_pos(panel->container, 0, 0);
        lv_obj_move_foreground(panel->container);
    }
    return panel;
}

eos_panel_t *eos_panel_create(lv_obj_t *parent, const eos_panel_cfg_t *cfg)
{
    EOS_CHECK_PTR_RETURN_VAL(cfg, NULL);

    eos_panel_t *panel = (eos_panel_t *)eos_malloc(sizeof(eos_panel_t));
    if (!panel)
    {
        return NULL;
    }
    memset(panel, 0, sizeof(eos_panel_t));

    panel->container = lv_obj_create(parent);
    lv_obj_remove_style_all(panel->container);
    lv_obj_set_size(panel->container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel->container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(panel->container, EOS_COLOR_BLACK, 0);
    lv_obj_set_style_bg_opa(panel->container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel->container, 0, 0);
    lv_obj_set_scrollbar_mode(panel->container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_row(panel->container, 20, 0);
    lv_obj_set_style_pad_top(panel->container, _PANEL_PAD_TOP, 0);
    lv_obj_set_style_pad_bottom(panel->container, _PANEL_PAD_BOTTOM, 0);

    lv_obj_add_event_cb(panel->container, _eos_panel_container_delete_cb, LV_EVENT_DELETE, panel);

    _eos_panel_build_content(panel, cfg);

    return panel;
}

lv_obj_t *eos_panel_get_extra_slot(eos_panel_t *panel)
{
    if (!panel)
    {
        return NULL;
    }
    return panel->extra_slot;
}

void eos_panel_delete(eos_panel_t *panel)
{
    if (!panel)
    {
        return;
    }

    if (panel->container && lv_obj_is_valid(panel->container))
    {
        lv_obj_remove_event_cb(panel->container, _eos_panel_container_delete_cb);
        lv_obj_delete(panel->container);
    }

    eos_free(panel);
}
