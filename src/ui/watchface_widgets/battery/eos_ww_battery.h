/**
 * @file eos_ww_battery.h
 * @brief Watchface battery indicator (icon + percent)
 */

#ifndef EOS_WW_BATTERY_H
#define EOS_WW_BATTERY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a battery indicator (icon + percentage)
 * @param parent Parent object
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_battery_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_BATTERY_H */
