/**
 * @file eos_touch.c
 * @brief Touch input device configuration
 */

#include "eos_touch.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "Touch"
#include "eos_config.h"
#include "eos_log.h"
#include "eos_port_critical.h"

/* Macros and Definitions -------------------------------------*/

/**
 * @brief Scroll limit in pixels.
 *
 * Movement below this threshold is treated as a click (not a drag/scroll).
 * LVGL default is 10px; reduced for tighter click detection on small screens.
 */
#define EOS_TOUCH_SCROLL_LIMIT 5

/**
 * @brief Long press time in milliseconds.
 *
 * Time to press before LV_EVENT_LONG_PRESSED is sent.
 * LVGL default is 400ms.
 */
#define EOS_TOUCH_LONG_PRESS_TIME 400
#define EOS_TOUCH_INJECT_QUEUE_SIZE 64U
#define EOS_TOUCH_MARKER_SIZE 26U
#define EOS_TOUCH_INDEV_PERIOD_MS 33U
#define EOS_TOUCH_CONTROL_GLOW_INSET 4U
#define EOS_TOUCH_CONTROL_BANNER_WIDTH (EOS_DISPLAY_WIDTH - 80U)
#define EOS_TOUCH_CONTROL_BANNER_HEIGHT 30U
#define EOS_TOUCH_CONTROL_BANNER_BOTTOM_MARGIN 28U

/* Variables --------------------------------------------------*/

/* Synthetic samples are owned by the EOS/LVGL execution context.  Physical
 * samples are submitted by the platform adapter and copied under the EOS
 * critical-section abstraction.  No FreeRTOS type or platform lock leaks into
 * this broker. */
typedef struct
{
    int32_t x;
    int32_t y;
    lv_indev_state_t state;
    uint16_t repeat;
} _eos_touch_inject_sample_t;

static _eos_touch_inject_sample_t _eos_touch_inject_queue[EOS_TOUCH_INJECT_QUEUE_SIZE];
static uint8_t _eos_touch_inject_head;
static uint8_t _eos_touch_inject_tail;
static uint8_t _eos_touch_inject_count;
static _eos_touch_inject_sample_t _eos_touch_inject_current;
static uint16_t _eos_touch_inject_current_repeat;
static uint8_t _eos_touch_inject_active;
static uint8_t _eos_touch_inject_release_queued;
static uint8_t _eos_touch_inject_pressed_delivered;
static uint8_t _eos_touch_inject_abort_release_pending;
static uint8_t _eos_touch_ready;
static lv_obj_t *_eos_touch_marker;
static lv_obj_t *_eos_touch_control_glow;
static lv_obj_t *_eos_touch_control_banner;
static lv_obj_t *_eos_touch_control_label;
static eos_touch_control_state_t _eos_touch_control_state = EOS_TOUCH_CONTROL_UNCONTROLLED;
static uint8_t _eos_touch_physical_suppress;
static int32_t _eos_touch_physical_x;
static int32_t _eos_touch_physical_y;
static lv_indev_state_t _eos_touch_physical_state = LV_INDEV_STATE_REL;
static uint8_t _eos_touch_physical_valid;

/* Function Implementations -----------------------------------*/

static void _eos_touch_marker_hide(void);
static bool _eos_touch_coordinate_valid(int32_t x, int32_t y);

