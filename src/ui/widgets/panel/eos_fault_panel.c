/**
 * @file eos_fault_panel.c
 * @brief Fault panel component for error display
 */

#include "eos_fault_panel.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include "eos_panel.h"
#include "eos_accordion.h"
#include "eos_basic_widgets.h"
#include "eos_icon.h"
#include "eos_activity.h"
#include "eos_mem.h"
#include "eos_log.h"
#include "eos_theme.h"

/* Macros and Definitions -------------------------------------*/

#define EOS_FAULT_PANEL_COLOR EOS_COLOR_RED

/* Function Implementations -----------------------------------*/

static const char *_fault_icon_get_symbol(eos_fault_icon_t icon_type, const char *custom_icon)
{
    switch (icon_type)
    {
        case EOS_FAULT_ICON_BUG:
            return RI_BUG_LINE;
        case EOS_FAULT_ICON_WARNING:
            return RI_ERROR_WARNING_LINE;
        case EOS_FAULT_ICON_INFO:
            return RI_INFORMATION_LINE;
        case EOS_FAULT_ICON_CUSTOM_SYMBOL:
            return custom_icon ? custom_icon : RI_BUG_LINE;
        default:
            return NULL;
    }
}

static eos_panel_icon_type_t _fault_icon_type_to_panel(eos_fault_icon_t icon_type)
{
    if (icon_type == EOS_FAULT_ICON_NONE)
    {
        return EOS_PANEL_ICON_TYPE_NONE;
    }
    return EOS_PANEL_ICON_TYPE_SYMBOL;
}

static void _eos_fault_panel_container_delete_cb(lv_event_t *e)
{
    eos_fault_panel_t *fault_panel = (eos_fault_panel_t *)lv_event_get_user_data(e);

    if (fault_panel)
    {
        fault_panel->panel = NULL;
        eos_free(fault_panel);
    }
}

eos_fault_panel_t *eos_fault_panel_create(const eos_fault_cfg_t *cfg)
{
    return eos_fault_panel_create_on_activity(NULL, cfg);
}

eos_fault_panel_t *eos_fault_panel_create_on_activity(eos_activity_t *activity, const eos_fault_cfg_t *cfg)
{
    EOS_CHECK_PTR_RETURN_VAL(cfg, NULL);

    eos_fault_panel_t *fault_panel = (eos_fault_panel_t *)eos_malloc(sizeof(eos_fault_panel_t));
    if (!fault_panel)
    {
        return NULL;
    }
    memset(fault_panel, 0, sizeof(eos_fault_panel_t));

    lv_color_t icon_bg = EOS_FAULT_PANEL_COLOR;

    eos_panel_cfg_t panel_cfg = {
        .icon_bg_color = icon_bg,
        .icon_type = _fault_icon_type_to_panel(cfg->icon_type),
        .icon_src = _fault_icon_get_symbol(cfg->icon_type, cfg->custom_icon),
        .title_id = cfg->title_id,
        .title_text = cfg->title_text,
        .message_id = cfg->message_id,
        .message_text = cfg->message_text,
        .confirm_btn_id = cfg->confirm_btn_id,
        .confirm_btn_text = cfg->confirm_btn_text,
        .confirm_cb = cfg->confirm_cb,
        .cancel_btn_id = cfg->cancel_btn_id,
        .cancel_btn_text = cfg->cancel_btn_text,
        .cancel_cb = cfg->cancel_cb,
    };

    if (activity != NULL)
    {
        fault_panel->panel = eos_panel_create_on_activity(activity, &panel_cfg);
    }
    else
    {
        eos_activity_t *current = eos_activity_get_current();
        fault_panel->panel = eos_panel_create_on_activity(current, &panel_cfg);
    }

    if (!fault_panel->panel)
    {
        eos_free(fault_panel);
        return NULL;
    }

    fault_panel->extra_slot = eos_panel_get_extra_slot((eos_panel_t *)fault_panel->panel);

    lv_obj_t *container = ((eos_panel_t *)fault_panel->panel)->container;
    if (container)
    {
        lv_obj_add_event_cb(container, _eos_fault_panel_container_delete_cb, LV_EVENT_DELETE, fault_panel);
    }

    return fault_panel;
}

lv_obj_t *eos_fault_panel_get_extra_slot(eos_fault_panel_t *panel)
{
    if (!panel)
    {
        return NULL;
    }
    return panel->extra_slot;
}

void eos_fault_panel_delete(eos_fault_panel_t *panel)
{
    if (!panel)
    {
        return;
    }

    if (panel->panel)
    {
        eos_panel_t *inner_panel = (eos_panel_t *)panel->panel;

        if (inner_panel->container && lv_obj_is_valid(inner_panel->container))
        {
            lv_obj_remove_event_cb_with_user_data(inner_panel->container, _eos_fault_panel_container_delete_cb, panel);
        }

        panel->panel = NULL;
        eos_panel_delete(inner_panel);
    }

    eos_free(panel);
}
