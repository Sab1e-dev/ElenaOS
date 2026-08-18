/**
 * @file eos_ww_center_cap.h
 * @brief Watchface centre cap (concentric dots over the hand pivot)
 */

#ifndef EOS_WW_CENTER_CAP_H
#define EOS_WW_CENTER_CAP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a two-layer centre cap over the hand pivot
 * @param parent Parent object (typically the circular watch face)
 * @param outer_d Outer ring diameter
 * @param inner_d Inner dot diameter
 * @param outer_color Outer ring colour (0xRRGGBB)
 * @param inner_color Inner dot colour (0xRRGGBB)
 * @return Transparent container holding the cap (NULL on failure)
 */
lv_obj_t *eos_ww_center_cap_create(lv_obj_t *parent,
                                   lv_coord_t outer_d,
                                   lv_coord_t inner_d,
                                   uint32_t outer_color,
                                   uint32_t inner_color);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_CENTER_CAP_H */
