/**
 * @file eos_ww_progress_ring.h
 * @brief Watchface progress ring (thin full-circle arc)
 */

#ifndef EOS_WW_PROGRESS_RING_H
#define EOS_WW_PROGRESS_RING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a progress ring (arc) with a 0..100 range
 * @param parent Parent object
 * @return Arc object (NULL on failure)
 */
lv_obj_t *eos_ww_progress_ring_create(lv_obj_t *parent);

/**
 * @brief Set the progress ring value
 * @param arc Progress ring object
 * @param value Value within the configured range
 */
void eos_ww_progress_ring_set_value(lv_obj_t *arc, int32_t value);

/**
 * @brief Set the progress ring range
 * @param arc Progress ring object
 * @param min Minimum value
 * @param max Maximum value
 */
void eos_ww_progress_ring_set_range(lv_obj_t *arc, int32_t min, int32_t max);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_PROGRESS_RING_H */
