/**
 * @file sni_api_lv.h
 * @brief LVGL API export
 */

#ifndef SNI_API_LV_H
#define SNI_API_LV_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "jerryscript.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize LVGL API
 */
void sni_api_lv_init(void);

/**
 * @brief Mount LVGL API to specified Realm
 * @param realm Target Realm value
 */
void sni_api_lv_mount(jerry_value_t realm);

#ifdef __cplusplus
}
#endif

#endif /* SNI_API_LV_H */
