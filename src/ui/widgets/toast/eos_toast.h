/**
 * @file eos_toast.h
 * @brief Temporary message toast
 */

#ifndef EOS_TOAST_H
#define EOS_TOAST_H

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
 * @brief Show a Toast message
 * @param icon_src Icon file path, set to NULL to not display icon
 * @param message Message content
 * @return lv_obj_t* Returns the created Toast object pointer
 */
lv_obj_t *eos_toast_show(const char *icon_src, const char *message);

/**
 * @brief Show a Toast message with character icon color support
 * @param icon_char Character icon (such as RI_ macro), set to NULL to not display icon
 * @param icon_color Icon color
 * @param message Message content
 * @return lv_obj_t* Returns the created Toast object pointer
 */
lv_obj_t *eos_toast_show_char_icon(const char *icon_char, lv_color_t icon_color, const char *message);
/**
 * @brief Toast message with formatted string support
 */
lv_obj_t *eos_toast_show_fmt(const char *icon_src, const char *fmt, ...);
/**
 * @brief Initialize Toast system
 */
void eos_toast_init(void);
#ifdef __cplusplus
}
#endif

#endif /* EOS_TOAST_H */
