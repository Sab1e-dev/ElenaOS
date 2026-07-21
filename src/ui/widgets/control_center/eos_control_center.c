/**
 * @file eos_control_center.c
 * @brief Pull-up control center
 */

#include "eos_control_center.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "eos_swipe_panel.h"
#define EOS_LOG_TAG "ControlCenter"
#include "eos_log.h"
#include "eos_theme.h"
#include "eos_service_config.h"
#include "eos_port.h"
#include "eos_service_audio.h"
#include "eos_config.h"
#include "eos_event.h"
#include "eos_icon.h"
#include "eos_flash_light.h"
#include "eos_anim.h"
#include "eos_lang.h"
#include "eos_service_display.h"
#include "eos_settings.h"
#include "eos_mem.h"
#include "eos_basic_widgets.h"
#include "eos_crown.h"
#include "eos_service_battery.h"
#include "eos_chrome_manager.h"
#include "eos_overlay_layer.h"
#include "eos_activity.h"
#include "eos_widget_data.h"

/* Macros and Definitions -------------------------------------*/
#define _BTN_DEFAULT_COLOR EOS_THEME_SECONDARY_COLOR
#define _SLIDER_DEFAULT_WIDTH 150
#define _SLIDER_DEFAULT_HEIGHT 360
#define _SLIDER_DEFAULT_RADIUS 50
#define _LIST_SCALE_THRESHOLD_Y ((float)EOS_DISPLAY_HEIGHT * 0.8)
/* Variables --------------------------------------------------*/
static eos_control_center_t *control_center_instance = NULL;
static void _control_center_overlay_pull_back(void);
static void _control_center_overlay_hide(void);
static void _control_center_overlay_on_focus(void);
static bool _control_center_overlay_is_open(void);
static lv_obj_t *_control_center_overlay_get_scrollable(void);
static lv_obj_t *_control_center_overlay_get_foreground_obj(void);
static const eos_chrome_overlay_t _control_center_overlay = {
    .pull_back = _control_center_overlay_pull_back,
    .hide = _control_center_overlay_hide,
    .on_focus = _control_center_overlay_on_focus,
    .is_open = _control_center_overlay_is_open,
    .get_scrollable = _control_center_overlay_get_scrollable,
    .get_foreground_obj = _control_center_overlay_get_foreground_obj,
    .name = "control_center",
};
/* Function Implementations -----------------------------------*/
void eos_control_panel_slide_change(void);

/* List callback ----------------------------------------------*/

static void _list_scroll_cb(lv_event_t *e)
{
    lv_obj_t *list = lv_event_get_user_data(e);
    lv_coord_t list_h = lv_obj_get_height(list);
    lv_coord_t list_y = lv_obj_get_y(list);

    uint32_t count = lv_obj_get_child_count(list);

    lv_coord_t bottom_threshold = list_y + (list_h * 8) / 10;
    lv_coord_t list_bottom = list_y + list_h;

    for (uint32_t i = 0; i < count; i++)
    {
        lv_obj_t *child = lv_obj_get_child(list, i);

        lv_coord_t w = lv_obj_get_width(child);
        lv_coord_t h = lv_obj_get_height(child);

        if (i % 2 == 0)
        {
            // Left column -> Upper right corner
            lv_obj_set_style_transform_pivot_x(child, w, 0);
            lv_obj_set_style_transform_pivot_y(child, 0, 0);
        }
        else
        {
            // Right column -> Upper left corner
            lv_obj_set_style_transform_pivot_x(child, 0, 0);
            lv_obj_set_style_transform_pivot_y(child, 0, 0);
        }

        lv_area_t child_area;
        lv_obj_get_coords(child, &child_area);

        lv_coord_t child_center = (child_area.y1 + child_area.y2) / 2;

        if (child_center < bottom_threshold)
        {
            lv_obj_set_style_transform_scale(child, 256, 0);
            continue;
        }

        lv_coord_t diff = list_bottom - child_center;
        const lv_coord_t range = list_h / 10;
        const int16_t max_scale = 256; // Original scale
        const int16_t min_scale = 180; // Minimum scale (when just entering)

        int16_t scale = min_scale + (diff * (max_scale - min_scale)) / range;
        if (scale > max_scale)
            scale = max_scale;
        else if (scale < min_scale)
            scale = min_scale;

        lv_obj_set_style_transform_scale(child, scale, 0);
    }
}

