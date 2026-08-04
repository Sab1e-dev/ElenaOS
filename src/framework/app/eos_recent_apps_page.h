/**
 * @file eos_recent_apps_page.h
 * @brief Recent Apps page Activity — card grid for quick app switching
 */

#ifndef EOS_RECENT_APPS_PAGE_H
#define EOS_RECENT_APPS_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_core.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Enter the Recent Apps page
 * @note Creates the recents Activity, registers animation routes, and enters it.
 *       If no suspended apps exist, shows an empty state with a message.
 */
void eos_recent_apps_page_enter(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_RECENT_APPS_PAGE_H */
