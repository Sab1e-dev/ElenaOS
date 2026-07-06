/**
 * @file eos_numpad.c
 * @brief Reusable numeric passcode keypad widget implementation
 */

#include "eos_numpad.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#define EOS_LOG_TAG "Numpad"
#include "eos_log.h"
#include "eos_icon.h"
#include "eos_theme.h"
#include "eos_font.h"
#include "eos_mem.h"

/* Internal helper struct for deferred on_complete invocation */
typedef struct
{
    char digits[EOS_NUMPAD_MAX_DIGITS + 1];
    void (*on_complete)(const char *digits, void *user_data);
    void *user_data;
} _numpad_complete_data_t;

/* ---- Shared pressed style (all numpad instances) ---- */

static lv_style_t _numpad_style_pressed;
static bool _numpad_style_inited = false;

static void _numpad_btn_pressed_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn)
        lv_obj_move_foreground(btn);
}

/* ---- Internal helpers ---- */

static void _set_dot_filled(lv_obj_t *dot, bool filled)
{
    if (filled)
    {
        lv_obj_set_style_bg_color(dot, EOS_COLOR_WHITE, 0);
    }
    else
    {
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x555555), 0);
    }
}

static void _update_dots(eos_numpad_t *numpad)
{
    for (uint8_t i = 0; i < numpad->target_length; i++)
    {
        if (numpad->dots[i])
        {
            _set_dot_filled(numpad->dots[i], (i < numpad->digit_count));
        }
    }
}

static void _clear_digits(eos_numpad_t *numpad)
{
    numpad->digit_count = 0;
    memset(numpad->entered_digits, 0, sizeof(numpad->entered_digits));
    _update_dots(numpad);
}

static char _get_button_digit(lv_obj_t *btn)
{
    if (!btn || !lv_obj_is_valid(btn))
        return '\0';
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (!label || !lv_obj_is_valid(label))
        return '\0';
    const char *text = lv_label_get_text(label);
    if (!text || strlen(text) != 1)
        return '\0';
    return text[0];
}

static void _numpad_complete_async_cb(void *user_data)
{
    _numpad_complete_data_t *data = (_numpad_complete_data_t *)user_data;
    if (!data || !data->on_complete)
        return;
    data->on_complete(data->digits, data->user_data);
    eos_free(data);
}

/* ---- Event callbacks ---- */

static void _digit_btn_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    eos_numpad_t *numpad = (eos_numpad_t *)lv_event_get_user_data(e);
    if (!numpad || !btn)
        return;

    char digit = _get_button_digit(btn);
    if (digit < '0' || digit > '9')
        return;

    if (numpad->digit_count >= numpad->target_length)
        return;

    numpad->entered_digits[numpad->digit_count++] = digit;
    numpad->entered_digits[numpad->digit_count] = '\0';
    _update_dots(numpad);

    /* Defer on_complete via lv_async_call so the button click event
     * finishes processing before any activity stack manipulation */
    if (numpad->digit_count >= numpad->target_length && numpad->on_complete)
    {
        _numpad_complete_data_t *data = (_numpad_complete_data_t *)eos_malloc(sizeof(_numpad_complete_data_t));
        if (data)
        {
            strncpy(data->digits, numpad->entered_digits, EOS_NUMPAD_MAX_DIGITS);
            data->digits[EOS_NUMPAD_MAX_DIGITS] = '\0';
            data->on_complete = numpad->on_complete;
            data->user_data = numpad->user_data;
            lv_async_call(_numpad_complete_async_cb, data);
        }
    }
}

static void _backspace_btn_cb(lv_event_t *e)
{
    eos_numpad_t *numpad = (eos_numpad_t *)lv_event_get_user_data(e);
    if (!numpad)
        return;

    if (numpad->digit_count > 0)
    {
        numpad->digit_count--;
        numpad->entered_digits[numpad->digit_count] = '\0';
        _update_dots(numpad);
    }
}

