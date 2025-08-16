/**
 * @file elena_os_lang.c
 * @brief 多语言系统
 * @author Sab1e
 * @date 2025-08-14
 */

#include "elena_os_lang.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
// Macros and Definitions
#define STR_ID_MAX 10
// Variables
const char **current_lang = NULL;       // 当前语言指针，指向当前语言的数组
uint32_t EVENT_LANG_CHANGED = 0;        // LVGL 自定义事件：语言已更改
/**
 * @brief 英文语言数组
 * @note 在此处新增字符串
 */
const char *lang_en[STR_ID_MAX] = {
    [STR_ID_OK] = "OK",
    [STR_ID_CANCEL] = "Cancel",
    [STR_ID_MSG_LIST_CLEAR_ALL] = "Clear All",
    [STR_ID_MSG_LIST_NO_MSG] = "No notifications",
    [STR_ID_MSG_LIST_ITEM_MARK_AS_READ] = "Mark as read"};
/**
 * @brief 中文语言数组
 * @note 在此处新增字符串
 */
const char *lang_zh[STR_ID_MAX] = {
    [STR_ID_OK] = "确定",
    [STR_ID_CANCEL] = "取消",
    [STR_ID_MSG_LIST_CLEAR_ALL] = "全部清除",
    [STR_ID_MSG_LIST_NO_MSG] = "没有消息",
    [STR_ID_MSG_LIST_ITEM_MARK_AS_READ] = "标记为已读"};
// Function Implementations
void eos_lang_init(void)
{
    EVENT_LANG_CHANGED = lv_event_register_id();
    current_lang = lang_en;     // 默认英语
}
void eos_lang_set(language_id_t lang)
{
    if (EVENT_LANG_CHANGED == 0)
    {
        return;
    }
    switch (lang)
    {
    case LANG_EN:
        current_lang = lang_en;
        break;
    case LANG_ZH:
        current_lang = lang_zh;
        break;
    }
    lv_obj_t *scr = lv_scr_act();
    lv_obj_send_event(scr, EVENT_LANG_CHANGED, NULL);
}
static void lang_event_cb(lv_event_t *e)
{
    lv_obj_t *label = lv_event_get_target(e);
    uint32_t id = (uint32_t)lv_event_get_user_data(e);
    lv_label_set_text(label, current_lang[id]);
}
lv_obj_t *eos_lang_label_create(lv_obj_t *parent, uint32_t str_id)
{
    if (EVENT_LANG_CHANGED == 0)
    {
        return NULL;
    }
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_add_event_cb(label, lang_event_cb, EVENT_LANG_CHANGED, (void *)str_id);
    lv_label_set_text(label, current_lang[str_id]);
    return label;
}
