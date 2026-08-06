/**
 * @file eos_developer_options.c
 * @brief Developer options features (OBJS display, touch tracking, FPS monitor)
 */

#include "eos_developer_options.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "lvgl.h"
#define EOS_LOG_TAG "DevOptions"
#include "eos_log.h"
#include "eos_mem.h"
#include "eos_activity.h"
#include "eos_service_config.h"

/* Macros and Definitions -------------------------------------*/
#define _TOUCH_X_COLOR lv_color_hex(0x0000FF) /* Blue for X axis */
#define _TOUCH_Y_COLOR lv_color_hex(0xFF0000) /* Red for Y axis */
#define _TOUCH_TRAIL_COLOR lv_color_hex(0x00BFFF) /* Cyan trajectory */
#define _TOUCH_LINE_THICKNESS 1
#define _OBJS_UPDATE_INTERVAL_MS 1000
#define _FPS_UPDATE_INTERVAL_MS 1000

/* Variables --------------------------------------------------*/

/* OBJS Display */
static lv_obj_t *_objs_label = NULL;
static bool _objs_enabled = false;

/* FPS Display */
static lv_obj_t *_fps_label = NULL;
static bool _fps_enabled = false;
static uint32_t _fps_frame_count = 0;
static uint32_t _fps_last_update = 0;

/* Touch Coordinate Display */
#define _TOUCH_MAX_SEGMENTS 1024
#define _TOUCH_SAMPLE_THRESHOLD 3

static lv_obj_t *_touch_label = NULL;
static lv_obj_t *_touch_label_x = NULL; /* Blue X coordinate label */
static lv_obj_t *_touch_label_y = NULL; /* Red Y coordinate label */
static lv_obj_t *_touch_cross_h = NULL; /* Horizontal crosshair line */
static lv_obj_t *_touch_cross_v = NULL; /* Vertical crosshair line */
static lv_indev_t *_touch_indev = NULL;
static lv_timer_t *_touch_timer = NULL;
static bool _touch_enabled = false;
static bool _touch_tracking = false;
static lv_point_t _touch_last_point;

/* Trajectory: single lv_line with dynamic points array */
static lv_obj_t *_touch_trail_line = NULL;
static lv_point_precise_t _touch_trail_pts[_TOUCH_MAX_SEGMENTS + 1];
static uint32_t _touch_trail_pt_count = 0;
static lv_style_t _touch_line_style;
static bool _touch_line_style_inited = false;

/* Function Implementations -----------------------------------*/

/* OBJS Display -----------------------------------------------*/

/**
 * @brief Count visible (non-hidden) LVGL objects in a subtree
 */
static uint32_t _count_visible_children(lv_obj_t *obj)
{
    if (!obj)
        return 0;
    /* Skip hidden subtrees entirely — LVGL does the same during rendering */
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN))
        return 0;
    uint32_t count = 1;
    uint32_t n = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < n; i++)
    {
        count += _count_visible_children(lv_obj_get_child(obj, i));
    }
    return count;
}

/**
 * @brief Create the OBJS display label on the system layer
 */
