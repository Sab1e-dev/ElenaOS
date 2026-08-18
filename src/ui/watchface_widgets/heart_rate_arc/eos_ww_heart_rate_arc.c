/**
 * @file eos_ww_heart_rate_arc.c
 * @brief Watchface heart rate arc (ring + BPM, colour-zoned)
 */

#include "eos_ww_heart_rate_arc.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include "eos_ww_common.h"
#include "eos_mem.h"
#include "eos_font.h"
#include "eos_service_sensor.h"
#define EOS_LOG_TAG "HeartRateArc"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/
#define _HR_MIN 40
#define _HR_MAX 180
/* Variables --------------------------------------------------*/
typedef struct
{
    lv_obj_t *arc;
    lv_obj_t *label;
} _heart_rate_arc_t;

/* Function Implementations -----------------------------------*/

static lv_color_t _hr_color(uint16_t hr)
{
    if (hr > 140)
    {
        return lv_color_hex(0xFF3B30); /* red */
    }
    if (hr > 100)
    {
        return lv_color_hex(0xFFCC00); /* yellow */
    }
    if (hr < 60)
    {
        return lv_color_hex(0x5AC8FA); /* blue */
    }
    return lv_color_hex(0x4CD964); /* green */
}

static void _heart_rate_arc_sensor_cb(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    _heart_rate_arc_t *h = user_data;
    (void)type;
    if (!h)
    {
        return;
    }

    uint16_t hr = data->data.hr.heart_rate;
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", hr);
    lv_label_set_text(h->label, buf);
    lv_arc_set_value(h->arc, hr);
    lv_obj_set_style_arc_color(h->arc, _hr_color(hr), LV_PART_INDICATOR);
}

static void _heart_rate_arc_delete_cb(lv_event_t *e)
{
    _heart_rate_arc_t *h = lv_event_get_user_data(e);
    if (!h)
    {
        return;
    }
    eos_sensor_unsubscribe(EOS_SENSOR_TYPE_HR, _heart_rate_arc_sensor_cb, h);
    eos_free(h);
}

lv_obj_t *eos_ww_heart_rate_arc_create(lv_obj_t *parent, lv_coord_t size, uint32_t track_color)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);
    if (size <= 0)
    {
        return NULL;
    }

    _heart_rate_arc_t *h = eos_malloc_zeroed(sizeof(_heart_rate_arc_t));
    EOS_CHECK_PTR_RETURN_VAL(h, NULL);

    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, size, size);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    eos_ww_make_static(container);

    lv_obj_t *arc = lv_arc_create(container);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_range(arc, _HR_MIN, _HR_MAX);
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

    h->arc = arc;
    h->label = label;

    lv_obj_add_event_cb(container, _heart_rate_arc_delete_cb, LV_EVENT_DELETE, h);
    eos_sensor_subscribe(EOS_SENSOR_TYPE_HR, _heart_rate_arc_sensor_cb, h, 1000);

    return container;
}
