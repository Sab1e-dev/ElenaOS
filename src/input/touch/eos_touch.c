/**
 * @file eos_touch.c
 * @brief Touch input device configuration
 */

#include "eos_touch.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "Touch"
#include "eos_log.h"

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

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

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
}
