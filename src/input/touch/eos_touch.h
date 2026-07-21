/**
 * @file eos_touch.h
 * @brief Get touch device
 */

#ifndef EOS_TOUCH_H
#define EOS_TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/
/**
 * @brief Get the first touch device
 * @return lv_indev_t* Returns touch device pointer if successful, otherwise returns NULL
 */
lv_indev_t *eos_touch_get_indev(void);
#ifdef __cplusplus
}
#endif

#endif /* EOS_TOUCH_H */
