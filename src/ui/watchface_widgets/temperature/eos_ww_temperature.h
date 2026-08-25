/**
 * @file eos_ww_temperature.h
 * @brief Watchface temperature indicator (icon + °C)
 */

#ifndef EOS_WW_TEMPERATURE_H
#define EOS_WW_TEMPERATURE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a temperature indicator (icon + °C)
 * @param parent Parent object
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_temperature_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_TEMPERATURE_H */
