/**
 * @file eos_ww_numeral_ring.c
 * @brief Watchface numeral ring (radial numeric markers)
 */

#include "eos_ww_numeral_ring.h"

/* Includes ---------------------------------------------------*/
#include <math.h>
#include <stdio.h>
#include "eos_ww_common.h"
#include "eos_font.h"
#define EOS_LOG_TAG "NumeralRing"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

lv_obj_t *eos_ww_numeral_ring_create(lv_obj_t *parent,
                                     lv_coord_t radius,
                                     uint8_t count,
                                     lv_coord_t digit_radius,
                                     lv_coord_t digit_size,
                                     uint32_t color)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);
    if (radius <= 0 || count == 0)
    {
        return NULL;
    }

    lv_obj_t *ring = lv_obj_create(parent);
    lv_obj_set_size(ring, radius * 2, radius * 2);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    eos_ww_make_static(ring);

    char buf[8];
    for (uint8_t i = 1; i <= count; i++)
    {
        /* Clock angle (0° = 12h, clockwise) -> math angle (0° = 3h, CCW) */
        double angle_deg = i * 360.0 / count;
        double rad = (angle_deg - 90.0) * M_PI / 180.0;
        lv_coord_t ox = (lv_coord_t)llround(cos(rad) * digit_radius);
        lv_coord_t oy = (lv_coord_t)llround(sin(rad) * digit_radius);

        lv_obj_t *label = lv_label_create(ring);
        lv_obj_set_size(label, digit_size, digit_size);
        lv_obj_align(label, LV_ALIGN_CENTER, ox, oy);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
        eos_label_set_font_size(label, EOS_FONT_SIZE_SMALL);
        snprintf(buf, sizeof(buf), "%u", i);
        lv_label_set_text(label, buf);
        eos_ww_make_static(label);
    }

    return ring;
}
