/**
 * @file eos_ww_background.c
 * @brief Watchface solid background helper
 */

#include "eos_ww_background.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#define EOS_LOG_TAG "Background"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

lv_obj_t *eos_ww_background_create(lv_obj_t *parent,
                                   lv_coord_t width,
                                   lv_coord_t height,
                                   lv_coord_t radius,
                                   uint32_t color)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    lv_obj_t *bg = lv_obj_create(parent);
    lv_obj_set_size(bg, width, height);
    lv_obj_set_style_bg_color(bg, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bg, radius, 0);
    eos_ww_make_static(bg);
    return bg;
}
