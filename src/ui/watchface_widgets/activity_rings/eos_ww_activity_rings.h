/**
 * @file eos_ww_activity_rings.h
 * @brief Watchface activity rings (move / exercise / stand)
 */

#ifndef EOS_WW_ACTIVITY_RINGS_H
#define EOS_WW_ACTIVITY_RINGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create three nested activity rings with a centred step count
 * @param parent Parent object
 * @param size Outer ring diameter in pixels
 * @param track_color Track (background) colour (0xRRGGBB)
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_activity_rings_create(lv_obj_t *parent, lv_coord_t size, uint32_t track_color);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_ACTIVITY_RINGS_H */
