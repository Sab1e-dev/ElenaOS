/**
 * @file eos_ww_activity_rings.c
 * @brief Watchface activity rings (move / exercise / stand)
 */

#include "eos_ww_activity_rings.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include "eos_ww_common.h"
#include "eos_mem.h"
#include "eos_font.h"
#include "eos_service_sensor.h"
#define EOS_LOG_TAG "ActivityRings"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/
#define _RING_COUNT 3
#define _MOVE_GOAL 500
#define _EXERCISE_GOAL 300
#define _STAND_GOAL 12
#define _RING_WIDTH 6
/* Variables --------------------------------------------------*/
typedef struct
{
    lv_obj_t *rings[_RING_COUNT];
    lv_obj_t *label;
} _activity_rings_t;

/* Function Implementations -----------------------------------*/

static void _activity_rings_sensor_cb(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    _activity_rings_t *a = user_data;
    (void)type;
    if (!a)
    {
        return;
    }

    uint32_t steps = data->data.step.steps;
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", steps);
    lv_label_set_text(a->label, buf);

    lv_arc_set_value(a->rings[0], (int32_t)(steps % _MOVE_GOAL));
    lv_arc_set_value(a->rings[1], (int32_t)(steps % _EXERCISE_GOAL));
    lv_arc_set_value(a->rings[2], (int32_t)(steps % _STAND_GOAL));
}

static void _activity_rings_delete_cb(lv_event_t *e)
{
    _activity_rings_t *a = lv_event_get_user_data(e);
    if (!a)
    {
        return;
    }
    eos_sensor_unsubscribe(EOS_SENSOR_TYPE_STEP, _activity_rings_sensor_cb, a);
    eos_free(a);
}

static void _setup_ring(lv_obj_t *arc, uint32_t goal, uint32_t color, uint32_t track_color)
{
    lv_arc_set_range(arc, 0, (int32_t)goal);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_rotation(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc, _RING_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, _RING_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(track_color), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_INDICATOR);
}

lv_obj_t *eos_ww_activity_rings_create(lv_obj_t *parent, lv_coord_t size, uint32_t track_color)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);
    if (size <= 0)
    {
        return NULL;
    }

    _activity_rings_t *a = eos_malloc_zeroed(sizeof(_activity_rings_t));
    EOS_CHECK_PTR_RETURN_VAL(a, NULL);

    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, size, size);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    eos_ww_make_static(container);

    static const uint32_t colors[_RING_COUNT] = {0xFF3B30, 0x4CD964, 0x5AC8FA};
    static const uint32_t goals[_RING_COUNT] = {_MOVE_GOAL, _EXERCISE_GOAL, _STAND_GOAL};

    for (uint8_t i = 0; i < _RING_COUNT; i++)
    {
        lv_coord_t ring_size = size - i * 2 * (_RING_WIDTH + 4);
        lv_obj_t *arc = lv_arc_create(container);
        lv_obj_set_size(arc, ring_size, ring_size);
        lv_obj_center(arc);
        _setup_ring(arc, goals[i], colors[i], track_color);
        a->rings[i] = arc;
    }

    lv_obj_t *label = lv_label_create(container);
    lv_obj_center(label);
    eos_label_set_font_size(label, EOS_FONT_SIZE_SMALL);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    eos_ww_make_static(label);
    a->label = label;

    lv_obj_add_event_cb(container, _activity_rings_delete_cb, LV_EVENT_DELETE, a);
    eos_sensor_subscribe(EOS_SENSOR_TYPE_STEP, _activity_rings_sensor_cb, a, 1000);

    return container;
}
