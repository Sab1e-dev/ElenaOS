/**
 * @file eos_ww_center_cap.c
 * @brief Watchface centre cap (concentric dots over the hand pivot)
 */

#include "eos_ww_center_cap.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#define EOS_LOG_TAG "CenterCap"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

lv_obj_t *eos_ww_center_cap_create(lv_obj_t *parent,
                                   lv_coord_t outer_d,
                                   lv_coord_t inner_d,
                                   uint32_t outer_color,
                                   uint32_t inner_color)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);
    if (outer_d <= 0 || inner_d <= 0)
    {
        return NULL;
    }

    lv_obj_t *cap = lv_obj_create(parent);
    lv_obj_set_size(cap, outer_d, outer_d);
    lv_obj_set_style_bg_opa(cap, LV_OPA_TRANSP, 0);
    eos_ww_make_static(cap);

    lv_obj_t *outer = lv_obj_create(cap);
    lv_obj_set_size(outer, outer_d, outer_d);
    lv_obj_set_style_bg_color(outer, lv_color_hex(outer_color), 0);
    lv_obj_set_style_bg_opa(outer, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(outer, outer_d / 2, 0);
    eos_ww_make_static(outer);

    lv_obj_t *inner = lv_obj_create(cap);
    lv_obj_set_size(inner, inner_d, inner_d);
    lv_obj_center(inner);
    lv_obj_set_style_bg_color(inner, lv_color_hex(inner_color), 0);
    lv_obj_set_style_bg_opa(inner, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(inner, inner_d / 2, 0);
    eos_ww_make_static(inner);

    return cap;
}
