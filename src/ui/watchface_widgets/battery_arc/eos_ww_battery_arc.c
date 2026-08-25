/**
 * @file eos_ww_battery_arc.c
 * @brief Watchface battery arc (ring + percent, colour-coded by level)
 */

#include "eos_ww_battery_arc.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include "eos_ww_common.h"
#include "eos_mem.h"
#include "eos_font.h"
#include "eos_service_battery.h"
#define EOS_LOG_TAG "BatteryArc"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/
typedef struct
{
    lv_obj_t *arc;
    lv_obj_t *label;
    lv_timer_t *timer;
} _battery_arc_t;

/* Function Implementations -----------------------------------*/

static lv_color_t _level_color(int8_t percent)
{
    if (percent > 50)
    {
        return lv_color_hex(0x4CD964); /* green */
    }
    if (percent > 20)
    {
        return lv_color_hex(0xFFCC00); /* yellow */
    }
    return lv_color_hex(0xFF3B30); /* red */
}

static void _battery_arc_update(_battery_arc_t *b)
{
    int8_t percent = eos_battery_get_percent();
    if (percent < 0)
    {
        percent = 0;
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    lv_label_set_text(b->label, buf);
    lv_arc_set_value(b->arc, percent);
    lv_obj_set_style_arc_color(b->arc, _level_color(percent), LV_PART_INDICATOR);
}

static void _battery_arc_timer_cb(lv_timer_t *timer)
{
    _battery_arc_t *b = lv_timer_get_user_data(timer);
    if (!b)
    {
        return;
    }
    _battery_arc_update(b);
}

static void _battery_arc_delete_cb(lv_event_t *e)
{
    _battery_arc_t *b = lv_event_get_user_data(e);
    if (!b)
    {
        return;
    }
    if (b->timer)
    {
        lv_timer_delete(b->timer);
    }
    eos_free(b);
}

lv_obj_t *eos_ww_battery_arc_create(lv_obj_t *parent, lv_coord_t size, uint32_t track_color)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);
    if (size <= 0)
    {
        return NULL;
    }

    _battery_arc_t *b = eos_malloc_zeroed(sizeof(_battery_arc_t));
    EOS_CHECK_PTR_RETURN_VAL(b, NULL);

    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, size, size);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    eos_ww_make_static(container);

    lv_obj_t *arc = lv_arc_create(container);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_rotation(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(track_color), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x4CD964), LV_PART_INDICATOR);

    lv_obj_t *label = lv_label_create(container);
    lv_obj_center(label);
    eos_label_set_font_size(label, EOS_FONT_SIZE_SMALL);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    eos_ww_make_static(label);

    b->arc = arc;
    b->label = label;
    b->timer = lv_timer_create(_battery_arc_timer_cb, 1000, b);
    lv_obj_add_event_cb(container, _battery_arc_delete_cb, LV_EVENT_DELETE, b);

    _battery_arc_update(b);
    return container;
}