static bool _eos_touch_control_glow_init(void)
{
    /* Keep the control affordance in LVGL's system layer.  The synthetic
     * touch marker remains on lv_layer_top(), so a marker can still be seen
     * above this non-interactive status decoration. */
    _eos_touch_control_glow = lv_obj_create(lv_layer_sys());
    if (_eos_touch_control_glow == NULL)
    {
        EOS_LOG_E("Failed to create control glow object");
        return false;
    }

    lv_obj_remove_style_all(_eos_touch_control_glow);
    lv_obj_set_size(_eos_touch_control_glow,
                    EOS_DISPLAY_WIDTH - 2U * EOS_TOUCH_CONTROL_GLOW_INSET,
                    EOS_DISPLAY_HEIGHT - 2U * EOS_TOUCH_CONTROL_GLOW_INSET);
    lv_obj_set_pos(_eos_touch_control_glow, EOS_TOUCH_CONTROL_GLOW_INSET, EOS_TOUCH_CONTROL_GLOW_INSET);
    lv_obj_set_style_radius(_eos_touch_control_glow,
                            LV_MAX((int32_t)EOS_DISPLAY_RADIUS - (int32_t)EOS_TOUCH_CONTROL_GLOW_INSET, 0),
                            0);
    lv_obj_set_style_bg_opa(_eos_touch_control_glow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_eos_touch_control_glow, 6, 0);
    lv_obj_set_style_border_color(_eos_touch_control_glow, lv_color_hex(0x686CFF), 0);
    lv_obj_remove_flag(_eos_touch_control_glow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_eos_touch_control_glow, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(_eos_touch_control_glow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(_eos_touch_control_glow);
    return true;
}

void eos_touch_init(void)
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev)
    {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER)
        {
            lv_indev_set_scroll_limit(indev, EOS_TOUCH_SCROLL_LIMIT);
            lv_indev_set_long_press_time(indev, EOS_TOUCH_LONG_PRESS_TIME);
            EOS_LOG_I("scroll_limit=%d, long_press_time=%d", EOS_TOUCH_SCROLL_LIMIT, EOS_TOUCH_LONG_PRESS_TIME);
        }
        indev = lv_indev_get_next(indev);
    }

    if (_eos_touch_marker == NULL)
    {
        _eos_touch_marker = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(_eos_touch_marker);
        lv_obj_set_size(_eos_touch_marker, EOS_TOUCH_MARKER_SIZE, EOS_TOUCH_MARKER_SIZE);
        lv_obj_set_style_radius(_eos_touch_marker, LV_RADIUS_CIRCLE, 0);
        /* Bright, cool gradient for a Codex-like touch affordance: white at
         * the top, fading to a soft mint/cyan at the bottom. */
        lv_obj_set_style_bg_color(_eos_touch_marker, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_grad_color(_eos_touch_marker, lv_color_hex(0xB8F3E6), 0);
        lv_obj_set_style_bg_grad_dir(_eos_touch_marker, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(_eos_touch_marker, LV_OPA_80, 0);
        lv_obj_set_style_border_width(_eos_touch_marker, 2, 0);
        lv_obj_set_style_border_color(_eos_touch_marker, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_opa(_eos_touch_marker, LV_OPA_90, 0);
        lv_obj_set_style_shadow_width(_eos_touch_marker, 8, 0);
        lv_obj_set_style_shadow_spread(_eos_touch_marker, 1, 0);
        lv_obj_set_style_shadow_color(_eos_touch_marker, lv_color_hex(0x8DE8D7), 0);
        lv_obj_set_style_shadow_opa(_eos_touch_marker, LV_OPA_50, 0);
        lv_obj_remove_flag(_eos_touch_marker, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_eos_touch_marker, LV_OBJ_FLAG_HIDDEN);
    }

    if (_eos_touch_control_glow == NULL)
    {
        (void)_eos_touch_control_glow_init();
    }

    if (_eos_touch_control_banner == NULL)
    {
        /* The banner and its label belong to the system layer, below the
         * top-layer touch marker and other top-most interaction visuals. */
        _eos_touch_control_banner = lv_obj_create(lv_layer_sys());
        lv_obj_remove_style_all(_eos_touch_control_banner);
        lv_obj_set_size(_eos_touch_control_banner, EOS_TOUCH_CONTROL_BANNER_WIDTH, EOS_TOUCH_CONTROL_BANNER_HEIGHT);
        lv_obj_set_pos(_eos_touch_control_banner,
                       (EOS_DISPLAY_WIDTH - EOS_TOUCH_CONTROL_BANNER_WIDTH) / 2,
                       EOS_DISPLAY_HEIGHT - EOS_TOUCH_CONTROL_BANNER_BOTTOM_MARGIN - EOS_TOUCH_CONTROL_BANNER_HEIGHT);
        lv_obj_set_style_radius(_eos_touch_control_banner, 12, 0);
        lv_obj_set_style_bg_color(_eos_touch_control_banner, lv_color_hex(0x282C78), 0);
        lv_obj_set_style_bg_opa(_eos_touch_control_banner, LV_OPA_70, 0);
        lv_obj_set_style_border_width(_eos_touch_control_banner, 1, 0);
        lv_obj_set_style_border_color(_eos_touch_control_banner, lv_color_hex(0x8588FF), 0);
        lv_obj_set_style_border_opa(_eos_touch_control_banner, LV_OPA_70, 0);
        lv_obj_remove_flag(_eos_touch_control_banner, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_eos_touch_control_banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_background(_eos_touch_control_banner);

        _eos_touch_control_label = lv_label_create(_eos_touch_control_banner);
        lv_label_set_text(_eos_touch_control_label, "AUTO CONTROL  -  TOUCH TO TAKE OVER");
        lv_obj_set_style_text_color(_eos_touch_control_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_opa(_eos_touch_control_label, LV_OPA_90, 0);
        lv_obj_center(_eos_touch_control_label);
        lv_obj_remove_flag(_eos_touch_control_label, LV_OBJ_FLAG_CLICKABLE);
    }
    _eos_touch_ready = 1U;
}

bool eos_touch_submit_physical(int32_t x, int32_t y, bool pressed)
{
    if (!_eos_touch_coordinate_valid(x, y))
    {
        return false;
    }

    eos_critical_ctx_t ctx = eos_critical_enter();
    _eos_touch_physical_x = x;
    _eos_touch_physical_y = y;
    _eos_touch_physical_state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    _eos_touch_physical_valid = 1U;
    eos_critical_leave(ctx);
    return true;
}

lv_indev_t *eos_touch_get_indev(void)
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev)
    {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER)
        {
            // Find touch device
            return indev;
        }
        indev = lv_indev_get_next(indev);
    }

    return NULL;
}

static bool _eos_touch_coordinate_valid(int32_t x, int32_t y)
{
    return x >= 0 && x < (int32_t)EOS_DISPLAY_WIDTH && y >= 0 && y < (int32_t)EOS_DISPLAY_HEIGHT;
}

static bool _eos_touch_enqueue(int32_t x, int32_t y, lv_indev_state_t state, uint16_t repeat)
{
    if (_eos_touch_inject_count >= EOS_TOUCH_INJECT_QUEUE_SIZE)
    {
        return false;
    }

    _eos_touch_inject_queue[_eos_touch_inject_tail].x = x;
    _eos_touch_inject_queue[_eos_touch_inject_tail].y = y;
    _eos_touch_inject_queue[_eos_touch_inject_tail].state = state;
    _eos_touch_inject_queue[_eos_touch_inject_tail].repeat = repeat == 0U ? 1U : repeat;
    _eos_touch_inject_tail = (uint8_t)((_eos_touch_inject_tail + 1U) % EOS_TOUCH_INJECT_QUEUE_SIZE);
    _eos_touch_inject_count++;
    return true;
}

static void _eos_touch_reset_queue(void)
{
    _eos_touch_inject_head = 0U;
    _eos_touch_inject_tail = 0U;
    _eos_touch_inject_count = 0U;
    _eos_touch_inject_current_repeat = 0U;
    _eos_touch_inject_release_queued = 0U;
    _eos_touch_inject_pressed_delivered = 0U;
}

static void _eos_touch_control_overlay_set(bool visible)
{
    if (_eos_touch_control_glow == NULL || _eos_touch_control_banner == NULL)
    {
        return;
    }

    if (visible)
    {
        lv_obj_clear_flag(_eos_touch_control_glow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_eos_touch_control_banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(_eos_touch_control_glow);
        lv_obj_move_foreground(_eos_touch_control_banner);
    }
    else
    {
        lv_obj_add_flag(_eos_touch_control_glow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_eos_touch_control_banner, LV_OBJ_FLAG_HIDDEN);
    }
}

static void _eos_touch_abort_injection(void)
{
    if (_eos_touch_inject_active != 0U && _eos_touch_inject_pressed_delivered != 0U)
    {
        _eos_touch_inject_abort_release_pending = 1U;
    }

    _eos_touch_inject_active = 0U;
    _eos_touch_inject_pressed_delivered = 0U;
    _eos_touch_reset_queue();
    _eos_touch_marker_hide();
}

bool eos_touch_control_acquire(void)
{
    if (_eos_touch_ready == 0U)
    {
        return false;
    }

    if (_eos_touch_control_state == EOS_TOUCH_CONTROL_CONTROLLED)
    {
        _eos_touch_control_overlay_set(true);
        return true;
    }

    _eos_touch_abort_injection();
    _eos_touch_physical_suppress = 0U;
    _eos_touch_control_state = EOS_TOUCH_CONTROL_CONTROLLED;
    _eos_touch_control_overlay_set(true);
    return true;
}

void eos_touch_control_release(void)
{
    if (_eos_touch_ready == 0U)
    {
        return;
    }

    _eos_touch_abort_injection();
    _eos_touch_physical_suppress = 0U;
    _eos_touch_control_state = EOS_TOUCH_CONTROL_UNCONTROLLED;
    _eos_touch_control_overlay_set(false);
}

eos_touch_control_state_t eos_touch_control_get_state(void)
{
    return _eos_touch_control_state;
}

static bool _eos_touch_filter_physical(lv_indev_data_t *data)
{
    if (data == NULL)
    {
        return false;
    }

    if (_eos_touch_physical_suppress != 0U)
    {
        if (data->state == LV_INDEV_STATE_REL)
        {
            _eos_touch_physical_suppress = 0U;
        }
        data->state = LV_INDEV_STATE_REL;
        return true;
    }

    if (_eos_touch_control_state == EOS_TOUCH_CONTROL_CONTROLLED && data->state == LV_INDEV_STATE_PR)
    {
        /* A physical press is an explicit request to take control back.
         * Cancel automation before LVGL sees the real sample. */
        _eos_touch_control_state = EOS_TOUCH_CONTROL_UNCONTROLLED;
        _eos_touch_physical_suppress = 1U;
        _eos_touch_abort_injection();
        _eos_touch_control_overlay_set(false);
        data->state = LV_INDEV_STATE_REL;
        return true;
    }

    return false;
}

bool eos_touch_read(lv_indev_data_t *data)
{
    int32_t x = 0;
    int32_t y = 0;
    lv_indev_state_t state = LV_INDEV_STATE_REL;

    if (data == NULL)
    {
        return false;
    }

    {
        eos_critical_ctx_t ctx = eos_critical_enter();
        if (_eos_touch_physical_valid != 0U)
        {
            x = _eos_touch_physical_x;
            y = _eos_touch_physical_y;
            state = _eos_touch_physical_state;
        }
        eos_critical_leave(ctx);
    }

    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state = state;

    (void)_eos_touch_filter_physical(data);
    if (eos_touch_read_injected(data))
    {
        return true;
    }
    return true;
}

static eos_touch_inject_result_t _eos_touch_begin(void)
{
    if (_eos_touch_ready == 0U)
    {
        return EOS_TOUCH_INJECT_NOT_READY;
    }
    if (_eos_touch_inject_active != 0U)
    {
        return EOS_TOUCH_INJECT_BUSY;
    }

    _eos_touch_reset_queue();
    _eos_touch_inject_active = 1U;
    return EOS_TOUCH_INJECT_OK;
}

static void _eos_touch_marker_set(int32_t x, int32_t y)
{
    if (_eos_touch_marker == NULL)
    {
        return;
    }

    lv_obj_set_pos(_eos_touch_marker,
                   (lv_coord_t)(x - (int32_t)(EOS_TOUCH_MARKER_SIZE / 2U)),
                   (lv_coord_t)(y - (int32_t)(EOS_TOUCH_MARKER_SIZE / 2U)));
    lv_obj_clear_flag(_eos_touch_marker, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_eos_touch_marker);
}

static void _eos_touch_marker_hide(void)
{
    if (_eos_touch_marker != NULL)
    {
        lv_obj_add_flag(_eos_touch_marker, LV_OBJ_FLAG_HIDDEN);
    }
}

bool eos_touch_read_injected(lv_indev_data_t *data)
{
    if (data == NULL)
    {
        return false;
    }

    if (_eos_touch_inject_abort_release_pending != 0U)
    {
        data->point.x = (lv_coord_t)_eos_touch_inject_current.x;
        data->point.y = (lv_coord_t)_eos_touch_inject_current.y;
        data->state = LV_INDEV_STATE_REL;
        _eos_touch_inject_abort_release_pending = 0U;
        _eos_touch_marker_hide();
        return true;
    }

    if (_eos_touch_inject_active == 0U)
    {
        return false;
    }

    if (_eos_touch_inject_current_repeat == 0U)
    {
        if (_eos_touch_inject_count == 0U)
        {
            if (_eos_touch_inject_release_queued == 0U)
            {
                /* A standalone down() must remain pressed across indev
                 * reads until the caller supplies move() or up(). */
                data->point.x = (lv_coord_t)_eos_touch_inject_current.x;
                data->point.y = (lv_coord_t)_eos_touch_inject_current.y;
                data->state = LV_INDEV_STATE_PR;
                _eos_touch_marker_set(_eos_touch_inject_current.x, _eos_touch_inject_current.y);
                return true;
            }

            /* The release sample was returned on the previous read.  Resume
             * the physical device on this read. */
            _eos_touch_inject_active = 0U;
            _eos_touch_inject_release_queued = 0U;
            _eos_touch_inject_pressed_delivered = 0U;
            _eos_touch_marker_hide();
            return false;
        }

        _eos_touch_inject_current = _eos_touch_inject_queue[_eos_touch_inject_head];
        _eos_touch_inject_head = (uint8_t)((_eos_touch_inject_head + 1U) % EOS_TOUCH_INJECT_QUEUE_SIZE);
        _eos_touch_inject_count--;
        _eos_touch_inject_current_repeat = _eos_touch_inject_current.repeat;
    }

    data->point.x = (lv_coord_t)_eos_touch_inject_current.x;
    data->point.y = (lv_coord_t)_eos_touch_inject_current.y;
    data->state = _eos_touch_inject_current.state;
    if (data->state == LV_INDEV_STATE_PR)
    {
        _eos_touch_inject_pressed_delivered = 1U;
    }
    _eos_touch_marker_set(_eos_touch_inject_current.x, _eos_touch_inject_current.y);
    _eos_touch_inject_current_repeat--;
    return true;
}

eos_touch_inject_result_t eos_touch_inject_down(int32_t x, int32_t y)
{
    eos_touch_inject_result_t result;

    if (!_eos_touch_coordinate_valid(x, y))
    {
        return EOS_TOUCH_INJECT_INVALID;
    }
    result = _eos_touch_begin();
    if (result != EOS_TOUCH_INJECT_OK)
    {
        return result;
    }
    if (!_eos_touch_enqueue(x, y, LV_INDEV_STATE_PR, 1U))
    {
        _eos_touch_inject_active = 0U;
        return EOS_TOUCH_INJECT_QUEUE_FULL;
    }
    return EOS_TOUCH_INJECT_OK;
}

eos_touch_inject_result_t eos_touch_inject_move(int32_t x, int32_t y)
{
    if (!_eos_touch_coordinate_valid(x, y))
    {
        return EOS_TOUCH_INJECT_INVALID;
    }
    if (_eos_touch_inject_active == 0U)
    {
        return _eos_touch_ready == 0U ? EOS_TOUCH_INJECT_NOT_READY : EOS_TOUCH_INJECT_NO_ACTIVE;
    }
    if (_eos_touch_inject_release_queued != 0U)
    {
        return EOS_TOUCH_INJECT_BUSY;
    }
    return _eos_touch_enqueue(x, y, LV_INDEV_STATE_PR, 1U) ? EOS_TOUCH_INJECT_OK : EOS_TOUCH_INJECT_QUEUE_FULL;
}

eos_touch_inject_result_t eos_touch_inject_up(int32_t x, int32_t y)
{
    if (!_eos_touch_coordinate_valid(x, y))
    {
        return EOS_TOUCH_INJECT_INVALID;
    }
    if (_eos_touch_inject_active == 0U)
    {
        return _eos_touch_ready == 0U ? EOS_TOUCH_INJECT_NOT_READY : EOS_TOUCH_INJECT_NO_ACTIVE;
    }
    if (_eos_touch_inject_release_queued != 0U)
    {
        return EOS_TOUCH_INJECT_BUSY;
    }
    if (!_eos_touch_enqueue(x, y, LV_INDEV_STATE_REL, 1U))
    {
        return EOS_TOUCH_INJECT_QUEUE_FULL;
    }
    _eos_touch_inject_release_queued = 1U;
    return EOS_TOUCH_INJECT_OK;
}

eos_touch_inject_result_t eos_touch_inject_tap(int32_t x, int32_t y)
{
    eos_touch_inject_result_t result;

    if (!_eos_touch_coordinate_valid(x, y))
    {
        return EOS_TOUCH_INJECT_INVALID;
    }
    result = _eos_touch_begin();
    if (result != EOS_TOUCH_INJECT_OK)
    {
        return result;
    }
    if (!_eos_touch_enqueue(x, y, LV_INDEV_STATE_PR, 1U) || !_eos_touch_enqueue(x, y, LV_INDEV_STATE_REL, 1U))
    {
        _eos_touch_inject_active = 0U;
        _eos_touch_reset_queue();
        return EOS_TOUCH_INJECT_QUEUE_FULL;
    }
    _eos_touch_inject_release_queued = 1U;
    return EOS_TOUCH_INJECT_OK;
}

eos_touch_inject_result_t eos_touch_inject_swipe(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t duration_ms)
{
    eos_touch_inject_result_t result;
    uint32_t steps;
    uint32_t repeat;
    uint32_t i;

    if (!_eos_touch_coordinate_valid(x1, y1) || !_eos_touch_coordinate_valid(x2, y2) || duration_ms == 0U
        || duration_ms > 5000U)
    {
        return EOS_TOUCH_INJECT_INVALID;
    }
    result = _eos_touch_begin();
    if (result != EOS_TOUCH_INJECT_OK)
    {
        return result;
    }

    steps = (duration_ms + EOS_TOUCH_INDEV_PERIOD_MS - 1U) / EOS_TOUCH_INDEV_PERIOD_MS;
    if (steps < 2U)
    {
        steps = 2U;
    }
    if (steps > 32U)
    {
        steps = 32U;
    }
    repeat = (duration_ms + (steps * EOS_TOUCH_INDEV_PERIOD_MS) - 1U) / (steps * EOS_TOUCH_INDEV_PERIOD_MS);
    if (repeat == 0U)
    {
        repeat = 1U;
    }

    if (!_eos_touch_enqueue(x1, y1, LV_INDEV_STATE_PR, (uint16_t)repeat))
    {
        _eos_touch_inject_active = 0U;
        return EOS_TOUCH_INJECT_QUEUE_FULL;
    }
    for (i = 1U; i < steps; i++)
    {
        int32_t x = x1 + (int32_t)(((int64_t)(x2 - x1) * (int64_t)i) / (int64_t)steps);
        int32_t y = y1 + (int32_t)(((int64_t)(y2 - y1) * (int64_t)i) / (int64_t)steps);
        if (!_eos_touch_enqueue(x, y, LV_INDEV_STATE_PR, (uint16_t)repeat))
        {
            _eos_touch_inject_active = 0U;
            _eos_touch_reset_queue();
            return EOS_TOUCH_INJECT_QUEUE_FULL;
        }
    }
    if (!_eos_touch_enqueue(x2, y2, LV_INDEV_STATE_REL, 1U))
    {
        _eos_touch_inject_active = 0U;
        _eos_touch_reset_queue();
        return EOS_TOUCH_INJECT_QUEUE_FULL;
    }
    _eos_touch_inject_release_queued = 1U;
    return EOS_TOUCH_INJECT_OK;
}
