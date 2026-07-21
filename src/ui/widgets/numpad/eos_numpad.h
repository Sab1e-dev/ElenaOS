/**
 * @file eos_numpad.h
 * @brief Reusable numeric passcode keypad widget (Apple Watch style)
 *
 * Provides a 3x4 grid of circular buttons with dot indicators for
 * entering a numeric passcode. Used by both the lock screen and
 * the settings password page.
 */

#ifndef EOS_NUMPAD_H
#define EOS_NUMPAD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

/* Public macros ----------------------------------------------*/

#define EOS_NUMPAD_BUTTON_SIZE 72
#define EOS_NUMPAD_GAP 14
#define EOS_NUMPAD_COLS 3
#define EOS_NUMPAD_ROWS 4
#define EOS_NUMPAD_OVERFLOW_PAD 20 /* Extra padding so scaled buttons don't get clipped */
#define EOS_NUMPAD_GRID_W \
    (EOS_NUMPAD_COLS * EOS_NUMPAD_BUTTON_SIZE + (EOS_NUMPAD_COLS - 1) * EOS_NUMPAD_GAP + EOS_NUMPAD_OVERFLOW_PAD * 2)
#define EOS_NUMPAD_GRID_H \
    (EOS_NUMPAD_ROWS * EOS_NUMPAD_BUTTON_SIZE + (EOS_NUMPAD_ROWS - 1) * EOS_NUMPAD_GAP + EOS_NUMPAD_OVERFLOW_PAD * 2)
#define EOS_NUMPAD_BTN_CX(col) \
    (EOS_NUMPAD_OVERFLOW_PAD + EOS_NUMPAD_BUTTON_SIZE / 2 + (col) * (EOS_NUMPAD_BUTTON_SIZE + EOS_NUMPAD_GAP))
#define EOS_NUMPAD_BTN_CY(row) \
    (EOS_NUMPAD_OVERFLOW_PAD + EOS_NUMPAD_BUTTON_SIZE / 2 + (row) * (EOS_NUMPAD_BUTTON_SIZE + EOS_NUMPAD_GAP))
#define EOS_NUMPAD_BTN_X(col) (EOS_NUMPAD_BTN_CX(col) - EOS_NUMPAD_BUTTON_SIZE / 2)
#define EOS_NUMPAD_BTN_Y(row) (EOS_NUMPAD_BTN_CY(row) - EOS_NUMPAD_BUTTON_SIZE / 2)

#define EOS_NUMPAD_DOT_SIZE 14
#define EOS_NUMPAD_DOT_GAP 16
#define EOS_NUMPAD_MAX_DIGITS 6

/* Public typedefs --------------------------------------------*/

/**
 * @brief Numpad context — fields are public for caller access
 */
typedef struct
{
    lv_obj_t *container; /* Flex-column wrapper holding dots + grid */
    lv_obj_t *dot_container; /* Flex-row of dot indicators */
    lv_obj_t *dots[EOS_NUMPAD_MAX_DIGITS]; /* Individual dot widgets */
    char entered_digits[EOS_NUMPAD_MAX_DIGITS + 1]; /* Entered digit buffer (null-terminated) */
    uint8_t digit_count; /* Number of digits entered so far */
    uint8_t target_length; /* Expected passcode length (4 or 6) */

    /* Callbacks */
    void (*on_complete)(const char *digits, void *user_data);
    void (*on_cancel)(void *user_data);
    void *user_data;
} eos_numpad_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a numpad widget with dot indicators and a 3x4 button grid.
 *
 * The widget is created as a child of `parent`. It contains a dot-indicator row
 * and the numeric keypad grid (1-9, backspace, 0, and optionally a cancel button).
 *
 * @param parent         Parent LVGL object (should be a flex-column container)
 * @param target_length  Expected passcode length (4 or 6)
 * @param show_cancel    If true, bottom-right cell shows a cancel (←) button
 * @param on_complete    Called (via lv_async_call) when all digits are entered
 * @param on_cancel      Called when the cancel button is pressed (may be NULL)
 * @param user_data      Opaque pointer passed to callbacks
 * @return eos_numpad_t* Allocated numpad context, or NULL on failure
 */
eos_numpad_t *eos_numpad_create(lv_obj_t *parent,
                                uint8_t target_length,
                                bool show_cancel,
                                void (*on_complete)(const char *digits, void *user_data),
                                void (*on_cancel)(void *user_data),
                                void *user_data);

/**
 * @brief Clear all entered digits and reset dot indicators.
 */
void eos_numpad_clear(eos_numpad_t *numpad);

/**
 * @brief Delete the numpad widget and free its context.
 * @note Does NOT free user_data — the caller owns that.
 */
void eos_numpad_delete(eos_numpad_t *numpad);

#ifdef __cplusplus
}
#endif

#endif /* EOS_NUMPAD_H */
