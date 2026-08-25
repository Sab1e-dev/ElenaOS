/**
 * @file eos_ww_barometer.h
 * @brief Watchface barometer indicator (hPa)
 */

#ifndef EOS_WW_BAROMETER_H
#define EOS_WW_BAROMETER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a barometer indicator (hPa)
 * @param parent Parent object
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_barometer_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_BAROMETER_H */
