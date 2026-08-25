/**
 * @file eos_watchface_list.h
 * @brief Watchface list
 */

#ifndef EOS_WATCHFACE_LIST_H
#define EOS_WATCHFACE_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_core.h"
#include "eos_activity.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Enter watchface list page
 * @return eos_activity_t* Watchface list page Activity
 */
void eos_watchface_list_enter(void);
#ifdef __cplusplus
}
#endif

#endif /* EOS_WATCHFACE_LIST_H */