static void _slide_widget_reached_threshold_cb(lv_event_t *e)
{
    lv_obj_t *container = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_scroll_to_y(container, 0, LV_ANIM_OFF);
    if (lv_obj_get_y(control_center_instance->swipe_panel->swipe_obj) < EOS_DISPLAY_HEIGHT)
        eos_crown_encoder_set_target_obj(container);
}

/* Basic components -------------------------------------------*/

static lv_obj_t *_control_center_create_switch_btn(lv_obj_t *parent, const char *symbol, lv_color_t color)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(btn, 160, 100);
    lv_obj_set_style_radius(btn, 50, 0);
    lv_obj_set_style_bg_color(btn, _BTN_DEFAULT_COLOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, color, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, symbol);
    lv_obj_center(label);
    return btn;
}

static void _control_center_slider_page_clicked_cb(lv_event_t *e)
{
    lv_obj_t *page = lv_event_get_target(e);
    eos_anim_fade_start(page, LV_OPA_80, LV_OPA_TRANSP, 300, true);
}

static lv_obj_t *_control_center_slider_create(const char *symbol)
{
    lv_obj_t *slider_page = lv_obj_create(eos_overlay_get_overlay_layer());
    lv_obj_remove_style_all(slider_page);
    lv_obj_set_size(slider_page, lv_pct(100), lv_pct(100));
    lv_obj_move_foreground(slider_page);
    lv_obj_set_style_bg_opa(slider_page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(slider_page, EOS_COLOR_BLACK, 0);
    lv_obj_add_event_cb(slider_page, _control_center_slider_page_clicked_cb, LV_EVENT_CLICKED, NULL);

    eos_anim_t *a = eos_anim_fade_create(slider_page, LV_OPA_TRANSP, LV_OPA_80, 300, false);
    eos_anim_fade_set_layered(a, false);
    eos_anim_start(a);

    lv_obj_t *slider_mask = lv_obj_create(slider_page);
    lv_obj_remove_style_all(slider_mask);
    lv_obj_set_size(slider_mask, _SLIDER_DEFAULT_WIDTH, _SLIDER_DEFAULT_HEIGHT);
    lv_obj_center(slider_mask);
    lv_obj_set_style_clip_corner(slider_mask, true, 0);
    lv_obj_set_style_radius(slider_mask, 50, 0);

    lv_obj_t *slider = lv_slider_create(slider_mask);
    lv_bar_set_orientation(slider, LV_BAR_ORIENTATION_VERTICAL);
    lv_obj_set_size(slider, _SLIDER_DEFAULT_WIDTH, _SLIDER_DEFAULT_HEIGHT);
    lv_obj_center(slider);

    lv_obj_set_style_clip_corner(slider, true, LV_PART_MAIN);

    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);

    lv_obj_set_style_width(slider, _SLIDER_DEFAULT_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_height(slider, _SLIDER_DEFAULT_HEIGHT, LV_PART_MAIN);

    lv_obj_set_style_bg_color(slider, EOS_COLOR_WHITE, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 50, LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(slider, EOS_COLOR_DARK_GREY_1, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 50, LV_PART_MAIN);

    lv_obj_set_style_bg_color(slider, EOS_COLOR_WHITE, LV_PART_INDICATOR | LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(slider_page);
    lv_label_set_text(label, symbol);
    eos_wdata_set(slider, EOS_WDATA_SLIDER_LABEL, (void *)label, NULL);
    lv_obj_set_style_text_color(label, EOS_COLOR_BLACK, 0);
    lv_obj_move_foreground(label);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -100);

    lv_obj_update_layout(label);
    lv_obj_set_style_transform_pivot_x(label, lv_obj_get_width(label) / 2, 0);
    lv_obj_set_style_transform_pivot_y(label, lv_obj_get_height(label) / 2, 0);

    eos_anim_fade_start(slider, LV_OPA_TRANSP, LV_OPA_COVER, 300, false);

    return slider;
}

static lv_obj_t *_control_center_create_btn(lv_obj_t *parent, const char *symbol)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 150, 100);
    lv_obj_set_style_radius(btn, 50, 0);
    lv_obj_set_style_bg_color(btn, _BTN_DEFAULT_COLOR, 0);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, symbol);
    lv_obj_center(label);
    return btn;
}

