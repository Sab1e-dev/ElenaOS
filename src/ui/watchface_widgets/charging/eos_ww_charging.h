/**
 * @file eos_ww_charging.h
 * @brief Watchface charging indicator (shown only while charging)
 */

#ifndef EOS_WW_CHARGING_H
#define EOS_WW_CHARGING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a charging indicator that is hidden while not charging
 * @param parent Parent object
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_charging_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_CHARGING_H */
