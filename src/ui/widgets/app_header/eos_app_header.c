/**
 * @file eos_app_header.c
 * @brief Application top navigation header
 */

#include "eos_app_header.h"
#include "eos_config.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "AppHeader"
#include "eos_log.h"
#include "eos_core.h"
#include "eos_port.h"
#include "eos_lang.h"
#include "eos_image.h"
#include "eos_theme.h"
#include "eos_font.h"
#include "eos_basic_widgets.h"
#include "eos_mem.h"
#include "eos_activity.h"
#include "eos_service_time.h"
#include "eos_event.h"
#include "eos_overlay_layer.h"
#include "eos_service_cache.h"
#include "eos_anim.h"

/* Macros and Definitions -------------------------------------*/
#define _HEADER_HEIGHT 120
#define _HEADER_CLOCK_UPDATE_PERIOD_MINUTES 1 /**< Clock label text update interval in minutes */

#define _HEADER_MARGIN_RIGHT 30

#define _HEADER_TITLE_WIDTH 240

#define _TITLE_LABEL_Y_OFFSET 20
#define _TITLE_LABEL_X_OFFSET -_HEADER_MARGIN_RIGHT
#define _ANIM_DURATION EOS_VIEW_SWITCH_DURATION

#define _BACK_BTN_MARGIN_LEFT 20

#define _ANIM_TITLE_MOVE_DISTANCE 50
#define _ANIM_BACK_BTN_MOVE_DISTANCE _ANIM_TITLE_MOVE_DISTANCE

/**
 * @brief Application top header structure definition
 */
typedef struct
{
    lv_obj_t *container;
    lv_obj_t *clock_label;
    lv_obj_t *title_label;
    lv_obj_t *back_btn;
    lv_timer_t *clock_timer;
    lv_obj_t *original_parent; // Original parent object for restoration
    lv_obj_t *old_fading_title; // Old title label pending cleanup
    lv_obj_t *old_fading_back_btn; // Old back button pending cleanup
    bool is_anim_entering; // Animation direction
    bool attached_to_view; // Whether attached to View
    lv_image_dsc_t *grad_bg_img; // Pre-rendered gradient background (ARGB8888 full-size)
} eos_app_header_t;

/* Variables --------------------------------------------------*/
static eos_app_header_t *app_header = NULL;
/* Function Implementations -----------------------------------*/
static void _clock_update_cb(lv_timer_t *timer);

static void _app_header_fade_out_ready_cb(eos_anim_t *a)
{
    lv_obj_t *obj = a->tar_obj;
    if (!obj || !lv_obj_is_valid(obj))
        return;

    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
}

