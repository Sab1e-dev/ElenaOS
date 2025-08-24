/**
 * @file script_engine_nav.h
 * @brief 脚本引擎的导航栈
 * @author Sab1e
 * @date 2025-08-24
 */

#ifndef SCRIPT_ENGINE_NAV_H
#define SCRIPT_ENGINE_NAV_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "script_engine_core.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/
/**
 * @brief 初始化导航栈
 * @param scr 要初始化的目标 screen
 * @note 此 scr 将会放在栈底，作为根页面（root screen），设置后无法修改。
 */
script_engine_result_t script_engine_nav_init(lv_obj_t *base_scr);
/**
 * @brief 创建新页面并压入导航栈
 * @return lv_obj_t* 创建成功则返回 scr 指针，失败则返回 NULL
 */
lv_obj_t *script_engine_nav_scr_create(void);
/**
 * @brief 返回上一页面并立即销毁 screen 对象
 */
script_engine_result_t script_engine_nav_back_clean(void);
/**
 * @brief 仅返回上一级，不销毁 screen 对象
 * @warning 在不需要 screen 时，需要手动调用`lv_obj_del`清除 screen，否则可能导致内存泄漏。
 */
script_engine_result_t script_engine_nav_back(void);
/**
 * @brief 清理导航栈
 */
void script_engine_nav_clean_up();
/**
 * @brief 判断脚本导航栈是否已经初始化
 * @return true 是
 * @return false 否
 */
bool is_script_nav_stack_initialized(void);
#ifdef __cplusplus
}
#endif

#endif /* SCRIPT_ENGINE_NAV_H */
