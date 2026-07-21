/**
 * @file eos_toast.c
 * @brief Temporary message toast
 */

#include "eos_toast.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "Toast"
#include "eos_log.h"
#include "eos_theme.h"
#include "eos_image.h"
#include "eos_config.h"
#include "eos_anim.h"
#include "eos_cqueue.h"
#include "eos_widget_data.h"

/* Macros and Definitions -------------------------------------*/
#define _TOAST_PAD_ALL 12

#define _ICON_WIDTH 44
#define _ICON_HEIGHT _ICON_WIDTH

#define _LABEL_MARGIN_LEFT 12
#define _SCREEN_PAD_ALL 10
#define _LABEL_MAX_WIDTH EOS_DISPLAY_WIDTH - _ICON_WIDTH - _TOAST_PAD_ALL * 2 - _LABEL_MARGIN_LEFT - _SCREEN_PAD_ALL * 2

#define _TOAST_ANIM_DURATION 500
#define _TOAST_MARGIN_TOP 35
#define _TOAST_SHOW_SCROLL_SPEED 10 /**< Scroll time per pixel, in milliseconds */
#define _TOAST_SHOW_DURATION 3000 /**< Base display time, in milliseconds */
#define _TOAST_SHOW_ANIM_BASE_DURATION 1000 /**< This duration is for the time after scrolling ends */

#define _TOAST_LABEL_MAX_LENGTH 128
/* Variables --------------------------------------------------*/
static eos_cqueue_t *anim_cq = NULL;
static bool is_toast_playing = false;
/* Function Implementations -----------------------------------*/
static void _play_move_anim(lv_obj_t *toast);
static lv_obj_t *_toast_create_container(void);
static lv_obj_t *_toast_finalize(lv_obj_t *toast, const char *message);

static void _anim_move_end_cb(lv_anim_t *a)
{
    lv_obj_t *label = (lv_obj_t *)lv_anim_get_user_data(a);
    if (label && lv_obj_is_valid(label))
    {
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    }
}

static void _toast_move_back_completed_cb(eos_anim_t *a)
{
    LV_UNUSED(a);
    // Start next animation
    if (eos_cqueue_get_size(anim_cq) > 0)
    {
        lv_obj_t *next_toast = eos_cqueue_dequeue(anim_cq);
        if (next_toast && lv_obj_is_valid(next_toast))
            _play_move_anim(next_toast);
    }
    else
    {
        is_toast_playing = false;
    }
}

static void _toast_start_move_back_cb(eos_anim_t *a)
{
    lv_obj_t *toast = a->tar_obj;
    if (toast && lv_obj_is_valid(toast))
    {
        uint32_t duration = (uint32_t)(intptr_t)eos_wdata_get(toast, EOS_WDATA_TOAST_DURATION);
        int32_t toast_x = lv_obj_get_x(toast);
        eos_anim_t *anim = eos_anim_move_create(toast,
                                                toast_x,
                                                _TOAST_MARGIN_TOP,
                                                toast_x,
                                                -lv_obj_get_height(toast),
                                                _TOAST_ANIM_DURATION,
                                                false);
        if (!anim)
            return;
        eos_anim_set_delay(anim, duration);
        eos_anim_add_cb(anim, _toast_move_back_completed_cb, NULL);
        eos_anim_start(anim);
    }
}

static void _play_move_anim(lv_obj_t *toast)
{
    if (!(toast && lv_obj_is_valid(toast)))
        return;
    // Animate into screen
    int32_t toast_x = lv_obj_get_x(toast);
    eos_anim_t *anim = eos_anim_move_create(toast,
                                            toast_x,
                                            -lv_obj_get_height(toast),
                                            toast_x,
                                            _TOAST_MARGIN_TOP,
                                            _TOAST_ANIM_DURATION,
                                            false);
    if (!anim)
        return;
    eos_anim_add_cb(anim, _toast_start_move_back_cb, NULL);
    eos_anim_start(anim);
}

