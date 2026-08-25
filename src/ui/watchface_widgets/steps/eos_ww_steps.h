/**
 * @file eos_ww_steps.h
 * @brief Watchface step counter (icon + count)
 */

#ifndef EOS_WW_STEPS_H
#define EOS_WW_STEPS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a step counter (icon + step count)
 * @param parent Parent object
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_steps_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_STEPS_H */