/* Functional components --------------------------------------*/

static void _control_center_bluetooth_switch_btn_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);

    if (lv_obj_has_state(btn, LV_STATE_CHECKED))
    {
        EOS_LOG_D("CHECKED");
        eos_bluetooth_enable();
        eos_config_set_bool(EOS_CONFIG_KEY_BLUETOOTH_BOOL, true);
    }
    else
    {
        EOS_LOG_D("UNCHECKED");
        eos_bluetooth_disable();
        eos_config_set_bool(EOS_CONFIG_KEY_BLUETOOTH_BOOL, false);
    }
}

static void _control_center_brightness_value_changed_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t *)eos_wdata_get(slider, EOS_WDATA_SLIDER_LABEL);
    EOS_CHECK_PTR_RETURN(label);

    // Get current Slider value
    int32_t value = lv_slider_get_value(slider);
    eos_display_set_brightness((uint8_t)value, EOS_DISPLAY_DURATION_OFF, false);

    // Map Slider value to angle
    int32_t angle = (int32_t)value * 18;

    // Set Label rotation
    lv_obj_set_style_transform_rotation(label, angle, 0);
}

static void _control_center_brightness_slider_delete_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    eos_config_set_number(EOS_CONFIG_KEY_DISPLAY_BRIGHTNESS_NUMBER, lv_slider_get_value(slider));
}

static void _control_center_brightness_btn_clicked_cb(lv_event_t *e)
{
    lv_obj_t *slider = _control_center_slider_create(RI_SUN_LINE);
    lv_slider_set_range(slider, EOS_DISPLAY_BRIGHTNESS_MIN, EOS_DISPLAY_BRIGHTNESS_MAX);
    lv_slider_set_value(slider, eos_config_get_number(EOS_CONFIG_KEY_DISPLAY_BRIGHTNESS_NUMBER, 50), LV_ANIM_ON);
    lv_obj_add_event_cb(slider, _control_center_brightness_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, _control_center_brightness_slider_delete_cb, LV_EVENT_DELETE, NULL);
}

static void _control_center_battery_update_display(lv_obj_t *label)
{
    EOS_CHECK_PTR_RETURN(label);

    eos_battery_state_t state;
    if (eos_battery_get_state(&state))
    {
        if (state.charging)
        {
            lv_label_set_text_fmt(label, RI_FLASHLIGHT_FILL " %d%%", state.percent);
        }
        else
        {
            lv_label_set_text_fmt(label, "%d%%", state.percent);
        }
    }
    else
    {
        lv_label_set_text(label, "N/A");
    }
}

static void _control_center_battery_level_update_cb(eos_event_t *e)
{
    lv_obj_t *label = eos_event_get_obj(e);
    if (!label || !lv_obj_is_valid(label))
    {
        return;
    }
    _control_center_battery_update_display(label);
}

static lv_obj_t *_control_center_create_battery(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 150, 100);
    lv_obj_set_style_radius(btn, 50, 0);
    lv_obj_set_style_bg_color(btn, _BTN_DEFAULT_COLOR, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_center(label);

    _control_center_battery_update_display(label);

    eos_event_code_t event_id = eos_battery_get_event_id();
    eos_event_subscribe_ex(event_id, _control_center_battery_level_update_cb, NULL, label);

    return btn;
}

static void _control_center_phone_find_cb(lv_event_t *e)
{
    eos_locate_phone();
}

static char *_control_center_volume_get_icon_by_value(uint8_t value)
{
    if (value > 60)
    {
        return RI_VOLUME_LOUD_FILL;
    }
    else if (value > 30)
    {
        return RI_VOLUME_UP_FILL;
    }
    else if (value > 0)
    {
        return RI_VOLUME_DOWN_FILL;
    }
    else
    {
        return RI_VOLUME_MUTE_FILL;
    }
}

