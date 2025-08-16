/*
 * @file       elena_os_lang.h
 * @brief      多语言系统
 * @author     Sab1e
 * @date       2025-08-14
 */

#ifndef ELENA_OS_LANG_H
#define ELENA_OS_LANG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
/**
 * @brief 字符串索引
 * 
 * @note 此处可添加新的字符串索引
 */
typedef enum {
    STR_ID_OK,
    STR_ID_CANCEL,
    STR_ID_MSG_LIST_CLEAR_ALL,
    STR_ID_MSG_LIST_NO_MSG,
    STR_ID_MSG_LIST_ITEM_MARK_AS_READ,
} string_id_t;
/**
 * @brief 语言类型
 * 
 */
typedef enum{
    LANG_EN,
    LANG_ZH
} language_id_t;
/* Public function prototypes --------------------------------*/
extern const char** current_lang;
/**
 * @brief 初始化语言系统
 * 
 */
void eos_lang_init(void);
/**
 * @brief 设置当前语言
 * 
 * @param lang 目标语言类型 `language_id_t`
 * @note 需要先初始化语言系统 `
 */
void eos_lang_set(language_id_t lang);
/**
 * @brief 创建一个支持多语言的 LVGL label
 * 
 * @param parent label 的父级 LVGL 对象
 * @param str_id 字符串 ID
 * @return lv_obj_t* 返回创建的 label
 */
lv_obj_t * eos_lang_label_create(lv_obj_t * parent, uint32_t str_id);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_LANG_H */
