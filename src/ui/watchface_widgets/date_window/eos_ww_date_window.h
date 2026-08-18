/**
 * @file eos_ww_date_window.h
 * @brief Watchface date window (weekday above day-of-month)
 */

#ifndef EOS_WW_DATE_WINDOW_H
#define EOS_WW_DATE_WINDOW_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a stacked date window (weekday above day-of-month)
 * @param parent Parent object
 * @return Container object (NULL on failure)
 */
lv_obj_t *eos_ww_date_window_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_DATE_WINDOW_H */
