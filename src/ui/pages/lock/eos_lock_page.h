/**
 * @file eos_lock_page.h
 * @brief Lock screen security barrier on lv_layer_top()
 */

#ifndef EOS_LOCK_PAGE_H
#define EOS_LOCK_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdbool.h>
#include "lvgl.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Show lock screen security barrier on lv_layer_top()
 */
void eos_lock_page_show(void);

/**
 * @brief Hide and destroy lock screen security barrier
 */
void eos_lock_page_hide(void);

/**
 * @brief Check if lock screen is currently visible
 * @return true if lock screen is showing
 */
bool eos_lock_page_is_visible(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_LOCK_PAGE_H */