static lv_obj_t *_toast_create_container(void)
{
    lv_obj_t *toast = lv_button_create(lv_layer_sys());
    lv_obj_set_size(toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(toast, EOS_COLOR_DARK_GREY_2, 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(toast, _TOAST_PAD_ALL, 0);
    lv_obj_set_style_radius(toast, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_flex_flow(toast, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(toast, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(toast, 8, 0);
    return toast;
}

static lv_obj_t *_toast_finalize(lv_obj_t *toast, const char *message)
{
    // Create mask
    lv_obj_t *mask = lv_obj_create(toast);
    lv_obj_remove_style_all(mask);
    lv_obj_set_style_clip_corner(mask, true, 0);
    lv_obj_set_size(mask, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(mask, LV_OPA_TRANSP, 0);
    lv_obj_set_style_translate_y(mask, 2, 0);
    lv_obj_set_style_margin_right(mask, 5, 0);

    // Create label
    lv_obj_t *label = lv_label_create(mask);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, EOS_COLOR_WHITE, 0);

    // Measure text
    lv_point_t size;
    lv_text_get_size(&size, message, lv_obj_get_style_text_font(label, 0), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_coord_t text_width = LV_MIN(size.x, _LABEL_MAX_WIDTH);

    lv_obj_add_flag(mask, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_set_width(label, text_width);
    lv_label_set_text(label, message);

    // Set scroll animation duration
    bool need_scroll = size.x > _LABEL_MAX_WIDTH;
    if (need_scroll)
    {
        uint32_t scroll_time = size.x * _TOAST_SHOW_SCROLL_SPEED;
        lv_obj_set_style_anim_duration(label, scroll_time, 0);
        lv_obj_set_style_radius(mask, LV_RADIUS_CIRCLE, 0);
    }

    // Top center
    lv_obj_align(toast, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_update_layout(toast);
    lv_obj_set_style_translate_y(toast, -lv_obj_get_height(toast), 0);

    // Calculate timer duration
    uint32_t duration = 0;
    if (need_scroll)
    {
        // Get label scroll animation duration
        uint32_t anim_dur = lv_obj_get_style_anim_duration(label, 0);
        duration = _TOAST_SHOW_ANIM_BASE_DURATION + anim_dur; // Add stay time after scrolling
    }
    else
    {
        duration = _TOAST_SHOW_DURATION;
    }
    EOS_LOG_D("Toast will show for %d ms", duration);
    eos_wdata_set(toast, EOS_WDATA_TOAST_DURATION, (void *)(intptr_t)duration, NULL);
    if (is_toast_playing)
    {
        if (!eos_cqueue_enqueue(anim_cq, toast))
            EOS_LOG_E("Toast enqueue failed");
    }
    else
    {
        is_toast_playing = true;
        _play_move_anim(toast);
    }

    return toast;
}

lv_obj_t *eos_toast_show(const char *icon_src, const char *message)
{
    lv_obj_t *toast = _toast_create_container();

    // Create icon
    lv_obj_t *icon = lv_image_create(toast);
    lv_image_set_src(icon, icon_src);
    eos_img_set_size(icon, _ICON_WIDTH, _ICON_HEIGHT);

    lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);
    return _toast_finalize(toast, message);
}

lv_obj_t *eos_toast_show_char_icon(const char *icon_char, lv_color_t icon_color, const char *message)
{
    lv_obj_t *toast = _toast_create_container();

    if (icon_char)
    {
        lv_obj_t *icon_wrap = lv_obj_create(toast);
        lv_obj_remove_style_all(icon_wrap);
        lv_obj_set_size(icon_wrap, _ICON_WIDTH, _ICON_HEIGHT);

        lv_obj_t *icon = lv_label_create(icon_wrap);
        lv_label_set_text(icon, icon_char);
        lv_obj_set_style_text_color(icon, icon_color, 0);
        lv_obj_set_style_translate_y(icon, 2, 0);
        lv_obj_center(icon);

        lv_obj_add_flag(icon_wrap, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    return _toast_finalize(toast, message);
}

lv_obj_t *eos_toast_show_fmt(const char *icon_src, const char *fmt, ...)
{
    char buf[_TOAST_LABEL_MAX_LENGTH];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    return eos_toast_show(icon_src, buf);
}

void eos_toast_init(void)
{
    anim_cq = eos_cqueue_create(4);
}