static void _cancel_btn_cb(lv_event_t *e)
{
    eos_numpad_t *numpad = (eos_numpad_t *)lv_event_get_user_data(e);
    if (!numpad)
        return;

    if (numpad->on_cancel)
    {
        numpad->on_cancel(numpad->user_data);
    }
}

/* ---- Button factory ---- */

static void _numpad_create_button(lv_obj_t *grid,
                                  const char *text,
                                  uint32_t col,
                                  uint32_t row,
                                  lv_event_cb_t cb,
                                  void *user_data)
{
    /* One-time init of shared pressed style */
    if (!_numpad_style_inited)
    {
        lv_style_init(&_numpad_style_pressed);
        lv_style_set_transform_scale(&_numpad_style_pressed, 384); /* 150% = 256 * 1.5 */
        lv_style_set_bg_color(&_numpad_style_pressed, lv_color_lighten(EOS_THEME_BUTTON_COLOR, 64));
        _numpad_style_inited = true;
    }

    lv_obj_t *btn = lv_button_create(grid);
    lv_obj_set_size(btn, EOS_NUMPAD_BUTTON_SIZE, EOS_NUMPAD_BUTTON_SIZE);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, EOS_THEME_BUTTON_COLOR, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_transform_pivot_x(btn, EOS_NUMPAD_BUTTON_SIZE / 2, 0);
    lv_obj_set_style_transform_pivot_y(btn, EOS_NUMPAD_BUTTON_SIZE / 2, 0);
    lv_obj_add_style(btn, &_numpad_style_pressed, LV_STATE_PRESSED);
    /* Manual absolute positioning — buttons float freely, no clipping from grid cells */
    lv_obj_set_pos(btn, EOS_NUMPAD_BTN_X(col), EOS_NUMPAD_BTN_Y(row));
    lv_obj_add_event_cb(btn, _numpad_btn_pressed_cb, LV_EVENT_PRESSED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, EOS_COLOR_WHITE, 0);
    eos_label_set_font_size(label, EOS_FONT_SIZE_LARGE);

    if (cb)
    {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
}

/* ---- Public API ---- */

eos_numpad_t *eos_numpad_create(lv_obj_t *parent,
                                uint8_t target_length,
                                bool show_cancel,
                                void (*on_complete)(const char *digits, void *user_data),
                                void (*on_cancel)(void *user_data),
                                void *user_data)
{
    if (!parent)
        return NULL;

    eos_numpad_t *numpad = (eos_numpad_t *)eos_malloc_zeroed(sizeof(eos_numpad_t));
    if (!numpad)
    {
        EOS_LOG_E("Failed to allocate numpad context");
        return NULL;
    }

    numpad->target_length = target_length;
    numpad->on_complete = on_complete;
    numpad->on_cancel = on_cancel;
    numpad->user_data = user_data;

    /* ---- Dot indicator row ---- */
    numpad->dot_container = lv_obj_create(parent);
    lv_obj_set_size(numpad->dot_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(numpad->dot_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(numpad->dot_container, 0, 0);
    lv_obj_set_style_pad_all(numpad->dot_container, 0, 0);
    lv_obj_set_flex_flow(numpad->dot_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(numpad->dot_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(numpad->dot_container, EOS_NUMPAD_DOT_GAP, 0);

    for (uint8_t i = 0; i < target_length; i++)
    {
        lv_obj_t *dot = lv_obj_create(numpad->dot_container);
        lv_obj_set_size(dot, EOS_NUMPAD_DOT_SIZE, EOS_NUMPAD_DOT_SIZE);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x555555), 0);
        lv_obj_set_style_border_width(dot, 1, 0);
        lv_obj_set_style_border_color(dot, lv_color_hex(0x888888), 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        numpad->dots[i] = dot;
    }

    /* ---- Numpad grid container ---- */
    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_set_size(grid, EOS_NUMPAD_GRID_W, EOS_NUMPAD_GRID_H);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_add_flag(grid, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);

    /* Rows 0-2: digits 1-9 */
    for (int r = 0; r < 3; r++)
    {
        for (int c = 0; c < 3; c++)
        {
            char digit_str[2];
            digit_str[0] = (char)('1' + r * 3 + c);
            digit_str[1] = '\0';
            _numpad_create_button(grid, digit_str, c, r, _digit_btn_cb, numpad);
        }
    }

    /* Row 3, col 0: backspace */
    lv_obj_t *bs_btn = lv_button_create(grid);
    lv_obj_set_size(bs_btn, EOS_NUMPAD_BUTTON_SIZE, EOS_NUMPAD_BUTTON_SIZE);
    lv_obj_set_style_radius(bs_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bs_btn, EOS_THEME_BUTTON_COLOR, 0);
    lv_obj_set_style_border_width(bs_btn, 0, 0);
    lv_obj_set_style_shadow_width(bs_btn, 0, 0);
    lv_obj_set_style_transform_pivot_x(bs_btn, EOS_NUMPAD_BUTTON_SIZE / 2, 0);
    lv_obj_set_style_transform_pivot_y(bs_btn, EOS_NUMPAD_BUTTON_SIZE / 2, 0);
    lv_obj_add_style(bs_btn, &_numpad_style_pressed, LV_STATE_PRESSED);
    lv_obj_set_pos(bs_btn, EOS_NUMPAD_BTN_X(0), EOS_NUMPAD_BTN_Y(3));
    lv_obj_add_event_cb(bs_btn, _numpad_btn_pressed_cb, LV_EVENT_PRESSED, NULL);

    lv_obj_t *bs_label = lv_label_create(bs_btn);
    lv_label_set_text(bs_label, RI_DELETE_BACK_2_LINE);
    lv_obj_center(bs_label);
    lv_obj_set_style_text_color(bs_label, EOS_COLOR_WHITE, 0);
    eos_label_set_font_size(bs_label, EOS_FONT_SIZE_LARGE);
    lv_obj_add_event_cb(bs_btn, _backspace_btn_cb, LV_EVENT_CLICKED, numpad);

    /* Row 3, col 1: 0 */
    _numpad_create_button(grid, "0", 1, 3, _digit_btn_cb, numpad);

    /* Row 3, col 2: cancel button or empty */
    if (show_cancel)
    {
        lv_obj_t *cancel_btn = lv_button_create(grid);
        lv_obj_set_size(cancel_btn, EOS_NUMPAD_BUTTON_SIZE, EOS_NUMPAD_BUTTON_SIZE);
        lv_obj_set_style_radius(cancel_btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(cancel_btn, EOS_THEME_BUTTON_COLOR, 0);
        lv_obj_set_style_border_width(cancel_btn, 0, 0);
        lv_obj_set_style_shadow_width(cancel_btn, 0, 0);
        lv_obj_set_style_transform_pivot_x(cancel_btn, EOS_NUMPAD_BUTTON_SIZE / 2, 0);
        lv_obj_set_style_transform_pivot_y(cancel_btn, EOS_NUMPAD_BUTTON_SIZE / 2, 0);
        lv_obj_add_style(cancel_btn, &_numpad_style_pressed, LV_STATE_PRESSED);
        lv_obj_set_pos(cancel_btn, EOS_NUMPAD_BTN_X(2), EOS_NUMPAD_BTN_Y(3));
        lv_obj_add_event_cb(cancel_btn, _numpad_btn_pressed_cb, LV_EVENT_PRESSED, NULL);

        lv_obj_t *cancel_label = lv_label_create(cancel_btn);
        lv_label_set_text(cancel_label, RI_ARROW_LEFT_S_LINE);
        lv_obj_center(cancel_label);
        lv_obj_set_style_text_color(cancel_label, EOS_COLOR_WHITE, 0);
        eos_label_set_font_size(cancel_label, EOS_FONT_SIZE_LARGE);
        lv_obj_add_event_cb(cancel_btn, _cancel_btn_cb, LV_EVENT_CLICKED, numpad);
    }

    return numpad;
}

void eos_numpad_clear(eos_numpad_t *numpad)
{
    if (!numpad)
        return;
    _clear_digits(numpad);
}

void eos_numpad_delete(eos_numpad_t *numpad)
{
    if (!numpad)
        return;
    eos_free(numpad);
}
