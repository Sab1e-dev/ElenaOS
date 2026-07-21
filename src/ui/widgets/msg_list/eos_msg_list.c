/**
 * @file eos_msg_list.c
 * @brief Drop-down message list
 */

#include "eos_msg_list.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_lang.h"
#include "eos_anim.h"
#define EOS_LOG_DISABLE
#define EOS_LOG_TAG "MessageList"
#include "eos_log.h"
#include "eos_image.h"
#include "eos_config.h"
#include "eos_theme.h"
#include "eos_slide_widget.h"
#include "eos_event.h"
#include "eos_port.h"
#include "eos_mem.h"
#include "eos_crown.h"
#include "eos_panel.h"
#include "eos_widget_data.h"
#include "eos_chrome_manager.h"
#include "eos_overlay_layer.h"
/* Macros and Definitions -------------------------------------*/
#define _DEBUG_LAYOUT 0

#define _MSG_LIST_ITEM_MARGIN_BOTTOM 25

#define _ICON_WIDTH 64
#define _ICON_HEIGHT _ICON_WIDTH

#define _ICON_LARGE_WIDTH 80
#define _ICON_LARGE_HEIGHT _ICON_LARGE_WIDTH

#define _ITEM_CLICK_THRESHOLD 3
// Swipe-to-delete related macros
#define _SWIPE_THRESHOLD (EOS_DISPLAY_WIDTH / 2) // Swipe distance threshold for deletion
#define _SWIPE_ANIM_DURATION 200 // Swipe animation duration
#define _DETAIL_ANIM_DURATION 250

typedef struct
{
    eos_panel_t *panel;
    eos_msg_list_item_t *item;
    lv_obj_t *item_container;
} detail_page_data_t;

/* Variables --------------------------------------------------*/
static detail_page_data_t *_detail_data = NULL;
static eos_msg_list_t *message_list_instance = NULL;
static void _msg_list_overlay_pull_back(void);
static void _msg_list_overlay_hide(void);
static void _msg_list_overlay_on_focus(void);
static bool _msg_list_overlay_is_open(void);
static lv_obj_t *_msg_list_overlay_get_scrollable(void);
static lv_obj_t *_msg_list_overlay_get_foreground_obj(void);
static const eos_chrome_overlay_t _msg_list_overlay = {
    .pull_back = _msg_list_overlay_pull_back,
    .hide = _msg_list_overlay_hide,
    .on_focus = _msg_list_overlay_on_focus,
    .is_open = _msg_list_overlay_is_open,
    .get_scrollable = _msg_list_overlay_get_scrollable,
    .get_foreground_obj = _msg_list_overlay_get_foreground_obj,
    .name = "msg_list",
};
/* Function Implementations -----------------------------------*/

static void _msg_list_item_clicked_cb(lv_event_t *e);

/* Delete Item Callback ---------------------------------------*/

