/**
 * @file eos_ww_date.h
 * @brief Watchface date (weekday + month/day)
 */

#ifndef EOS_WW_DATE_H
#define EOS_WW_DATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a date widget showing e.g. "WED 08/18"
 * @param parent Parent object
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_date_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_DATE_H */
