/**
 * @file eos_ww_progress_ring.c
 * @brief Watchface progress ring (thin full-circle arc)
 */

#include "eos_ww_progress_ring.h"

/* Includes ---------------------------------------------------*/
#define EOS_LOG_TAG "ProgressRing"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

lv_obj_t *eos_ww_progress_ring_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, 56, 56);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_rotation(arc, 0);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_MAIN);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    return arc;
}

void eos_ww_progress_ring_set_value(lv_obj_t *arc, int32_t value)
{
    EOS_CHECK_PTR_RETURN(arc);
    lv_arc_set_value(arc, value);
}

void eos_ww_progress_ring_set_range(lv_obj_t *arc, int32_t min, int32_t max)
{
    EOS_CHECK_PTR_RETURN(arc);
    lv_arc_set_range(arc, min, max);
}