static void _del_item_cb(eos_msg_list_t *list)
{
    EOS_CHECK_PTR_RETURN(list);
    bool has_items = false;
    uint32_t child_count = lv_obj_get_child_count(list->list);
    for (uint32_t i = 0; i < child_count; i++)
    {
        lv_obj_t *child = lv_obj_get_child(list->list, i);
        if (child == list->clear_all_btn)
        {
            continue;
        }
        if (eos_wdata_get(child, EOS_WDATA_MSG_LIST_ITEM) != NULL)
        {
            has_items = true;
            break;
        }
    }

    if (!has_items)
    {
        if (list->no_msg_label)
        {
            lv_obj_remove_flag(list->no_msg_label, LV_OBJ_FLAG_HIDDEN);
        }
        if (list->clear_all_btn)
        {
            lv_obj_add_flag(list->clear_all_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* Swipe-to-Delete Related Callbacks --------------------------*/

static void _slide_widget_reached_threashold_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    eos_msg_list_t *list = (eos_msg_list_t *)lv_event_get_user_data(e);
    eos_msg_list_item_t *item = (eos_msg_list_item_t *)eos_wdata_get(obj, EOS_WDATA_MSG_LIST_ITEM);
    if (item)
    {
        eos_msg_list_item_delete(item);
        _del_item_cb(list);
    }
}

/* Detail Page Related Callbacks ------------------------------*/

static void _delete_anim_end_cb(eos_anim_t *a)
{
    if (!_detail_data)
        return;

    eos_msg_list_item_t *item = _detail_data->item;

    if (_detail_data->panel)
    {
        eos_panel_delete(_detail_data->panel);
    }

    if (item && !item->is_deleted)
    {
        eos_msg_list_t *msg_list = item->msg_list;
        eos_msg_list_item_delete(item);
        if (msg_list)
        {
            _del_item_cb(msg_list);
        }
    }

    eos_free(_detail_data);
    _detail_data = NULL;
}

static void _dismiss_anim_end_cb(eos_anim_t *a)
{
    if (!_detail_data)
        return;

    if (_detail_data->panel)
    {
        eos_panel_delete(_detail_data->panel);
    }

    eos_free(_detail_data);
    _detail_data = NULL;
}

static void _dismiss_btn_click_cb(lv_event_t *e)
{
    if (!_detail_data)
        return;

    lv_obj_t *item_cont = _detail_data->item_container;
    if (item_cont)
    {
        eos_anim_t *item_fade =
            eos_anim_fade_create(item_cont, LV_OPA_TRANSP, LV_OPA_COVER, _DETAIL_ANIM_DURATION, false);
        if (item_fade)
        {
            eos_anim_fade_set_main_opa(item_fade, true);
            eos_anim_start(item_fade);
        }
    }

    eos_anim_t *scale_anim =
        eos_anim_transform_scale_create(_detail_data->panel->container, 256, 0, _DETAIL_ANIM_DURATION, false);
    if (scale_anim)
    {
        eos_anim_add_cb(scale_anim, _dismiss_anim_end_cb, NULL);
        eos_anim_start(scale_anim);
    }
    else
    {
        if (item_cont)
        {
            lv_obj_set_style_opa(item_cont, LV_OPA_COVER, 0);
        }
        eos_panel_delete(_detail_data->panel);
        eos_free(_detail_data);
        _detail_data = NULL;
    }
}

static void _mark_as_read_btn_click_cb(lv_event_t *e)
{
    if (!_detail_data)
        return;

    eos_anim_t *fade_anim =
        eos_anim_fade_create(_detail_data->panel->container, LV_OPA_COVER, LV_OPA_TRANSP, _DETAIL_ANIM_DURATION, false);
    if (fade_anim)
    {
        eos_anim_add_cb(fade_anim, _delete_anim_end_cb, NULL);
        eos_anim_start(fade_anim);
    }
    else
    {
        eos_msg_list_item_t *item = _detail_data->item;
        eos_msg_list_t *msg_list = item ? item->msg_list : NULL;
        eos_panel_delete(_detail_data->panel);
        if (item && !item->is_deleted)
        {
            eos_msg_list_item_delete(item);
            if (msg_list)
            {
                _del_item_cb(msg_list);
            }
        }
        eos_free(_detail_data);
        _detail_data = NULL;
    }
}

static void _msg_list_item_clicked_cb(lv_event_t *e)
{
    eos_slide_widget_t *sw = (eos_slide_widget_t *)lv_event_get_param(e);
    if (abs(eos_slide_widget_get_displacement(sw)) > _ITEM_CLICK_THRESHOLD)
        return;

    lv_obj_t *item_container = lv_event_get_current_target(e);
    eos_msg_list_item_t *item = (eos_msg_list_item_t *)eos_wdata_get(item_container, EOS_WDATA_MSG_LIST_ITEM);
    EOS_CHECK_PTR_RETURN(item && !item->is_deleted && item->container && !_detail_data);

    _detail_data = eos_malloc_zeroed(sizeof(detail_page_data_t));
    EOS_CHECK_PTR_RETURN(_detail_data);
    _detail_data->item = item;
    _detail_data->item_container = item->container;

    const void *icon_src = item->icon ? lv_image_get_src(item->icon) : NULL;

    eos_panel_cfg_t cfg = {
        .icon_bg_color = EOS_COLOR_BLACK,
        .icon_type = icon_src ? EOS_PANEL_ICON_TYPE_IMAGE : EOS_PANEL_ICON_TYPE_NONE,
        .icon_src = icon_src,
        .title_text = lv_label_get_text(item->title_label),
        .message_text = item->msg_str,
        .confirm_btn_id = STR_ID_APP_FLASH_LIGHT_DISMISS,
        .confirm_cb = _dismiss_btn_click_cb,
        .cancel_btn_id = STR_ID_MSG_LIST_ITEM_MARK_AS_READ,
        .cancel_cb = _mark_as_read_btn_click_cb,
    };

    eos_panel_t *panel = eos_panel_create(lv_layer_top(), &cfg);
    if (!panel)
    {
        eos_free(_detail_data);
        _detail_data = NULL;
        return;
    }

    _detail_data->panel = panel;

    lv_obj_set_style_bg_color(panel->container, EOS_COLOR_BLACK, 0);
    lv_obj_set_style_bg_opa(panel->container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel->container, EOS_DISPLAY_RADIUS, 0);

    if (panel->icon)
    {
        lv_obj_set_size(panel->icon, _ICON_LARGE_WIDTH, _ICON_LARGE_HEIGHT);
        lv_obj_t *icon_img = lv_obj_get_child(panel->icon, 0);
        if (icon_img)
        {
            eos_img_set_size(icon_img, _ICON_LARGE_WIDTH, _ICON_LARGE_HEIGHT);
        }
    }

    lv_obj_set_style_opa_layered(panel->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_transform_scale(panel->container, 0, 0);

    lv_area_t item_coords;
    lv_obj_get_coords(item->container, &item_coords);
    int32_t item_center_x = (item_coords.x1 + item_coords.x2) / 2;
    int32_t item_center_y = (item_coords.y1 + item_coords.y2) / 2;
    lv_obj_set_style_transform_pivot_x(panel->container, item_center_x, 0);
    lv_obj_set_style_transform_pivot_y(panel->container, item_center_y, 0);

    {
        eos_anim_t *item_fade =
            eos_anim_fade_create(item->container, LV_OPA_COVER, LV_OPA_TRANSP, _DETAIL_ANIM_DURATION, false);
        if (item_fade)
        {
            eos_anim_fade_set_main_opa(item_fade, true);
            eos_anim_start(item_fade);
        }
    }

    eos_anim_t *scale_anim = eos_anim_transform_scale_create(panel->container, 0, 256, _DETAIL_ANIM_DURATION, false);
    if (scale_anim)
    {
        eos_anim_start(scale_anim);
    }

    eos_anim_t *fade_anim =
        eos_anim_fade_create(panel->container, LV_OPA_TRANSP, LV_OPA_COVER, _DETAIL_ANIM_DURATION, false);
    if (fade_anim)
    {
        eos_anim_start(fade_anim);
    }
}

/* Public Functions************************* ------------------*/

eos_msg_list_item_t *eos_msg_list_item_create(eos_msg_list_t *list)
{
    eos_msg_list_item_t *item = eos_malloc_zeroed(sizeof(eos_msg_list_item_t));
    EOS_CHECK_PTR_RETURN_VAL(list, NULL);
    EOS_CHECK_PTR_RETURN_VAL_FREE(item, NULL, item);

    item->msg_list = list;
    item->is_deleted = false;
    // Create container
    item->container = lv_button_create(list->list);
    EOS_CHECK_PTR_RETURN_VAL_FREE(item->container, NULL, item);
    lv_obj_set_size(item->container, lv_pct(100), LV_SIZE_CONTENT);
    // Vertical layout for item
    lv_obj_set_flex_flow(item->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(item->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(item->container, EOS_COLOR_DARK_GREY_2, 0);
    lv_obj_set_style_border_width(item->container, 0, 0);
    lv_obj_set_style_margin_bottom(item->container, _MSG_LIST_ITEM_MARGIN_BOTTOM, 0);
    lv_obj_set_style_pad_all(item->container, 20, 0);
    lv_obj_set_style_align(item->container, LV_ALIGN_CENTER, 0);
    lv_obj_set_style_radius(item->container, 30, 0);
    eos_wdata_set(item->container, EOS_WDATA_MSG_LIST_ITEM, item, NULL);
    lv_obj_set_style_translate_x(item->container, 0, 0);
    lv_obj_set_style_shadow_width(item->container, 0, 0);
    lv_obj_remove_flag(item->container, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_update_layout(item->container);
    eos_slide_widget_t *sw = eos_slide_widget_create_with_touch(item->container,
                                                                item->container,
                                                                EOS_SLIDE_DIR_HOR,
                                                                EOS_DISPLAY_WIDTH,
                                                                EOS_THRESHOLD_40);
    eos_slide_widget_set_bidirectional(sw, true);
    eos_slide_widget_add_event_cb_reached_threshold(sw, _slide_widget_reached_threashold_cb, list);
    eos_slide_widget_add_event_cb_reverted(sw, _msg_list_item_clicked_cb, NULL);
    // First row of item layout
    item->row1 = lv_obj_create(item->container);
    lv_obj_set_size(item->row1, lv_pct(100), 64);
    lv_obj_set_style_bg_opa(item->row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item->row1, 0, 0);
    lv_obj_set_style_pad_all(item->row1, 0, 0);
    lv_obj_remove_flag(item->row1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(item->row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item->row1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_BETWEEN);
#if _DEBUG_LAYOUT
    lv_obj_set_style_bg_opa(item->row1, LV_OPA_60, 0);
    lv_obj_set_style_bg_color(item->row1, EOS_COLOR_RED, 0);
#endif
    // Icon area
    item->icon = lv_image_create(item->row1);
    lv_obj_set_size(item->icon, _ICON_WIDTH, _ICON_HEIGHT);
    lv_obj_set_style_bg_opa(item->icon, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item->icon, 0, 0);
    lv_obj_set_style_margin_all(item->icon, 0, 0);
    lv_obj_remove_flag(item->icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_image_set_src(item->icon, EOS_IMG_APP);
    eos_img_set_size(item->icon, _ICON_WIDTH, _ICON_HEIGHT);

    // Title label
    item->title_label = lv_label_create(item->row1);
    lv_label_set_long_mode(item->title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_flex_grow(item->title_label, 1);

    // Time label
    item->time_label = lv_label_create(item->row1);
    lv_obj_set_style_text_align(item->time_label, LV_TEXT_ALIGN_RIGHT, 0);

    // Message content
    item->msg_label = lv_label_create(item->container);
    lv_obj_set_width(item->msg_label, lv_pct(100));
    lv_label_set_long_mode(item->msg_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_margin_ver(item->msg_label, 16, 0);
    lv_obj_align_to(item->msg_label, item->row1, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Set event bubble to avoid affecting container's Clicked event
    lv_obj_add_flag(item->row1, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(item->icon, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(item->title_label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(item->time_label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(item->msg_label, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Update UI state when adding new message
    if (list->no_msg_label)
    {
        lv_obj_add_flag(list->no_msg_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (list->clear_all_btn)
    {
        lv_obj_remove_flag(list->clear_all_btn, LV_OBJ_FLAG_HIDDEN);
    }

    return item;
}

void eos_msg_list_item_delete(eos_msg_list_item_t *item)
{
    EOS_CHECK_PTR_RETURN(item);

    // Clear container user data
    if (item->container)
    {
        eos_wdata_remove(item->container, EOS_WDATA_MSG_LIST_ITEM);
    }

    // Free message string
    if (item->msg_str)
    {
        eos_free(item->msg_str);
        item->msg_str = NULL;
    }

    // Delete container directly (LVGL will automatically delete child objects)
    if (item->container)
    {
        lv_obj_delete_async(item->container);
        item->container = NULL;
    }

    // Free struct
    eos_free(item);
}

void eos_msg_list_item_set_msg(eos_msg_list_item_t *item, const char *msg)
{
    EOS_CHECK_PTR_RETURN(item);

    // Free old message
    if (item->msg_str)
    {
        eos_free(item->msg_str);
        item->msg_str = NULL;
    }

    if (!msg || strlen(msg) == 0)
    {
        lv_label_set_text(item->msg_label, "");
    }
    else
    {
        // Allocate new memory and copy
        item->msg_str = eos_malloc(strlen(msg) + 1);
        if (item->msg_str)
        {
            strcpy(item->msg_str, msg);
            lv_label_set_text(item->msg_label, item->msg_str);
        }
        else
        {
            lv_label_set_text(item->msg_label, "");
        }
    }
}

void eos_msg_list_item_set_title(eos_msg_list_item_t *item, const char *title)
{
    EOS_CHECK_PTR_RETURN(item);
    lv_label_set_text(item->title_label, title ? title : "");
}

void eos_msg_list_item_set_time(eos_msg_list_item_t *item, const char *time)
{
    EOS_CHECK_PTR_RETURN(item);
    lv_label_set_text(item->time_label, time ? time : "");
}

void eos_msg_list_item_icon_set_src(eos_msg_list_item_t *item, const char *src)
{
    EOS_CHECK_PTR_RETURN(item && src);
    lv_image_set_src(item->icon, src);
    eos_img_set_size(item->icon, _ICON_WIDTH, _ICON_HEIGHT);
}

void eos_msg_list_clear_all(eos_msg_list_t *msg_list)
{
    EOS_CHECK_PTR_RETURN(msg_list && msg_list->list);

    // Get all child objects
    uint32_t child_count = lv_obj_get_child_count(msg_list->list);
    lv_obj_t **children = eos_malloc(sizeof(lv_obj_t *) * child_count);

    // Save child object pointers (avoid traversal issues during dynamic deletion)
    for (uint32_t i = 0; i < child_count; i++)
    {
        children[i] = lv_obj_get_child(msg_list->list, i);
    }

    // Delete child objects
    for (uint32_t i = 0; i < child_count; i++)
    {
        lv_obj_t *child = children[i];
        if (child == msg_list->clear_all_btn)
        {
            continue;
        }
        eos_msg_list_item_t *item = (eos_msg_list_item_t *)eos_wdata_get(child, EOS_WDATA_MSG_LIST_ITEM);
        if (item)
        {
            eos_msg_list_item_delete(item);
        }
        else
        {
            lv_obj_delete_async(child);
        }
    }

    eos_free(children);
}

/* Clear Button Related Callbacks -----------------------------*/

/**
 * @brief Callback for message animation end
 */
static void _msg_list_item_anim_end_cb(eos_anim_t *a)
{
    eos_msg_list_t *list = (eos_msg_list_t *)a->user_data;
    EOS_CHECK_PTR_RETURN(list);
    // Decrease animation count
    if (list->animating_count > 0)
    {
        list->animating_count--;
    }

    // Check if all animations are completed
    if (list->animating_count == 0)
    {
        // Directly clear all contents
        eos_msg_list_clear_all(list);
        eos_swipe_panel_pull_back(list->swipe_panel);
        _del_item_cb(list);
    }
}

/**
 * @brief Trigger message deletion animation
 * @param list Target message list
 */
static void _trigger_msg_anims(eos_msg_list_t *list)
{
    EOS_CHECK_PTR_RETURN(list);
    static uint8_t anim_index = 0; // Static variable to track current animation index
    lv_obj_t *parent = list->list;
    uint8_t child_count = lv_obj_get_child_count(parent);

    // Process only one message item animation at a time
    for (uint8_t i = anim_index; i < child_count && list->animating_count < 2; i++)
    {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        eos_msg_list_item_t *item = (eos_msg_list_item_t *)eos_wdata_get(child, EOS_WDATA_MSG_LIST_ITEM);

        // Skip the clear button itself
        if (child == list->clear_all_btn)
        {
            anim_index++;
            continue;
        }

        if (item)
        {
            // Create animation
            eos_anim_t *fade_anim = eos_anim_fade_create(item->container, LV_OPA_COVER, LV_OPA_TRANSP, 300, false);
            if (fade_anim)
                eos_anim_start(fade_anim);
            eos_anim_t *move_anim = eos_anim_move_create(item->container,
                                                         lv_obj_get_x(item->container),
                                                         0,
                                                         EOS_DISPLAY_WIDTH,
                                                         0,
                                                         300,
                                                         false);
            if (move_anim)
            {
                eos_anim_add_cb(move_anim, _msg_list_item_anim_end_cb, list);
                eos_anim_start(move_anim);
            }

            list->animating_count++;
            anim_index++;
        }
    }

    anim_index = 0;
}

/**
 * @brief Callback for clear all messages button
 */
static void _msg_list_clear_all_btn_cb(lv_event_t *e)
{
    eos_msg_list_t *list = (eos_msg_list_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(list);
    // Hide no-message label
    if (list->no_msg_label)
    {
        lv_obj_add_flag(list->no_msg_label, LV_OBJ_FLAG_HIDDEN);
    }

    // Reset animation count
    list->animating_count = 0;

    // Trigger animations
    _trigger_msg_anims(list);
}

/* List Related Functions -------------------------------------*/

static void _msg_list_deleted_cb(lv_event_t *e)
{
    eos_msg_list_t *list = (eos_msg_list_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(list);

    // Clear all message items
    eos_msg_list_clear_all(list);

    // Delete no message label
    if (list->no_msg_label)
    {
        lv_obj_delete_async(list->no_msg_label);
        list->no_msg_label = NULL;
    }

    // Delete swipe panel (will automatically delete all internal objects)
    if (list->swipe_panel)
    {
        eos_swipe_panel_delete(list->swipe_panel);
        list->swipe_panel = NULL;
    }

    // Free list struct
    eos_free(list);
}

static void _slide_widget_reached_threshold_cb(lv_event_t *e)
{
    if (lv_obj_get_y(message_list_instance->swipe_panel->swipe_obj) >= 0)
        eos_crown_encoder_set_target_obj(message_list_instance->list);
}

static void _msg_list_opened_cb(lv_event_t *e)
{
    EOS_LOG_I("Message list opened");
    eos_chrome_manager_notify_overlay_opened(&_msg_list_overlay);
}

static void _msg_list_closed_cb(lv_event_t *e)
{
    EOS_LOG_I("Message list closed");
    eos_chrome_manager_notify_overlay_closed(&_msg_list_overlay);
}

static void _msg_list_overlay_pull_back(void)
{
    eos_msg_list_close_detail();
    eos_msg_list_t *msg_list = eos_msg_list_get_instance();
    if (msg_list && msg_list->swipe_panel)
    {
        eos_swipe_panel_pull_back(msg_list->swipe_panel);
    }
}

static void _msg_list_overlay_hide(void)
{
    eos_msg_list_close_detail();
    eos_msg_list_hide();
}

static void _msg_list_overlay_on_focus(void)
{
    EOS_LOG_D("Msg list focused");
}

static bool _msg_list_overlay_is_open(void)
{
    eos_msg_list_t *msg_list = eos_msg_list_get_instance();
    if (msg_list && msg_list->swipe_panel && msg_list->swipe_panel->sw)
    {
        return eos_slide_widget_get_state(msg_list->swipe_panel->sw) == EOS_SLIDE_WIDGET_STATE_OPEN;
    }
    return false;
}

static lv_obj_t *_msg_list_overlay_get_scrollable(void)
{
    eos_msg_list_t *msg_list = eos_msg_list_get_instance();
    if (msg_list && msg_list->list)
    {
        return msg_list->list;
    }
    return NULL;
}

static lv_obj_t *_msg_list_overlay_get_foreground_obj(void)
{
    eos_msg_list_t *msg_list = eos_msg_list_get_instance();
    if (msg_list && msg_list->swipe_panel && msg_list->swipe_panel->sw)
    {
        return eos_slide_widget_get_touch_obj(msg_list->swipe_panel->sw);
    }
    return NULL;
}

eos_msg_list_t *eos_msg_list_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);
    eos_msg_list_t *msg_list = eos_malloc_zeroed(sizeof(eos_msg_list_t));
    EOS_CHECK_PTR_RETURN_VAL_FREE(msg_list, NULL, msg_list);
    _detail_data = NULL;

    msg_list->swipe_panel = eos_swipe_panel_create(parent);
    EOS_CHECK_PTR_RETURN_VAL_FREE(msg_list->swipe_panel, NULL, msg_list);
    eos_swipe_panel_set_dir(msg_list->swipe_panel, EOS_SWIPE_DIR_DOWN);
    eos_crown_encoder_register_slide_widget(msg_list->swipe_panel->sw);
    eos_slide_widget_add_event_cb_reached_threshold(msg_list->swipe_panel->sw,
                                                    _slide_widget_reached_threshold_cb,
                                                    NULL);
    eos_slide_widget_add_event_cb_opened(msg_list->swipe_panel->sw, _msg_list_opened_cb, NULL);
    eos_slide_widget_add_event_cb_closed(msg_list->swipe_panel->sw, _msg_list_closed_cb, NULL);

    msg_list->list = lv_list_create(msg_list->swipe_panel->swipe_obj);
    EOS_CHECK_PTR_RETURN_VAL_FREE(msg_list->list, NULL, msg_list);
    lv_obj_set_size(msg_list->list, LV_PCT(100), LV_PCT(88));
    lv_obj_center(msg_list->list);
    lv_obj_set_style_bg_opa(msg_list->list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(msg_list->list, 0, 0);
    lv_obj_set_style_pad_all(msg_list->list, 30, 0);
    lv_obj_align(msg_list->list, LV_ALIGN_TOP_MID, 0, 0);
    eos_wdata_set(msg_list->list, EOS_WDATA_MSG_LIST, msg_list, NULL);
    lv_obj_set_scroll_dir(msg_list->list, LV_DIR_VER);
    lv_obj_add_event_cb(msg_list->list, _msg_list_deleted_cb, LV_EVENT_DELETE, msg_list);
    // Create clear all button
    msg_list->clear_all_btn = lv_button_create(msg_list->list);
    lv_obj_set_size(msg_list->clear_all_btn, lv_pct(100), 80);
    lv_obj_remove_flag(msg_list->clear_all_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(msg_list->clear_all_btn, EOS_COLOR_DARK_GREY_2, 0);
    lv_obj_set_style_border_width(msg_list->clear_all_btn, 0, 0);
    lv_obj_set_style_margin_bottom(msg_list->clear_all_btn, _MSG_LIST_ITEM_MARGIN_BOTTOM, 0);
    lv_obj_set_style_pad_all(msg_list->clear_all_btn, 0, 0);
    lv_obj_set_style_align(msg_list->clear_all_btn, LV_ALIGN_CENTER, 0);
    lv_obj_set_style_radius(msg_list->clear_all_btn, 50, 0);
    lv_obj_set_style_shadow_width(msg_list->clear_all_btn, 0, 0);

    // Hide clear button initially
    lv_obj_add_flag(msg_list->clear_all_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *clear_all_label = lv_label_create(msg_list->clear_all_btn);
    eos_label_set_text_id(clear_all_label, STR_ID_MSG_LIST_CLEAR_ALL);

    lv_obj_center(clear_all_label);

    lv_obj_add_event_cb(msg_list->clear_all_btn, _msg_list_clear_all_btn_cb, LV_EVENT_CLICKED, msg_list);

    // Create no message label
    msg_list->no_msg_label = lv_label_create(msg_list->swipe_panel->swipe_obj);
    eos_label_set_text_id(msg_list->no_msg_label, STR_ID_MSG_LIST_NO_MSG);

    lv_obj_center(msg_list->no_msg_label);

    return msg_list;
}

void eos_msg_list_close_detail(void)
{
    if (!_detail_data)
        return;

    if (_detail_data->item_container)
    {
        lv_obj_set_style_opa(_detail_data->item_container, LV_OPA_COVER, 0);
    }

    if (_detail_data->panel)
    {
        eos_panel_delete(_detail_data->panel);
    }

    eos_free(_detail_data);
    _detail_data = NULL;
}

void eos_msg_list_hide(void)
{
    EOS_CHECK_PTR_RETURN(message_list_instance);
    eos_msg_list_close_detail();
    eos_swipe_panel_hide(message_list_instance->swipe_panel);
    eos_crown_encoder_activate_current_overlay_scrollable();
}

void eos_msg_list_show(void)
{
    EOS_CHECK_PTR_RETURN(message_list_instance);
    eos_swipe_panel_show(message_list_instance->swipe_panel);
    eos_crown_encoder_activate_current_overlay_scrollable();
}

eos_msg_list_t *eos_msg_list_get_instance(void)
{
    return message_list_instance;
}

void eos_msg_list_init(void)
{
    message_list_instance = eos_msg_list_create(eos_overlay_get_overlay_layer());
}

const eos_chrome_overlay_t *eos_msg_list_get_overlay_descriptor(void)
{
    return &_msg_list_overlay;
}
