/**
 * @file eos_ww_heart_rate.h
 * @brief Watchface heart rate indicator (icon + BPM)
 */

#ifndef EOS_WW_HEART_RATE_H
#define EOS_WW_HEART_RATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a heart rate indicator (icon + BPM)
 * @param parent Parent object
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_heart_rate_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_HEART_RATE_H */
