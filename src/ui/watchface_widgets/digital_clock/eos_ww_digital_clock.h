/**
 * @file eos_ww_digital_clock.h
 * @brief Watchface digital clock (HH:MM:SS)
 */

#ifndef EOS_WW_DIGITAL_CLOCK_H
#define EOS_WW_DIGITAL_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a digital clock showing the current time (HH:MM:SS)
 * @param parent Parent object
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_digital_clock_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_DIGITAL_CLOCK_H */
