/**
 * @file elena_os_config.h
 * @brief 系统配置
 * @author Sab1e
 * @date 2025-09-13
 */

#ifndef ELENA_OS_CONFIG_H
#define ELENA_OS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "elena_os_sys.h"

/************************** 配置开始 **************************/

/************************** 字体配置 **************************/
/**
 * @brief 使用标准的 TTF 格式
 * @param EOS_FONT_TTF_PATH TTF文件路径
 */
// #define EOS_FONT_USE_TTF
// #define EOS_FONT_TTF_PATH        EOS_SYS_RES_FONT_DIR "font.ttf"

/**
 * @brief 使用 LVGL Font Converter 生成的 C 文字字库
 * @param EOS_FONT_C_NAME 字库名称
 */
// #define EOS_FONT_USE_C
// #define EOS_FONT_C_NAME          eos_font_resource_han_rounded_30

/************************** 配置结束 **************************/

#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_CONFIG_H */
