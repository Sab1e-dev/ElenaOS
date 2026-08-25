/**
 * @file eos_ww_tick_ring.c
 * @brief Watchface tick ring (radial tick marks)
 */

#include "eos_ww_tick_ring.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#include "eos_mem.h"
#define EOS_LOG_TAG "TickRing"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static lv_obj_t *_create_tick(lv_obj_t *parent,
                              lv_coord_t radius,
                              lv_coord_t margin,
                              int32_t angle_deg,
                              lv_coord_t width,
                              lv_coord_t length,
                              uint32_t color)
{
    lv_obj_t *tick = lv_obj_create(parent);
    lv_obj_set_size(tick, width, length);

    /* Reference position: top of the ring container */
    lv_obj_set_pos(tick, radius - width / 2, margin);

    /* Pivot at the ring centre so rotation spins the tick around the face */
    lv_obj_set_style_transform_pivot_x(tick, width / 2, 0);
    lv_obj_set_style_transform_pivot_y(tick, radius - margin, 0);

    /* LVGL rotation uses tenths of a degree */
    lv_obj_set_style_transform_rotation(tick, angle_deg * 10, 0);

    lv_obj_set_style_bg_color(tick, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tick, width / 2, 0);
    eos_ww_make_static(tick);
    return tick;
}

lv_obj_t *eos_ww_tick_ring_create(lv_obj_t *parent,
                                  lv_coord_t radius,
                                  uint32_t tick_count,
                                  uint32_t major_every,
                                  lv_coord_t margin,
                                  lv_coord_t major_len,
                                  lv_coord_t minor_len,
                                  lv_coord_t major_width,
                                  lv_coord_t minor_width,
                                  uint32_t major_color,
                                  uint32_t minor_color)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);
    if (radius <= 0 || tick_count == 0)
    {
        return NULL;
    }

    lv_obj_t *ring = lv_obj_create(parent);
    lv_obj_set_size(ring, radius * 2, radius * 2);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    eos_ww_make_static(ring);

    for (uint32_t i = 0; i < tick_count; i++)
    {
        int32_t angle_deg = (int32_t)(i * 360 / tick_count);
        bool major = (major_every > 0 && (i % major_every) == 0);

        if (major)
        {
            _create_tick(ring, radius, margin, angle_deg, major_width, major_len, major_color);
        }
        else
        {
            _create_tick(ring, radius, margin, angle_deg, minor_width, minor_len, minor_color);
        }
    }

    return ring;
}
