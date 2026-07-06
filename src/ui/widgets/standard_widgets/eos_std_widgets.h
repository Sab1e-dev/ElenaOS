/**
 * @file eos_std_widgets.h
 * @brief System standard widgets
 */

#ifndef EOS_STD_WIDGETS_H
#define EOS_STD_WIDGETS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief Create info page
 * @param scr Screen object
 * @param icon_bg_color Circular icon background color
 * @param icon Icon source
 * @param title_txt Title text
 * @param txt Content text
 * @return lv_obj_t* Returns message page container list
 */
lv_obj_t *eos_std_info_create(lv_obj_t *scr,
                              lv_color_t icon_bg_color,
                              const char *icon,
                              const char *title_txt,
                              const char *txt);

/**
 * @brief Create title and comment combination
 * @param parent Parent object
 * @param title Title content
 * @param comment Comment content
 */
void eos_std_title_comment_create(lv_obj_t *parent, const char *title, const char *comment);
#ifdef __cplusplus
}
#endif

#endif /* EOS_STD_WIDGETS_H */
