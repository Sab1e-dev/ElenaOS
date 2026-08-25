/**
 * @file eos_ww_numeral_ring.h
 * @brief Watchface numeral ring (radial numeric markers)
 */

#ifndef EOS_WW_NUMERAL_RING_H
#define EOS_WW_NUMERAL_RING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a ring of numerals (1..count) around the parent centre
 * @param parent Parent object (typically the circular watch face)
 * @param radius Face radius in pixels
 * @param count Number of numerals (commonly 12 or 24)
 * @param digit_radius Distance from centre to each numeral centre
 * @param digit_size Size of the square box hosting each numeral
 * @param color Numeral colour (0xRRGGBB)
 * @return Transparent container holding the numerals (NULL on failure)
 */
lv_obj_t *eos_ww_numeral_ring_create(lv_obj_t *parent,
                                     lv_coord_t radius,
                                     uint8_t count,
                                     lv_coord_t digit_radius,
                                     lv_coord_t digit_size,
                                     uint32_t color);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_NUMERAL_RING_H */
