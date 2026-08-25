/**
 * @file eos_ww_spo2.h
 * @brief Watchface SpO2 indicator (icon + percent)
 */

#ifndef EOS_WW_SPO2_H
#define EOS_WW_SPO2_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a SpO2 indicator (icon + percent)
 * @param parent Parent object
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_spo2_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_SPO2_H */
