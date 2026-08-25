/**
 * @file eos_radio_page.h
 * @brief Radio page - single-selection list page
 */

#ifndef EOS_RADIO_PAGE_H
#define EOS_RADIO_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

#define EOS_INVALID_RADIO_INDEX UINT32_MAX /**< Invalid index value */

/* Public typedefs --------------------------------------------*/

typedef struct eos_radio_page_t eos_radio_page_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Add new option to radio list
 *
 * Option index values increment from 0, for example:
 *
 * ```c
 * eos_radio_page_t *rp = eos_radio_page_create("Title")
 * eos_radio_page_add_item(rp,"Option 1");     // index = 0
 * eos_radio_page_add_item(rp,"Option 2");     // index = 1
 * eos_radio_page_add_item(rp,"Option 3");     // index = 2
 * eos_radio_page_show(rp);                    // enter activity
 * ```
 * @param rp Radio page pointer
 * @param txt Option text string
 * @return uint32_t Returns index value of the radio option on success, otherwise returns `EOS_INVALID_RADIO_INDEX`
 */
uint32_t eos_radio_page_add_item(eos_radio_page_t *rp, const char *txt);

/**
 * @brief Add subtitle to the top of radio page
 * @param subtitle Subtitle string
 */
void eos_radio_page_set_subtitle(eos_radio_page_t *rp, const char *subtitle);

/**
 * @brief Add comment to the bottom of radio page
 * @param comment Comment string
 */
void eos_radio_page_set_comment(eos_radio_page_t *rp, const char *comment);

/**
 * @brief Add callback function for radio page selection
 * @param event_cb Standard `lv_event_cb_t`, use `lv_event_get_param()` to get the selected item's index (0-based)
 * @param user_data User data
 */
void eos_radio_page_add_event_cb(eos_radio_page_t *rp, lv_event_cb_t event_cb, void *user_data);

/**
 * @brief Create new radio page (without entering activity)
 *
 * After creating the page, add items via eos_radio_page_add_item(),
 * then call eos_radio_page_show() to enter the activity.
 * @param title Title string
 * @return eos_radio_page_t* Returns radio page pointer on success, NULL on failure
 */
eos_radio_page_t *eos_radio_page_create(const char *title);

/**
 * @brief Show/enter the radio page activity
 *
 * Must be called after all items are added via eos_radio_page_add_item().
 * @param rp Radio page pointer
 */
void eos_radio_page_show(eos_radio_page_t *rp);

/**
 * @brief Check the radio option at specified index
 * @param index Index of the target radio option
 */
void eos_radio_page_check(eos_radio_page_t *rp, uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* EOS_RADIO_PAGE_H */
