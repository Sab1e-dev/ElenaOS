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
#include "elena_os_event.h"
#include "elena_os_log.h"
#include "lvgl.h"

// Macros and Definitions
/**
 * @brief 英文语言数组
 * @note 在此处新增字符串
 */
const char *lang_en[STR_ID_MAX_NUMBER] = {
    [STR_ID_LANGUAGE] = "English",
    [STR_ID_OK] = "OK",
    [STR_ID_CANCEL] = "Cancel",
    [STR_ID_MSG_LIST_CLEAR_ALL] = "Clear all",
    [STR_ID_MSG_LIST_NO_MSG] = "No notifications",
    [STR_ID_MSG_LIST_ITEM_MARK_AS_READ] = "Mark as read",
    [STR_ID_BASE_ITEM_BACK] = LV_SYMBOL_LEFT "Back",
    [STR_ID_TEST_LANG_STR] = "On a late-summer afternoon, the wind set the chimes on the balcony jingling, like some unintentional signal.",
    [STR_ID_SCRIPT_RUN_ERR] = "The script encountered a fatal error during runtime",
    [STR_ID_SETTINGS] = "Settings",
    [STR_ID_SETTINGS_BLUETOOTH] = "Bluetooth",
    [STR_ID_SETTINGS_BLUETOOTH_ENABLE] = "Enable Bluetooth",
    [STR_ID_SETTINGS_DISPLAY] = "Display",
    [STR_ID_SETTINGS_NOTIFICATION] = "Notification"
    // 在此添加新的字符串ID和英文翻译
};

/**
 * @brief 中文语言数组
 * @note 在此处新增字符串
 */
const char *lang_zh[STR_ID_MAX_NUMBER] = {
    [STR_ID_LANGUAGE] = "简体中文",
    [STR_ID_OK] = "确定",
    [STR_ID_CANCEL] = "取消",
    [STR_ID_MSG_LIST_CLEAR_ALL] = "全部清除",
    [STR_ID_MSG_LIST_NO_MSG] = "没有消息",
    [STR_ID_MSG_LIST_ITEM_MARK_AS_READ] = "标记为已读",
    [STR_ID_BASE_ITEM_BACK] = LV_SYMBOL_LEFT "返回",
    [STR_ID_TEST_LANG_STR] = "在夏末的午后，风把阳台上的风铃吹得叮当作响，像是某种不经意的暗号。",
    [STR_ID_SCRIPT_RUN_ERR] = "脚本在运行时遇到了致命错误",
    [STR_ID_SETTINGS] = "设置",
    [STR_ID_SETTINGS_BLUETOOTH] = "蓝牙",
    [STR_ID_SETTINGS_BLUETOOTH_ENABLE] = "启用蓝牙",
    [STR_ID_SETTINGS_DISPLAY] = "显示",
    [STR_ID_SETTINGS_NOTIFICATION] = "通知"
    // 在此添加新的字符串ID和中文翻译
};

const char **current_lang = NULL;     // 当前语言指针
static bool lang_initialized = false; // 语言系统初始化标志

// 函数声明
static void lang_event_cb(lv_event_t *e);

// Function Implementations
void eos_lang_init(void)
{
    if (!lang_initialized)
    {
        current_lang = lang_en; // 默认英语
        lang_initialized = true;
    }
}

void eos_lang_set(language_id_t lang)
{
    switch (lang)
    {
    case LANG_EN:
        current_lang = lang_en;
        break;
    case LANG_ZH:
        current_lang = lang_zh;
        break;
    }

    // 使用事件广播系统刷新所有标签
    eos_event_broadcast(LV_EVENT_REFRESH, NULL);

    EOS_LOG_D("LANG CHANGED");
}

language_id_t eos_lang_get(void)
{
    if (current_lang == lang_zh)
    {
        return LANG_ZH;
    }
    else if (current_lang == lang_en)
    {
        return LANG_EN;
    }
    else
    {
        return LANG_UNKNOWN;
    }
}

char *eos_lang_get_language_str(void)
{
    return current_lang[STR_ID_LANGUAGE];
}

static void lang_event_cb(lv_event_t *e)
{
    lv_obj_t *label = lv_event_get_target(e);

    // 从用户数据中获取str_id
    uint32_t id = (uint32_t)lv_event_get_user_data(e);
    if (!id)
    {
        EOS_LOG_E("No id for label");
        return;
    }

    if (id < STR_ID_MAX_NUMBER && current_lang && current_lang[id])
    {
        lv_label_set_text(label, current_lang[id]);
    }
}

lv_obj_t *eos_lang_label_create(lv_obj_t *parent, uint32_t str_id)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    // 初始化语言系统（如果未初始化）
    if (!lang_initialized)
    {
        eos_lang_init();
    }

    // 创建标签
    lv_obj_t *label = lv_label_create(parent);
    if (!label)
        return NULL;

    // 设置初始文本
    if (str_id < STR_ID_MAX_NUMBER && current_lang && current_lang[str_id])
    {
        lv_label_set_text(label, current_lang[str_id]);
    }

    // 使用事件系统注册回调
    eos_event_add_cb(label, lang_event_cb, LV_EVENT_REFRESH, (void *)str_id);

    return label;
}
