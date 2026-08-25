/**
 * @file eos_ww_battery_arc.h
 * @brief Watchface battery arc (ring + percent, colour-coded by level)
 */

#ifndef EOS_WW_BATTERY_ARC_H
#define EOS_WW_BATTERY_ARC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a battery arc ring with a centred percentage label
 * @param parent Parent object
 * @param size Ring diameter in pixels
 * @param track_color Track (background) colour (0xRRGGBB)
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_battery_arc_create(lv_obj_t *parent, lv_coord_t size, uint32_t track_color);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_BATTERY_ARC_H */
