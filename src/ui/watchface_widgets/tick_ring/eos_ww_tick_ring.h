/**
 * @file eos_ww_tick_ring.h
 * @brief Watchface tick ring (radial tick marks)
 */

#ifndef EOS_WW_TICK_RING_H
#define EOS_WW_TICK_RING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include "lvgl.h"
/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a radial ring of tick marks around the parent centre
 * @param parent Parent object (typically the circular watch face)
 * @param radius Face radius in pixels (half the face size)
 * @param tick_count Total number of ticks (e.g. 60)
 * @param major_every Every Nth tick is drawn as a major tick (e.g. 5)
 * @param margin Distance from the outer edge to the tick outer end
 * @param major_len Major tick length
 * @param minor_len Minor tick length
 * @param major_width Major tick width
 * @param minor_width Minor tick width
 * @param major_color Major tick colour (0xRRGGBB)
 * @param minor_color Minor tick colour (0xRRGGBB)
 * @return Transparent container holding the ticks (NULL on failure)
 */
lv_obj_t *eos_ww_tick_ring_create(lv_obj_t *parent,
                                  lv_coord_t radius,
                                  uint32_t tick_count,
                                  uint32_t major_every,
                                  lv_coord_t margin,
                                  lv_coord_t major_len,
                                  lv_coord_t minor_len,
                                  lv_coord_t major_width,
                                  lv_coord_t minor_width,
                                  uint32_t major_color,
                                  uint32_t minor_color);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_TICK_RING_H */
