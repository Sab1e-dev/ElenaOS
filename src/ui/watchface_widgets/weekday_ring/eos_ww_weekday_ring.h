/**
 * @file eos_ww_weekday_ring.h
 * @brief Watchface weekday strip (7 labels, today highlighted)
 */

#ifndef EOS_WW_WEEKDAY_RING_H
#define EOS_WW_WEEKDAY_RING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a weekday strip of 7 abbreviations with today highlighted
 * @param parent Parent object
 * @param idle_color Colour of inactive days (0xRRGGBB)
 * @param active_color Colour of the current day (0xRRGGBB)
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_weekday_ring_create(lv_obj_t *parent, uint32_t idle_color, uint32_t active_color);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_WEEKDAY_RING_H */
