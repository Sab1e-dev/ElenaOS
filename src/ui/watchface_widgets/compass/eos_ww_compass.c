/**
 * @file eos_ww_compass.c
 * @brief Watchface compass (magnetometer-driven needle)
 */

#include "eos_ww_compass.h"

/* Includes ---------------------------------------------------*/
#include <math.h>
#include "eos_ww_common.h"
#include "eos_mem.h"
#include "eos_font.h"
#include "eos_service_sensor.h"
#define EOS_LOG_TAG "Compass"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
/* Variables --------------------------------------------------*/
typedef struct
{
    lv_obj_t *needle;
} _compass_t;

/* Function Implementations -----------------------------------*/

static void _compass_sensor_cb(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    _compass_t *c = user_data;
    (void)type;
    if (!c || !c->needle)
    {
        return;
    }

    /* Magnetic north heading from the magnetometer X/Y plane */
    double heading = atan2((double)data->data.mag.y, (double)data->data.mag.x) * 180.0 / M_PI;
    lv_obj_set_style_transform_rotation(c->needle, (int32_t)(-heading * 10), 0);
}

static void _compass_delete_cb(lv_event_t *e)
{
    _compass_t *c = lv_event_get_user_data(e);
    if (!c)
    {
        return;
    }
    eos_sensor_unsubscribe(EOS_SENSOR_TYPE_MAG, _compass_sensor_cb, c);
    eos_free(c);
}

lv_obj_t *eos_ww_compass_create(lv_obj_t *parent, lv_coord_t size, uint32_t needle_color)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);
    if (size <= 0)
    {
        return NULL;
    }

    _compass_t *c = eos_malloc_zeroed(sizeof(_compass_t));
    EOS_CHECK_PTR_RETURN_VAL(c, NULL);

    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, size, size);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(container, size / 2, 0);
    lv_obj_set_style_border_width(container, 2, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x2E3640), 0);
    eos_ww_make_static(container);

    /* North marker */
    lv_obj_t *north = lv_label_create(container);
    lv_label_set_text(north, "N");
    lv_obj_align(north, LV_ALIGN_TOP_MID, 0, 0);
    eos_label_set_font_size(north, EOS_FONT_SIZE_SMALL);
    lv_obj_set_style_text_color(north, lv_color_hex(0xFF3B30), 0);
    eos_ww_make_static(north);

    /* Needle — thin bar rotating around the container centre */
    lv_coord_t needle_h = size * 4 / 10;
    lv_obj_t *needle = lv_obj_create(container);
    lv_obj_set_size(needle, 3, needle_h);
    lv_obj_center(needle);
    lv_obj_set_style_transform_pivot_x(needle, 1, 0);
    lv_obj_set_style_transform_pivot_y(needle, needle_h / 2, 0);
    lv_obj_set_style_bg_color(needle, lv_color_hex(needle_color), 0);
    lv_obj_set_style_bg_opa(needle, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(needle, 1, 0);
    eos_ww_make_static(needle);

    c->needle = needle;

    lv_obj_add_event_cb(container, _compass_delete_cb, LV_EVENT_DELETE, c);
    eos_sensor_subscribe(EOS_SENSOR_TYPE_MAG, _compass_sensor_cb, c, 100);

    return container;
}