static void _app_header_apply_activity_mode(eos_activity_t *activity)
{
    EOS_CHECK_PTR_RETURN(app_header);

    bool time_only = false;
    lv_color_t clock_color = EOS_COLOR_WHITE;
    if (activity)
    {
        time_only = eos_activity_is_app_header_time_only(activity);
        if (time_only)
        {
            clock_color = eos_activity_get_app_header_time_only_text_color(activity);
        }
    }

    if (app_header->container && lv_obj_is_valid(app_header->container))
    {
        lv_opa_t bg_opa = time_only ? LV_OPA_TRANSP : LV_OPA_COVER;
        lv_obj_set_style_bg_image_opa(app_header->container, bg_opa, 0);
    }

    if (app_header->clock_label && lv_obj_is_valid(app_header->clock_label))
    {
        lv_obj_set_style_text_color(app_header->clock_label, clock_color, 0);
        lv_obj_remove_flag(app_header->clock_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (app_header->title_label && lv_obj_is_valid(app_header->title_label))
    {
        if (time_only)
            lv_obj_add_flag(app_header->title_label, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_remove_flag(app_header->title_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (app_header->back_btn && lv_obj_is_valid(app_header->back_btn))
    {
        if (time_only)
            lv_obj_add_flag(app_header->back_btn, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_remove_flag(app_header->back_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static bool _app_header_can_reparent(lv_obj_t *new_parent)
{
    if (!app_header)
    {
        return false;
    }

    if (!app_header->container || !lv_obj_is_valid(app_header->container))
    {
        app_header->attached_to_view = false;
        EOS_LOG_E("AppHeader container is invalid");
        return false;
    }

    if (!new_parent || !lv_obj_is_valid(new_parent))
    {
        EOS_LOG_E("AppHeader target parent is invalid");
        return false;
    }

    return true;
}

static void _set_title_style(lv_obj_t *label)
{
    lv_obj_add_style(label, eos_theme_get_label_style(), 0);
    lv_obj_set_width(label, _HEADER_TITLE_WIDTH);
    lv_label_set_text(label, "");
    eos_label_set_font_size(label, EOS_FONT_SIZE_LARGE);

    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(label, EOS_THEME_PRIMARY_COLOR, 0);
    lv_obj_align(label, LV_ALIGN_RIGHT_MID, _TITLE_LABEL_X_OFFSET, _TITLE_LABEL_Y_OFFSET);
}

static void _set_back_btn_style(lv_obj_t *btn)
{
    lv_obj_set_size(btn, 64, 64);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, EOS_THEME_SECONDARY_COLOR, 0);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, _BACK_BTN_MARGIN_LEFT, 0);
}

static void _ah_set_translate_x_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0);
}

static void _ah_set_opa_layered_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa_layered((lv_obj_t *)var, (lv_opa_t)v, 0);
}

void _play_title_changed_anim(eos_activity_t *from,
                              eos_activity_t *to,
                              bool need_anim,
                              bool reverse_anim,
                              lv_anim_timeline_t *at)
{
    EOS_CHECK_PTR_RETURN(app_header);
    if (!(lv_obj_is_valid(app_header->title_label) && lv_obj_has_class(app_header->title_label, &lv_label_class)))
        return;

    bool from_time_only = from ? eos_activity_is_app_header_time_only(from) : false;
    bool to_time_only = to ? eos_activity_is_app_header_time_only(to) : false;

    if (from_time_only || to_time_only)
    {
        need_anim = false;
    }

    if (!need_anim || !at)
    {
        const char *new_title = eos_activity_get_title(to);
        EOS_LOG_D("New title: %s", new_title);
        lv_label_set_text(app_header->title_label, new_title ? new_title : "");

        lv_color_t color = eos_activity_get_title_color(to);
        lv_obj_set_style_text_color(app_header->title_label, color, 0);
        _app_header_apply_activity_mode(to);
        return;
    }

    if (app_header->old_fading_title && lv_obj_is_valid(app_header->old_fading_title))
    {
        lv_obj_delete(app_header->old_fading_title);
    }
    if (app_header->old_fading_back_btn && lv_obj_is_valid(app_header->old_fading_back_btn))
    {
        lv_obj_delete(app_header->old_fading_back_btn);
    }
    app_header->old_fading_title = NULL;
    app_header->old_fading_back_btn = NULL;

    if (reverse_anim)
    {
        app_header->is_anim_entering = false;
    }
    else
    {
        app_header->is_anim_entering = true;
    }

    lv_obj_t *l = app_header->title_label;
    lv_obj_t *back_btn = app_header->back_btn;
    lv_obj_t *parent = lv_obj_get_parent(l);

    int32_t title_start_x = 0;
    int32_t title_end_x;
    int32_t back_btn_start_x = 0;
    int32_t back_btn_end_x;

    if (app_header->is_anim_entering)
    {
        title_end_x = title_start_x - _ANIM_TITLE_MOVE_DISTANCE;
        back_btn_end_x = back_btn_start_x - _ANIM_BACK_BTN_MOVE_DISTANCE;
    }
    else
    {
        title_end_x = title_start_x + _ANIM_TITLE_MOVE_DISTANCE;
        back_btn_end_x = back_btn_start_x + _ANIM_BACK_BTN_MOVE_DISTANCE;
    }

    lv_anim_t a;

    lv_anim_init(&a);
    lv_anim_set_var(&a, l);
    lv_anim_set_values(&a, title_start_x, title_end_x);
    lv_anim_set_exec_cb(&a, _ah_set_translate_x_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_duration(&a, _ANIM_DURATION);
    lv_anim_timeline_add(at, 0, &a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, l);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_exec_cb(&a, _ah_set_opa_layered_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_duration(&a, _ANIM_DURATION + 1);
    lv_anim_timeline_add(at, 0, &a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, back_btn);
    lv_anim_set_values(&a, back_btn_start_x, back_btn_end_x);
    lv_anim_set_exec_cb(&a, _ah_set_translate_x_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_duration(&a, _ANIM_DURATION);
    lv_anim_timeline_add(at, 0, &a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, back_btn);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_exec_cb(&a, _ah_set_opa_layered_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_duration(&a, _ANIM_DURATION + 1);
    lv_anim_timeline_add(at, 0, &a);

    app_header->old_fading_title = l;
    app_header->old_fading_back_btn = back_btn;

    lv_obj_t *new_l = lv_label_create(parent);
    _set_title_style(new_l);

    const char *new_title = eos_activity_get_title(to);
    EOS_LOG_D("New title: %s", new_title);
    lv_label_set_text(new_l, new_title ? new_title : "");

    lv_color_t color = eos_activity_get_title_color(to);
    lv_obj_set_style_text_color(new_l, color, 0);

    lv_obj_t *new_back_btn = eos_back_btn_create(parent, false);
    _set_back_btn_style(new_back_btn);

    int32_t new_title_start_x, new_title_end_x = 0;
    int32_t new_back_btn_start_x, new_back_btn_end_x = 0;

    if (app_header->is_anim_entering)
    {
        new_title_start_x = new_title_end_x + _ANIM_TITLE_MOVE_DISTANCE;
        new_back_btn_start_x = new_back_btn_end_x + _ANIM_BACK_BTN_MOVE_DISTANCE;
    }
    else
    {
        new_title_start_x = new_title_end_x - _ANIM_TITLE_MOVE_DISTANCE;
        new_back_btn_start_x = new_back_btn_end_x - _ANIM_BACK_BTN_MOVE_DISTANCE;
    }

    lv_obj_set_style_translate_x(new_l, new_title_start_x, 0);
    lv_obj_set_style_opa_layered(new_l, LV_OPA_TRANSP, 0);
    lv_obj_set_style_translate_x(new_back_btn, new_back_btn_start_x, 0);
    lv_obj_set_style_opa_layered(new_back_btn, LV_OPA_TRANSP, 0);

    lv_anim_init(&a);
    lv_anim_set_var(&a, new_l);
    lv_anim_set_values(&a, new_title_start_x, new_title_end_x);
    lv_anim_set_exec_cb(&a, _ah_set_translate_x_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_duration(&a, _ANIM_DURATION);
    lv_anim_timeline_add(at, 0, &a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, new_l);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a, _ah_set_opa_layered_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_duration(&a, _ANIM_DURATION);
    lv_anim_timeline_add(at, 0, &a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, new_back_btn);
    lv_anim_set_values(&a, new_back_btn_start_x, new_back_btn_end_x);
    lv_anim_set_exec_cb(&a, _ah_set_translate_x_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_duration(&a, _ANIM_DURATION);
    lv_anim_timeline_add(at, 0, &a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, new_back_btn);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a, _ah_set_opa_layered_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_duration(&a, _ANIM_DURATION);
    lv_anim_timeline_add(at, 0, &a);

    app_header->title_label = new_l;
    app_header->back_btn = new_back_btn;
}

/**
 * @brief Update LVGL string to display current time
 */
static inline void _app_header_update_clock_label(lv_obj_t *label)
{
    eos_datetime_t dt = eos_time_get();
    uint32_t next_ms = (uint32_t)((_HEADER_CLOCK_UPDATE_PERIOD_MINUTES * 60) - dt.sec) * 1000;
    lv_timer_set_period(app_header->clock_timer, next_ms);
    lv_label_set_text_fmt(label, "%02d:%02d", dt.hour, dt.min);
}

/**
 * @brief Time refresh callback, triggered by LVGL timer
 */
static void _clock_update_cb(lv_timer_t *timer)
{
    lv_obj_t *label = lv_timer_get_user_data(timer);
    EOS_CHECK_PTR_RETURN(app_header && label);
    if (lv_obj_has_flag(app_header->container, LV_OBJ_FLAG_HIDDEN))
    {
        return;
    }
    // Update display text
    _app_header_update_clock_label(label);
}

void eos_app_header_hide(void)
{
    EOS_CHECK_PTR_RETURN(app_header);
    EOS_LOG_D("Hide app header");
    // If attached to a View, restore the parent object first
    if (app_header->attached_to_view)
    {
        lv_obj_t *restore_parent = app_header->original_parent;
        if (!restore_parent || !lv_obj_is_valid(restore_parent))
        {
            restore_parent = eos_overlay_get_header_layer();
            app_header->original_parent = restore_parent;
        }

        if (_app_header_can_reparent(restore_parent))
        {
            lv_obj_set_parent(app_header->container, restore_parent);
        }
        app_header->attached_to_view = false;
    }

    if (app_header->container && lv_obj_is_valid(app_header->container))
    {
        lv_obj_add_flag(app_header->container, LV_OBJ_FLAG_HIDDEN);
    }
}

void eos_app_header_show(eos_activity_t *a)
{
    EOS_CHECK_PTR_RETURN(app_header);

    // Check if it is a Watchface Activity; if so, hide AppHeader
    eos_activity_t *target_activity = a;
    if (!target_activity)
        target_activity = eos_activity_get_current();

    if (target_activity && target_activity == eos_activity_get_watchface())
    {
        EOS_LOG_D("Skip showing app header for watchface activity");
        return;
    }

    EOS_LOG_D("Show app header");
    if (!(app_header->container && lv_obj_is_valid(app_header->container)))
    {
        app_header->attached_to_view = false;
        EOS_LOG_E("AppHeader container is invalid");
        return;
    }

    if (app_header->attached_to_view)
    {
        lv_obj_t *restore_parent = app_header->original_parent;
        if (!restore_parent || !lv_obj_is_valid(restore_parent))
        {
            restore_parent = eos_overlay_get_header_layer();
            app_header->original_parent = restore_parent;
        }

        if (_app_header_can_reparent(restore_parent))
        {
            lv_obj_set_parent(app_header->container, restore_parent);
        }
        app_header->attached_to_view = false;
    }
    // Get title text from current Activity
    const char *title = NULL;
    if (a)
        title = eos_activity_get_title(a);
    else
        title = eos_activity_get_title(eos_activity_get_current());
    if (title)
        lv_label_set_text(app_header->title_label, title);
    else
        lv_label_set_text(app_header->title_label, "");
    // Get title color from current Activity
    lv_color_t color = EOS_COLOR_WHITE;
    if (a)
        color = eos_activity_get_title_color(a);
    else
        color = eos_activity_get_title_color(eos_activity_get_current());
    if (lv_obj_is_valid(app_header->title_label))
        lv_obj_set_style_text_color(app_header->title_label, color, 0);

    _app_header_apply_activity_mode(target_activity);
    _app_header_update_clock_label(app_header->clock_label);
    lv_obj_remove_flag(app_header->container, LV_OBJ_FLAG_HIDDEN);
}

void eos_app_header_set_visible_animated(eos_activity_t *a, bool visible, uint32_t duration_ms)
{
    EOS_CHECK_PTR_RETURN(app_header);

    if (!(app_header->container && lv_obj_is_valid(app_header->container)))
        return;

    if (duration_ms == 0)
    {
        if (visible)
            eos_app_header_show(a);
        else
            eos_app_header_hide();
        return;
    }

    lv_obj_t *container = app_header->container;
    eos_anim_t *anim = eos_anim_fade_create(container,
                                            visible ? LV_OPA_TRANSP : LV_OPA_COVER,
                                            visible ? LV_OPA_COVER : LV_OPA_TRANSP,
                                            duration_ms,
                                            false);
    if (!anim)
        return;

    eos_anim_fade_set_main_opa(anim, true);

    if (visible)
    {
        eos_app_header_show(a);
        lv_obj_set_style_opa(container, LV_OPA_TRANSP, 0);
        eos_anim_start(anim);
    }
    else
    {
        if (lv_obj_has_flag(container, LV_OBJ_FLAG_HIDDEN))
        {
            eos_anim_del(anim);
            return;
        }
        lv_obj_set_style_opa(container, LV_OPA_COVER, 0);
        eos_anim_add_cb(anim, _app_header_fade_out_ready_cb, NULL);
        eos_anim_start(anim);
    }
}

static void _app_header_slide_hide_ready_cb(eos_anim_t *a)
{
    lv_obj_t *container = a->tar_obj;
    if (!container || !lv_obj_is_valid(container))
    {
        return;
    }

    lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_translate_y(container, 0, 0);
}

void eos_app_header_slide_visible_animated(eos_activity_t *a, bool visible, uint32_t duration_ms)
{
    EOS_CHECK_PTR_RETURN(app_header);

    if (!(app_header->container && lv_obj_is_valid(app_header->container)))
    {
        return;
    }

    if (duration_ms == 0)
    {
        if (visible)
        {
            eos_app_header_show(a);
        }
        else
        {
            eos_app_header_hide();
        }
        return;
    }

    lv_obj_t *container = app_header->container;

    int32_t header_height = lv_obj_get_height(container);
    if (header_height <= 0)
    {
        header_height = _HEADER_HEIGHT;
    }

    int32_t container_x = lv_obj_get_x(container);
    eos_anim_t *anim = eos_anim_move_create(container,
                                            container_x,
                                            visible ? -header_height : 0,
                                            container_x,
                                            visible ? 0 : -header_height,
                                            duration_ms,
                                            false);
    if (!anim)
        return;

    if (visible)
    {
        eos_app_header_show(a);
        lv_obj_remove_flag(container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_translate_y(container, -header_height, 0);
        eos_anim_start(anim);
    }
    else
    {
        if (lv_obj_has_flag(container, LV_OBJ_FLAG_HIDDEN))
        {
            eos_anim_del(anim);
            return;
        }
        lv_obj_set_style_translate_y(container, 0, 0);
        eos_anim_add_cb(anim, _app_header_slide_hide_ready_cb, NULL);
        eos_anim_start(anim);
    }
}
/**
 * @brief Attach app header to specified View
 * @param view View to attach to
 */
void eos_app_header_attach_to_view(lv_obj_t *view)
{
    EOS_CHECK_PTR_RETURN(app_header && view);

    if (!_app_header_can_reparent(view))
    {
        app_header->attached_to_view = false;
        return;
    }

    if (app_header->attached_to_view)
    {
        lv_obj_t *cur_parent = lv_obj_get_parent(app_header->container);
        if (cur_parent == view)
        {
            return;
        }
    }

    if (!app_header->original_parent || !lv_obj_is_valid(app_header->original_parent))
    {
        app_header->original_parent = eos_overlay_get_header_layer();
    }

    lv_obj_set_parent(app_header->container, view);
    app_header->attached_to_view = true;
}

/**
 * @brief Detach app header from View, restore to original parent object
 */
void eos_app_header_detach_from_view(void)
{
    EOS_CHECK_PTR_RETURN(app_header);
    if (!app_header->attached_to_view)
    {
        return;
    }

    lv_obj_t *restore_parent = app_header->original_parent;
    if (!restore_parent || !lv_obj_is_valid(restore_parent))
    {
        restore_parent = eos_overlay_get_header_layer();
        app_header->original_parent = restore_parent;
    }

    if (_app_header_can_reparent(restore_parent))
    {
        lv_obj_set_parent(app_header->container, restore_parent);
    }
    app_header->attached_to_view = false;
}

bool eos_app_header_is_attached_to_view(void)
{
    EOS_CHECK_PTR_RETURN_VAL(app_header, false);
    return app_header->attached_to_view;
}

bool eos_app_header_is_visible(void)
{
    EOS_CHECK_PTR_RETURN_VAL(app_header, false);
    return !lv_obj_has_flag(app_header->container, LV_OBJ_FLAG_HIDDEN);
}

static void _update_title_label(lv_event_t *e)
{
    const char *title = eos_activity_get_title(eos_activity_get_current());
    if (title)
        lv_label_set_text(app_header->title_label, title);
    else
        lv_label_set_text(app_header->title_label, "");
}

static lv_image_dsc_t *_create_gradient_bg(void)
{
    const lv_coord_t img_w = EOS_DISPLAY_WIDTH;
    const lv_coord_t img_h = _HEADER_HEIGHT;
    const uint32_t cf_bytes = 4;
    const uint32_t buf_size = (uint32_t)img_w * img_h * cf_bytes;

    uint8_t *buf = eos_cache_buf_alloc(buf_size);
    if (!buf)
    {
        EOS_LOG_E("Failed to allocate gradient bg buffer");
        return NULL;
    }

    for (lv_coord_t y = 0; y < img_h; y++)
    {
        int frac = y * 255 / (img_h - 1);
        uint8_t opa;
        if (frac <= 125)
            opa = LV_OPA_90;
        else
            opa = LV_OPA_90 - (uint8_t)((uint32_t)LV_OPA_90 * (frac - 125) / (255 - 125));
        for (lv_coord_t x = 0; x < img_w; x++)
        {
            uint8_t *p = &buf[(y * img_w + x) * cf_bytes];
            p[0] = 0x00; // B
            p[1] = 0x00; // G
            p[2] = 0x00; // R
            p[3] = opa; // A
        }
    }

    lv_image_dsc_t *dsc = eos_malloc_zeroed(sizeof(lv_image_dsc_t));
    if (!dsc)
    {
        eos_cache_buf_free(buf);
        return NULL;
    }

    dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf = LV_COLOR_FORMAT_ARGB8888;
    dsc->header.w = img_w;
    dsc->header.h = img_h;
    dsc->header.stride = img_w * cf_bytes;
    dsc->data = buf;
    dsc->data_size = buf_size;

    return dsc;
}

static void _grad_bg_img_delete_cb(lv_event_t *e)
{
    lv_image_dsc_t *dsc = lv_event_get_user_data(e);
    if (dsc)
    {
        if (dsc->data)
            eos_cache_buf_free((void *)dsc->data);
        eos_free(dsc);
    }
}

void eos_app_header_init(void)
{
    EOS_LOG_D("Init eos_app_header");
    app_header = eos_malloc_zeroed(sizeof(eos_app_header_t));
    EOS_CHECK_PTR_RETURN_FREE(app_header, app_header);

    app_header->grad_bg_img = _create_gradient_bg();
    EOS_CHECK_PTR_RETURN_FREE(app_header->grad_bg_img, app_header);

    // Semi-transparent container
    app_header->container = lv_obj_create(eos_overlay_get_header_layer());
    app_header->original_parent = lv_obj_get_parent(app_header->container); // Save original parent object
    lv_obj_remove_style_all(app_header->container);
    lv_obj_set_size(app_header->container, EOS_DISPLAY_WIDTH, _HEADER_HEIGHT);
    lv_obj_align(app_header->container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_image_src(app_header->container, app_header->grad_bg_img, 0);
    lv_obj_set_style_bg_image_opa(app_header->container, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(app_header->container, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(app_header->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(app_header->container, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(app_header->container, _grad_bg_img_delete_cb, LV_EVENT_DELETE, app_header->grad_bg_img);

    lv_coord_t header_h = _HEADER_HEIGHT;
    lv_coord_t header_w = lv_obj_get_width(app_header->container);

    // Back button
    app_header->back_btn = eos_back_btn_create(app_header->container, false);
    _set_back_btn_style(app_header->back_btn);

    // Clock label
    app_header->clock_label = lv_label_create(app_header->container);
    lv_obj_add_style(app_header->clock_label, eos_theme_get_label_style(), 0);
    app_header->clock_timer =
        lv_timer_create(_clock_update_cb, _HEADER_CLOCK_UPDATE_PERIOD_MINUTES * 60 * 1000, app_header->clock_label);
    lv_timer_set_repeat_count(app_header->clock_timer, -1);
    _app_header_update_clock_label(app_header->clock_label);
    lv_obj_align(app_header->clock_label, LV_ALIGN_RIGHT_MID, -_HEADER_MARGIN_RIGHT, -20);

    // Title label
    app_header->title_label = lv_label_create(app_header->container);
    _set_title_style(app_header->title_label);
    lv_obj_add_event_cb(app_header->title_label, _update_title_label, LV_EVENT_REFRESH, NULL);

    // Hide app_header by default
    lv_obj_add_flag(app_header->container, LV_OBJ_FLAG_HIDDEN);

    app_header->is_anim_entering = false;
    app_header->attached_to_view = false;
}
