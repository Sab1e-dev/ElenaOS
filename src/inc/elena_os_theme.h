/**
 * @file elena_os_theme.h
 * @brief 主题色
 * @author Sab1e
 * @date 2025-08-27
 */

#ifndef ELENA_OS_THEME_H
#define ELENA_OS_THEME_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/
#define EOS_THEME_PRIMARY_COLOR   lv_color_hex(0x0066ff)
#define EOS_THEME_SECONDARY_COLOR lv_color_hex(0x111220)
#define EOS_THEME_DANGEROS_COLOR     lv_color_hex(0xe84545)

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/
/**
 * @brief 设置系统主题色
 * @param primary_color 主色
 * @param secondary_color 副色
 * @param font 字体
 */
void eos_theme_set(lv_color_t primary_color, lv_color_t secondary_color, const lv_font_t *font);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_THEME_H */