static void _control_center_volume_value_changed_cb(lv_event_t *e)
{
    eos_control_center_t *cc = (eos_control_center_t *)lv_event_get_user_data(e);
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t *)eos_wdata_get(slider, EOS_WDATA_SLIDER_LABEL);
    EOS_CHECK_PTR_RETURN(label);
    // Get current Slider value
    int16_t value = lv_slider_get_value(slider);
    eos_service_audio_set_volume(value);
    lv_label_set_text(label, _control_center_volume_get_icon_by_value(value));
    if (lv_obj_has_state(cc->mute_btn, LV_STATE_CHECKED))
    {
        lv_obj_remove_state(cc->mute_btn, LV_STATE_CHECKED);
        eos_settings_slient_mode_off();
    }
}

static void _control_center_volume_slider_delete_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    eos_config_set_number(EOS_CONFIG_KEY_SPEAKER_VOLUME_NUMBER, lv_slider_get_value(slider));
}

static void _control_center_volume_btn_clicked_cb(lv_event_t *e)
{
    eos_control_center_t *cc = (eos_control_center_t *)lv_event_get_user_data(e);
    uint8_t volume = eos_config_get_number(EOS_CONFIG_KEY_SPEAKER_VOLUME_NUMBER, 50);

    lv_obj_t *slider = _control_center_slider_create(_control_center_volume_get_icon_by_value(volume));
    lv_slider_set_range(slider, EOS_SPEAKER_VOLUME_MIN, EOS_SPEAKER_VOLUME_MAX);

    lv_slider_set_value(slider, volume, LV_ANIM_ON);
    lv_obj_add_event_cb(slider, _control_center_volume_value_changed_cb, LV_EVENT_VALUE_CHANGED, cc);
    lv_obj_add_event_cb(slider, _control_center_volume_slider_delete_cb, LV_EVENT_DELETE, NULL);
}

static void _control_center_flash_light_btn_clicked_cb(lv_event_t *e)
{
    eos_flash_light_show();
}

static void _control_center_mute_switch_btn_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);

    if (lv_obj_has_state(btn, LV_STATE_CHECKED))
    {
        eos_settings_slient_mode_on();
    }
    else
    {
        eos_settings_slient_mode_off();
    }
}

static void _control_center_settings_entry_cb(lv_event_t *e)
{
    eos_settings_enter();
}

static void _control_center_opened_cb(lv_event_t *e)
{
    EOS_LOG_I("Control center opened");
    eos_chrome_manager_notify_overlay_opened(&_control_center_overlay);
}

static void _control_center_closed_cb(lv_event_t *e)
{
    EOS_LOG_I("Control center closed");
    eos_chrome_manager_notify_overlay_closed(&_control_center_overlay);

    /* On non-watchface pages the touch area should be hidden when the panel
     * closes, so it does not intercept touches meant for the app below.
     * The watchface is responsible for re-showing it via on_resume. */
    eos_activity_t *act = eos_activity_get_current();
    if (!act || eos_activity_get_type(act) != EOS_ACTIVITY_TYPE_WATCHFACE)
    {
        lv_obj_add_flag(eos_slide_widget_get_touch_obj(control_center_instance->swipe_panel->sw), LV_OBJ_FLAG_HIDDEN);
    }
}

static void _control_center_overlay_pull_back(void)
{
    eos_control_center_t *cc = eos_control_center_get_instance();
    if (cc && cc->swipe_panel)
    {
        eos_swipe_panel_pull_back(cc->swipe_panel);
    }
}

static void _control_center_overlay_hide(void)
{
    eos_control_center_hide();
}

static void _control_center_overlay_on_focus(void)
{
    EOS_LOG_D("Control center focused");
}

static bool _control_center_overlay_is_open(void)
{
    eos_control_center_t *cc = eos_control_center_get_instance();
    if (cc && cc->swipe_panel && cc->swipe_panel->sw)
    {
        return eos_slide_widget_get_state(cc->swipe_panel->sw) == EOS_SLIDE_WIDGET_STATE_OPEN;
    }
    return false;
}

