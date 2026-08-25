/**
 * @file eos_ww_moon_phase.h
 * @brief Watchface moon phase indicator
 */

#ifndef EOS_WW_MOON_PHASE_H
#define EOS_WW_MOON_PHASE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a moon phase widget showing the illuminated fraction
 * @param parent Parent object
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_moon_phase_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_MOON_PHASE_H */
