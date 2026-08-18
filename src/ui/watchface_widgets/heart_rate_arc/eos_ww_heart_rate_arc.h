/**
 * @file eos_ww_heart_rate_arc.h
 * @brief Watchface heart rate arc (ring + BPM, colour-zoned)
 */

#ifndef EOS_WW_HEART_RATE_ARC_H
#define EOS_WW_HEART_RATE_ARC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a heart rate arc ring with a centred BPM label
 * @param parent Parent object
 * @param size Ring diameter in pixels
 * @param track_color Track (background) colour (0xRRGGBB)
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_heart_rate_arc_create(lv_obj_t *parent, lv_coord_t size, uint32_t track_color);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_HEART_RATE_ARC_H */