static lv_obj_t *_control_center_overlay_get_scrollable(void)
{
    eos_control_center_t *cc = eos_control_center_get_instance();
    if (cc && cc->container)
    {
        return cc->container;
    }
    return NULL;
}

static lv_obj_t *_control_center_overlay_get_foreground_obj(void)
{
    eos_control_center_t *cc = eos_control_center_get_instance();
    if (cc && cc->swipe_panel && cc->swipe_panel->sw)
    {
        return eos_slide_widget_get_touch_obj(cc->swipe_panel->sw);
    }
    return NULL;
}

/* Control center ---------------------------------------------*/

eos_control_center_t *eos_control_center_create(lv_obj_t *parent)
{
    eos_control_center_t *cc = eos_malloc_zeroed(sizeof(eos_control_center_t));
    EOS_CHECK_PTR_RETURN_VAL(cc, NULL);

    eos_swipe_panel_t *swipe_panel = eos_swipe_panel_create(parent);
    eos_swipe_panel_set_dir(swipe_panel, EOS_SWIPE_DIR_UP);
    eos_crown_encoder_register_slide_widget(swipe_panel->sw);
    eos_swipe_panel_show_handle_bar(swipe_panel);

    eos_slide_widget_add_event_cb_opened(swipe_panel->sw, _control_center_opened_cb, NULL);
    eos_slide_widget_add_event_cb_closed(swipe_panel->sw, _control_center_closed_cb, NULL);

    cc->swipe_panel = swipe_panel;

    lv_obj_t *container = lv_list_create(swipe_panel->swipe_obj);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(88));
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 30, 0);
    lv_obj_align(container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_scroll_dir(container, LV_DIR_VER);
    lv_obj_set_style_pad_column(container, 20, 0); // Column spacing
    lv_obj_set_style_pad_row(container, 8, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(
        container,
        LV_FLEX_ALIGN_CENTER, // Main axis (horizontal direction) starts from the beginning (left-aligned)
        LV_FLEX_ALIGN_CENTER, // Cross axis (vertical direction) centered
        LV_FLEX_ALIGN_START);
    lv_obj_add_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(container, _list_scroll_cb, LV_EVENT_SCROLL, container);
    eos_slide_widget_add_event_cb_moving(swipe_panel->sw, _list_scroll_cb, container);
    eos_slide_widget_add_event_cb_done(swipe_panel->sw, _list_scroll_cb, container);
    eos_slide_widget_add_event_cb_reached_threshold(swipe_panel->sw, _slide_widget_reached_threshold_cb, container);
    cc->container = container;
    lv_obj_t *btn;
    /* Bluetooth switch -------------------------------------------*/
    btn = _control_center_create_switch_btn(container, RI_BLUETOOTH_FILL, EOS_COLOR_BLUE);
    lv_obj_add_event_cb(btn, _control_center_bluetooth_switch_btn_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (eos_config_get_bool(EOS_CONFIG_KEY_BLUETOOTH_BOOL, false))
    {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
    }
    else
    {
        lv_obj_remove_state(btn, LV_STATE_CHECKED);
    }
    cc->bl_btn = btn;
    /* Brightness adjustment scrollbar ----------------------------*/
    btn = _control_center_create_btn(container, RI_SUN_FILL);
    lv_obj_add_event_cb(btn, _control_center_brightness_btn_clicked_cb, LV_EVENT_CLICKED, 0);
    cc->brightness_btn = btn;
    /* Battery display --------------------------------------------*/
    btn = _control_center_create_battery(container);
    cc->bat_btn = btn;
    /* Find phone -------------------------------------------------*/
    btn = _control_center_create_btn(container, RI_PHONE_FIND_FILL);
    lv_obj_add_event_cb(btn, _control_center_phone_find_cb, LV_EVENT_CLICKED, 0);
    cc->locate_phone_btn = btn;
    /* Mute -------------------------------------------------------*/
    btn = _control_center_create_switch_btn(container, RI_NOTIFICATION_4_FILL, EOS_COLOR_ORANGE);
    lv_obj_add_event_cb(btn, _control_center_mute_switch_btn_cb, LV_EVENT_CLICKED, 0);
    if (eos_config_get_bool(EOS_CONFIG_KEY_MUTE_BOOL, false))
    {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
    }
    else
    {
        lv_obj_remove_state(btn, LV_STATE_CHECKED);
    }
    cc->mute_btn = btn;
    /* Volume adjustment scrollbar --------------------------------*/
    btn = _control_center_create_btn(container, RI_VOLUME_UP_FILL);
    lv_obj_add_event_cb(btn, _control_center_volume_btn_clicked_cb, LV_EVENT_CLICKED, cc);
    cc->volume_btn = btn;
    /* Flashlight -------------------------------------------------*/
    btn = _control_center_create_btn(container, RI_FLASH_LIGHT);
    lv_obj_add_event_cb(btn, _control_center_flash_light_btn_clicked_cb, LV_EVENT_CLICKED, 0);
    cc->flash_light_btn = btn;
    /* Settings entry ---------------------------------------------*/
    btn = _control_center_create_btn(container, RI_SETTINGS_4_FILL);
    lv_obj_add_event_cb(btn, _control_center_settings_entry_cb, LV_EVENT_CLICKED, 0);
    cc->settings_btn = btn;
    return cc;
}

void eos_control_panel_slide_change(void)
{
    EOS_CHECK_PTR_RETURN(control_center_instance);
    eos_swipe_panel_t *sp = control_center_instance->swipe_panel;
    if (eos_slide_widget_get_state(sp->sw) == EOS_SLIDE_WIDGET_STATE_OPEN)
    {
        eos_swipe_panel_pull_back(sp);
    }
    else
    {
        eos_swipe_panel_slide_down(sp);
    }
}

void eos_control_center_show(void)
{
    EOS_CHECK_PTR_RETURN(control_center_instance);
    lv_obj_remove_flag(eos_slide_widget_get_touch_obj(control_center_instance->swipe_panel->sw), LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(control_center_instance->swipe_panel->swipe_obj, LV_OBJ_FLAG_HIDDEN);
    eos_crown_encoder_activate_current_overlay_scrollable();
}

void eos_control_center_hide(void)
{
    EOS_CHECK_PTR_RETURN(control_center_instance);
    lv_obj_add_flag(eos_slide_widget_get_touch_obj(control_center_instance->swipe_panel->sw), LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_get_y(control_center_instance->swipe_panel->swipe_obj) < EOS_DISPLAY_HEIGHT)
        lv_obj_add_flag(control_center_instance->swipe_panel->swipe_obj, LV_OBJ_FLAG_HIDDEN);
    eos_crown_encoder_activate_current_overlay_scrollable();
}

eos_control_center_t *eos_control_center_get_instance(void)
{
    return control_center_instance;
}

static void _system_config_update_event_cb(eos_event_t *e)
{
    LV_UNUSED(e);
    EOS_CHECK_PTR_RETURN(control_center_instance);

    // Update Bluetooth switch state
    if (eos_config_get_bool(EOS_CONFIG_KEY_BLUETOOTH_BOOL, false))
    {
        lv_obj_add_state(control_center_instance->bl_btn, LV_STATE_CHECKED);
    }
    else
    {
        lv_obj_remove_state(control_center_instance->bl_btn, LV_STATE_CHECKED);
    }

    // Update mute switch state
    if (eos_config_get_bool(EOS_CONFIG_KEY_MUTE_BOOL, false))
    {
        lv_obj_add_state(control_center_instance->mute_btn, LV_STATE_CHECKED);
    }
    else
    {
        lv_obj_remove_state(control_center_instance->mute_btn, LV_STATE_CHECKED);
    }

    EOS_LOG_D("Switch buttons updated");
}

void eos_control_center_init(void)
{
    control_center_instance = eos_control_center_create(eos_overlay_get_overlay_layer());
    eos_event_subscribe(EOS_EVENT_SYSTEM_CONFIG_UPDATE, _system_config_update_event_cb, NULL);
}

const eos_chrome_overlay_t *eos_control_center_get_overlay_descriptor(void)
{
    return &_control_center_overlay;
}
