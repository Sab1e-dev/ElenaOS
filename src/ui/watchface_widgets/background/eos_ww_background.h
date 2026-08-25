/**
 * @file eos_ww_background.h
 * @brief Watchface solid background helper
 */

#ifndef EOS_WW_BACKGROUND_H
#define EOS_WW_BACKGROUND_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a solid rounded-rectangle background
 * @param parent Parent object
 * @param width Background width
 * @param height Background height
 * @param radius Corner radius (0 for square)
 * @param color Background colour (0xRRGGBB)
 * @return Background object (NULL on failure)
 */
lv_obj_t *eos_ww_background_create(lv_obj_t *parent,
                                   lv_coord_t width,
                                   lv_coord_t height,
                                   lv_coord_t radius,
                                   uint32_t color);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_BACKGROUND_H */