static void _objs_label_create(void)
{
    if (_objs_label)
        return;

    _objs_label = lv_label_create(lv_layer_sys());
    /* White text */
    lv_obj_set_style_text_color(_objs_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(_objs_label, &lv_font_montserrat_14, 0);
    /* Semi-transparent black background, matching FPS display style */
    lv_obj_set_style_bg_opa(_objs_label, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(_objs_label, lv_color_black(), 0);
    lv_obj_set_style_pad_all(_objs_label, 3, 0);
    lv_obj_align(_objs_label, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_label_set_text(_objs_label, "OBJS: 0");
}

/**
 * @brief Destroy the OBJS display label
 */
static void _objs_label_destroy(void)
{
    if (_objs_label && lv_obj_is_valid(_objs_label))
    {
        lv_obj_delete(_objs_label);
    }
    _objs_label = NULL;
}

/* FPS Display -----------------------------------------------*/

/**
 * @brief Create the FPS display label on the system layer
 */
static void _fps_label_create(void)
{
    if (_fps_label)
        return;

    _fps_label = lv_label_create(lv_layer_sys());
    lv_obj_set_style_text_color(_fps_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(_fps_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_opa(_fps_label, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(_fps_label, lv_color_black(), 0);
    lv_obj_set_style_pad_all(_fps_label, 3, 0);
    lv_obj_align(_fps_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_label_set_text(_fps_label, "FPS: --");
    _fps_frame_count = 0;
    _fps_last_update = lv_tick_get();
}

/**
 * @brief Destroy the FPS display label
 */
static void _fps_label_destroy(void)
{
    if (_fps_label && lv_obj_is_valid(_fps_label))
    {
        lv_obj_delete(_fps_label);
    }
    _fps_label = NULL;
}

/**
 * @brief Update FPS display (called from main loop, throttled to 1s)
 */
static void _fps_display_update(void)
{
    if (!_fps_label)
        return;

    _fps_frame_count++;

    uint32_t now = lv_tick_get();
    uint32_t elapsed = now - _fps_last_update;
    if (elapsed < _FPS_UPDATE_INTERVAL_MS)
        return;

    uint32_t fps = (_fps_frame_count * 1000) / elapsed;
    lv_label_set_text_fmt(_fps_label, "FPS: %" PRIu32, fps);
    _fps_frame_count = 0;
    _fps_last_update = now;
}

/**
 * @brief Update OBJS display label text
 */
static void _objs_display_update(void)
{
    if (!_objs_label)
        return;

    static uint32_t last_update = 0;
    uint32_t now = lv_tick_get();
    if (now - last_update < _OBJS_UPDATE_INTERVAL_MS)
        return;
    last_update = now;

    uint32_t n = 0;
    eos_activity_t *act = eos_activity_get_current();
    if (act)
    {
        lv_obj_t *view = eos_activity_get_view(act);
        if (view && lv_obj_is_valid(view))
        {
            n = _count_visible_children(view);
        }
    }
    lv_label_set_text_fmt(_objs_label, "OBJS: %" PRIu32, n);
}

/* Touch Coordinate Display -----------------------------------*/

/**
 * @brief Delete the trail line
 */
static void _touch_trail_clear(void)
{
    if (_touch_trail_line && lv_obj_is_valid(_touch_trail_line))
    {
        lv_obj_delete(_touch_trail_line);
    }
    _touch_trail_line = NULL;
    _touch_trail_pt_count = 0;
    _touch_tracking = false;
}

/**
 * @brief Initialize the line style once
 */
static void _touch_line_style_ensure(void)
{
    if (_touch_line_style_inited)
        return;
    lv_style_init(&_touch_line_style);
    lv_style_set_line_color(&_touch_line_style, _TOUCH_TRAIL_COLOR);
    lv_style_set_line_width(&_touch_line_style, 2);
    lv_style_set_line_rounded(&_touch_line_style, true);
    _touch_line_style_inited = true;
}

/**
 * @brief Start a new trail — create a fresh lv_line with initial point
 */
static void _touch_trail_begin(int32_t x, int32_t y)
{
    _touch_trail_clear();

    _touch_line_style_ensure();
    _touch_trail_line = lv_line_create(lv_layer_sys());
    lv_obj_add_style(_touch_trail_line, &_touch_line_style, 0);
    lv_obj_remove_flag(_touch_trail_line, LV_OBJ_FLAG_CLICKABLE);

    _touch_trail_pts[0].x = x;
    _touch_trail_pts[0].y = y;
    _touch_trail_pt_count = 1;
    lv_line_set_points(_touch_trail_line, _touch_trail_pts, 1);
}

/**
 * @brief Add a point to the trail polyline
 */
static void _touch_trail_add_point(int32_t x, int32_t y)
{
    if (!_touch_trail_line || _touch_trail_pt_count >= _TOUCH_MAX_SEGMENTS + 1)
        return;

    _touch_trail_pts[_touch_trail_pt_count].x = x;
    _touch_trail_pts[_touch_trail_pt_count].y = y;
    _touch_trail_pt_count++;
    lv_line_set_points(_touch_trail_line, _touch_trail_pts, _touch_trail_pt_count);
}

/**
 * @brief Update crosshair position
 */
static void _touch_crosshair_update(int32_t x, int32_t y)
{
    lv_display_t *disp = lv_display_get_default();
    if (!disp)
        return;

    int32_t screen_w = lv_display_get_horizontal_resolution(disp);
    int32_t screen_h = lv_display_get_vertical_resolution(disp);

    if (_touch_cross_h && lv_obj_is_valid(_touch_cross_h))
    {
        lv_obj_set_size(_touch_cross_h, screen_w, _TOUCH_LINE_THICKNESS);
        lv_obj_set_pos(_touch_cross_h, 0, y);
    }

    if (_touch_cross_v && lv_obj_is_valid(_touch_cross_v))
    {
        lv_obj_set_size(_touch_cross_v, _TOUCH_LINE_THICKNESS, screen_h);
        lv_obj_set_pos(_touch_cross_v, x, 0);
    }
}

/**
 * @brief Update coordinate label text
 */
static void _touch_label_update(int32_t x, int32_t y)
{
    if (_touch_label_x && lv_obj_is_valid(_touch_label_x))
    {
        lv_label_set_text_fmt(_touch_label_x, "X: %d", x);
    }
    if (_touch_label_y && lv_obj_is_valid(_touch_label_y))
    {
        lv_label_set_text_fmt(_touch_label_y, "  Y: %d", y);
    }
}

/**
 * @brief Touch polling timer callback (runs at ~30Hz)
 *
 * Polls the pointer indev directly — more reliable than event callbacks
 * which may not fire on the indev itself in all LVGL configurations.
 */
static void _touch_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!_touch_indev)
        return;

    lv_indev_state_t state = lv_indev_get_state(_touch_indev);
    lv_point_t point;
    lv_indev_get_point(_touch_indev, &point);

    if (state == LV_INDEV_STATE_PRESSED)
    {
        if (!_touch_tracking)
        {
            /* Touch just started — begin trail, show crosshair */
            _touch_trail_begin(point.x, point.y);
            _touch_tracking = true;
            _touch_last_point = point;

            if (_touch_cross_h)
                lv_obj_remove_flag(_touch_cross_h, LV_OBJ_FLAG_HIDDEN);
            if (_touch_cross_v)
                lv_obj_remove_flag(_touch_cross_v, LV_OBJ_FLAG_HIDDEN);
            if (_touch_label)
                lv_obj_remove_flag(_touch_label, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            /* Touch continues — extend polyline if moved enough */
            if (LV_ABS(point.x - _touch_last_point.x) >= _TOUCH_SAMPLE_THRESHOLD ||
                LV_ABS(point.y - _touch_last_point.y) >= _TOUCH_SAMPLE_THRESHOLD)
            {
                _touch_trail_add_point(point.x, point.y);
                _touch_last_point = point;
            }
        }
        /* Update crosshair and label every tick */
        _touch_crosshair_update(point.x, point.y);
        _touch_label_update(point.x, point.y);
    }
    else
    {
        if (_touch_tracking)
        {
            /* Touch ended — hide crosshair, keep trail */
            _touch_tracking = false;
            if (_touch_cross_h)
                lv_obj_add_flag(_touch_cross_h, LV_OBJ_FLAG_HIDDEN);
            if (_touch_cross_v)
                lv_obj_add_flag(_touch_cross_v, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/**
 * @brief Find the pointer (touch/mouse) input device
 */
static lv_indev_t *_find_pointer_indev(void)
{
    lv_indev_t *indev = NULL;
    while ((indev = lv_indev_get_next(indev)) != NULL)
    {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER)
        {
            return indev;
        }
    }
    return NULL;
}

/**
 * @brief Enable touch coordinate display
 */
static void _touch_display_enable(void)
{
    if (_touch_enabled)
        return;

    lv_display_t *disp = lv_display_get_default();
    if (!disp)
        return;

    int32_t screen_w = lv_display_get_horizontal_resolution(disp);
    int32_t screen_h = lv_display_get_vertical_resolution(disp);

    if (screen_w <= 0 || screen_h <= 0)
        return;

    /* Find pointer indev */
    _touch_indev = _find_pointer_indev();
    if (!_touch_indev)
    {
        EOS_LOG_W("No pointer input device found");
        return;
    }

    /* Create coordinate label container on system layer */
    _touch_label = lv_obj_create(lv_layer_sys());
    lv_obj_set_style_bg_opa(_touch_label, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(_touch_label, lv_color_black(), 0);
    lv_obj_set_style_border_width(_touch_label, 0, 0);
    lv_obj_set_style_pad_all(_touch_label, 3, 0);
    lv_obj_set_size(_touch_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(_touch_label, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_touch_label, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(_touch_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_flag(_touch_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_touch_label, LV_OBJ_FLAG_CLICKABLE);

    /* X label (blue) */
    _touch_label_x = lv_label_create(_touch_label);
    lv_obj_set_style_text_color(_touch_label_x, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_text_font(_touch_label_x, &lv_font_montserrat_14, 0);
    lv_label_set_text(_touch_label_x, "X: 0");

    /* Y label (red) */
    _touch_label_y = lv_label_create(_touch_label);
    lv_obj_set_style_text_color(_touch_label_y, lv_color_hex(0x0000FF), 0);
    lv_obj_set_style_text_font(_touch_label_y, &lv_font_montserrat_14, 0);
    lv_label_set_text(_touch_label_y, "  Y: 0");

    /* Create crosshair lines (non-interactive) */
    _touch_cross_h = lv_obj_create(lv_layer_sys());
    lv_obj_set_style_bg_color(_touch_cross_h, _TOUCH_Y_COLOR, 0); /* Red = Y axis */
    lv_obj_set_style_bg_opa(_touch_cross_h, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_touch_cross_h, 0, 0);
    lv_obj_set_style_pad_all(_touch_cross_h, 0, 0);
    lv_obj_set_style_radius(_touch_cross_h, 0, 0);
    lv_obj_set_size(_touch_cross_h, screen_w, _TOUCH_LINE_THICKNESS);
    lv_obj_add_flag(_touch_cross_h, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_touch_cross_h, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(_touch_cross_h, LV_OBJ_FLAG_SCROLLABLE);

    _touch_cross_v = lv_obj_create(lv_layer_sys());
    lv_obj_set_style_bg_color(_touch_cross_v, _TOUCH_X_COLOR, 0); /* Blue = X axis */
    lv_obj_set_style_bg_opa(_touch_cross_v, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_touch_cross_v, 0, 0);
    lv_obj_set_style_pad_all(_touch_cross_v, 0, 0);
    lv_obj_set_style_radius(_touch_cross_v, 0, 0);
    lv_obj_set_size(_touch_cross_v, _TOUCH_LINE_THICKNESS, screen_h);
    lv_obj_add_flag(_touch_cross_v, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_touch_cross_v, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(_touch_cross_v, LV_OBJ_FLAG_SCROLLABLE);

    /* Create polling timer at ~30Hz for smooth tracking */
    _touch_timer = lv_timer_create(_touch_timer_cb, 33, NULL);

    _touch_enabled = true;
    EOS_LOG_I("Touch coordinate display enabled");
}

/**
 * @brief Disable touch coordinate display
 */
static void _touch_display_disable(void)
{
    if (!_touch_enabled)
        return;

    /* Delete polling timer */
    if (_touch_timer)
    {
        lv_timer_delete(_touch_timer);
        _touch_timer = NULL;
    }
    _touch_indev = NULL;

    /* Destroy crosshair lines */
    if (_touch_cross_h && lv_obj_is_valid(_touch_cross_h))
        lv_obj_delete(_touch_cross_h);
    _touch_cross_h = NULL;

    if (_touch_cross_v && lv_obj_is_valid(_touch_cross_v))
        lv_obj_delete(_touch_cross_v);
    _touch_cross_v = NULL;

    /* Clear trajectory trail */
    _touch_trail_clear();

    /* Destroy label container (cascades to X/Y children) */
    if (_touch_label && lv_obj_is_valid(_touch_label))
        lv_obj_delete(_touch_label);
    _touch_label = NULL;
    _touch_label_x = NULL;
    _touch_label_y = NULL;

    _touch_enabled = false;
    EOS_LOG_I("Touch coordinate display disabled");
}

/* Public API ------------------------------------------------*/

void eos_developer_options_init(void)
{
    /* Restore persisted state from config — use setters so guards and actual
     * enable/disable actions are executed correctly */
    bool fps_on = eos_config_get_bool(EOS_CONFIG_KEY_DEV_FPS_BOOL, false);
    if (fps_on)
    {
        eos_developer_options_set_fps_enabled(true);
    }

    bool objs_on = eos_config_get_bool(EOS_CONFIG_KEY_DEV_OBJS_BOOL, false);
    if (objs_on)
    {
        eos_developer_options_set_objs_enabled(true);
    }

    bool touch_on = eos_config_get_bool(EOS_CONFIG_KEY_DEV_TOUCH_BOOL, false);
    if (touch_on)
    {
        eos_developer_options_set_touch_enabled(true);
    }
}

void eos_developer_options_update(void)
{
    if (_fps_enabled)
    {
        _fps_display_update();
    }
    if (_objs_enabled)
    {
        _objs_display_update();
    }
}

void eos_developer_options_set_fps_enabled(bool enabled)
{
    if (enabled == _fps_enabled)
        return;

    _fps_enabled = enabled;

    if (enabled)
    {
        _fps_label_create();
    }
    else
    {
        _fps_label_destroy();
    }

    eos_config_set_bool(EOS_CONFIG_KEY_DEV_FPS_BOOL, enabled);
}

bool eos_developer_options_get_fps_enabled(void)
{
    return _fps_enabled;
}

void eos_developer_options_set_objs_enabled(bool enabled)
{
    if (enabled == _objs_enabled)
        return;

    _objs_enabled = enabled;

    if (enabled)
    {
        _objs_label_create();
    }
    else
    {
        _objs_label_destroy();
    }

    eos_config_set_bool(EOS_CONFIG_KEY_DEV_OBJS_BOOL, enabled);
}

bool eos_developer_options_get_objs_enabled(void)
{
    return _objs_enabled;
}

void eos_developer_options_set_touch_enabled(bool enabled)
{
    if (enabled == _touch_enabled)
        return;

    if (enabled)
    {
        _touch_display_enable();
    }
    else
    {
        _touch_display_disable();
    }

    /* _touch_enabled is managed by _touch_display_enable/disable internally */
    eos_config_set_bool(EOS_CONFIG_KEY_DEV_TOUCH_BOOL, _touch_enabled);
}

bool eos_developer_options_get_touch_enabled(void)
{
    return _touch_enabled;
}
