/**
 * @file elena_os_nav.h
 * @brief 导航栈
 * @author Sab1e
 * @date 2025-08-16
 */

#ifndef ELENA_OS_NAV_H
#define ELENA_OS_NAV_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "elena_os_core.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief 初始化导航栈
 * @param scr 要初始化的目标 screen
 * @note 此 scr 将会放在栈底，作为根页面（root）
 */
ElenaOSResult_t eos_nav_init(lv_obj_t *scr);
/**
 * @brief 创建新页面并压入导航栈
 * @return lv_obj_t* 创建成功则返回 scr 指针，失败则返回 NULL
 */
lv_obj_t *eos_nav_scr_create(void);
/**
 * @brief 安全清理栈当前的 screen 及上方的所有页面
 * @note 用于应用退出时清理
 */
ElenaOSResult_t eos_nav_clear_stack(void);
/**
 * @brief 返回上一页面并立即销毁 screen 对象
 */
ElenaOSResult_t eos_nav_back_clear(void);
/**
 * @brief 仅返回上一级，不销毁 screen 对象
 * @warning 在不需要 screen 时，需要手动调用`lv_obj_del`清除 screen，否则可能导致内存泄漏。
 */
ElenaOSResult_t eos_nav_back(void);

#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_NAV_H */
