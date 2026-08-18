/**
 * @file eos_ww_compass.h
 * @brief Watchface compass (magnetometer-driven needle)
 */

#ifndef EOS_WW_COMPASS_H
#define EOS_WW_COMPASS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a compass with a magnetometer-driven needle
 * @param parent Parent object
 * @param size Compass diameter in pixels
 * @param needle_color Needle colour (0xRRGGBB)
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_compass_create(lv_obj_t *parent, lv_coord_t size, uint32_t needle_color);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_COMPASS_H */
