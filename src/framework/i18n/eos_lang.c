/**
 * @file eos_lang.c
 * @brief Multi-language system
 */

#include "eos_lang.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "eos_event.h"
#define EOS_LOG_TAG "Language"
#include "eos_log.h"
#include "lvgl.h"
#include "eos_service_config.h"
#include "eos_icon.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/
/**
 * @brief English language array
 * @note Add strings here as needed
 */
const char *lang_en[STR_ID_MAX_NUMBER] = {
    [STR_ID_LANGUAGE] = "English",
    [STR_ID_ERROR] = "Error",
    [STR_ID_OK] = "OK",
    [STR_ID_DONE] = "Done",
    [STR_ID_CANCEL] = "Cancel",
    [STR_ID_OFF] = "OFF",
    [STR_ID_NORMAL] = "Normal",
    [STR_ID_INTENSE] = "Intense",
    [STR_ID_MSG_LIST_CLEAR_ALL] = "Clear all",
    [STR_ID_MSG_LIST_NO_MSG] = "No notifications",
    [STR_ID_MSG_LIST_ITEM_MARK_AS_READ] = "Mark as read",
    [STR_ID_BACK] = "Back",
    [STR_ID_TEST_LANG_STR] =
        "On a late-summer afternoon, the wind set the chimes on the balcony jingling, like some unintentional signal.",
    [STR_ID_APP_RUN_ERR_TITLE] = "Application Stopped",
    [STR_ID_APP_RUN_ERR] = "The application encountered a critical error and cannot continue running. Please report "
                           "this issue to the developer.",
    [STR_ID_APP_RUN_ERR_BACKTRACE] = "Backtrace",
    [STR_ID_WATCHFACE_RUN_ERR_TITLE] = "Watchface Stopped",
    [STR_ID_WATCHFACE_RUN_ERR] = "The watchface encountered a critical error and cannot continue running. Please "
                                 "report this issue to the developer.",
    [STR_ID_WATCHFACE_SWITCH] = "Switch Watch Face",
    [STR_ID_SETTINGS] = "Settings",
    [STR_ID_SETTINGS_BLUETOOTH] = "Bluetooth",
    [STR_ID_SETTINGS_BLUETOOTH_ENABLE] = "Bluetooth",
    [STR_ID_SETTINGS_DISPLAY] = "Display",
    [STR_ID_SETTINGS_DISPLAY_BRIGHTNESS] = "Brightness",
    [STR_ID_SETTINGS_DISPLAY_AOD] = "AOD",
    [STR_ID_SETTINGS_DISPLAY_AOD_COMMENT] = "Elenix Watch can show the time at all times.",
    [STR_ID_SETTINGS_WAKE] = "Wake",
    [STR_ID_SETTINGS_WAKE_DURATION] = "Wake Duration",
    [STR_ID_SETTINGS_WAKE_ON_RAISE] = "Wake on raise",
    [STR_ID_SETTINGS_WAKE_FOR_N_SECONDS] = "Wake for %d seconds",
    [STR_ID_SETTINGS_WAKE_ON_TAP] = "On tap",
    [STR_ID_SETTINGS_WAKE_ON_TAP_COMMENT] = "Choose 'Tap to Wake' on the Elenix Watch, then set the wake duration.",
    [STR_ID_SETTINGS_HAPTICS_STRENGTH] = "Haptics strength",
    [STR_ID_SETTINGS_HAPTICS] = "Haptics",
    [STR_ID_SETTINGS_NOTIFICATION] = "Notification",
    [STR_ID_SETTINGS_APPS] = "Apps",
    [STR_ID_SETTINGS_APPS_DETAILS] = "App Details",
    [STR_ID_SETTINGS_APPS_APPID] = "ID",
    [STR_ID_SETTINGS_APPS_AUTHOR] = "Author",
    [STR_ID_SETTINGS_APPS_VERSION] = "Version",
    [STR_ID_SETTINGS_APPS_DESCRIPTON] = "Description",
    [STR_ID_SETTINGS_APPS_UNINSTALL] = "Uninstall",
    [STR_ID_SETTINGS_APPS_UNINSTALL_MSG] = "Deleting this app will also remove all of its data.",
    [STR_ID_SETTINGS_APPS_CLEAR_DATA] = "Clear data",
    [STR_ID_SETTINGS_APPS_SDK] = "SDK",
    [STR_ID_SETTINGS_GENERAL] = "General",
    [STR_ID_SETTINGS_GENERAL_LANGUAGE] = "Language",
    [STR_ID_SETTINGS_GENERAL_DEVICE_INFO] = "Device info",
    [STR_ID_SETTINGS_GENERAL_DEVICE_NAME] = "Device name",
    [STR_ID_SETTINGS_GENERAL_EOS_VER] = "ElenixOS version",
    [STR_ID_SETTINGS_GENERAL_MARKETING_NAME] = "Marketing name",
    [STR_ID_SETTINGS_GENERAL_MODEL_NUMBER] = "Model number",
    [STR_ID_SETTINGS_GENERAL_OPEN_SOURCE] = "Open Source",
    [STR_ID_SETTINGS_GENERAL_OPEN_SOURCE_CONTENT] = "ElenixOS is open-sourced on GitHub:\n"
                                                    "https://github.com/\nElenixOS/ElenixOS",
    [STR_ID_SETTINGS_GENERAL_LEGAL_INFO] = "Legal info",
    [STR_ID_SETTINGS_GENERAL_LEGAL_INFO_CONTENT] = "ElenixOS is licensed under the Apache License, Version 2.0.\n"
                                                   "See full license at:\n"
                                                   "https://www.apache.org/\nlicenses/LICENSE-2.0",
    [STR_ID_SETTINGS_SOUNDS_AND_HAPTICS] = "Sounds & Haptics",
    [STR_ID_SETTINGS_SOUNDS_AND_ALERTS] = "Sounds & Alerts",
    [STR_ID_SETTINGS_SOUNDS_AND_HAPTICS_SILENT_MODE] = "Silent Mode",
    [STR_ID_SETTINGS_SOUNDS_AND_HAPTICS_VOLUME] = "Volume",
    [STR_ID_SETTINGS_APPS_CLEAR_DATA_SUCCESS] = "App data cleared",
    [STR_ID_APP_FLASH_LIGHT_DISMISS] = "Dismiss",
    [STR_ID_TOAST_SHOW_MUTE] = "Muted",
    [STR_ID_TOAST_SHOW_UNMUTE] = "Unmuted",
    [STR_ID_SECOND] = "sec",
    [STR_ID_SYS_INIT_FAILED] = "System initialization failed",
    [STR_ID_SYS_INIT_FAILED_CONTENT] = "An fatal error occurred during system initialization. Please restart the "
                                       "device. If the problem persists, contact support.",
    [STR_ID_SENSOR_ACCE] = "Accelerometer",
    [STR_ID_SENSOR_GYRO] = "Gyroscope",
    [STR_ID_SENSOR_MAG] = "Magnetometer",
    [STR_ID_SENSOR_TEMP] = "Temperature Sensor",
    [STR_ID_SENSOR_HUMI] = "Humidity Sensor",
    [STR_ID_SENSOR_BARO] = "Barometer",
    [STR_ID_SENSOR_LIGHT] = "Ambient Light Sensor",
    [STR_ID_SENSOR_PROXIMITY] = "Proximity Sensor",
    [STR_ID_SENSOR_HR] = "Heart Rate Sensor",
    [STR_ID_SENSOR_TVOC] = "TVOC Sensor",
    [STR_ID_SENSOR_NOISE] = "Noise Sensor",
    [STR_ID_SENSOR_STEP] = "Step Counter",
    [STR_ID_SENSOR_FORCE] = "Force Sensor",
    [STR_ID_SENSOR_BAT] = "Battery Sensor",
    [STR_ID_SETTINGS_PASSWORD] = "Password",
    [STR_ID_SETTINGS_PASSWORD_ENABLE] = "Enable Password",
    [STR_ID_SETTINGS_PASSWORD_DISABLE] = "Disable Password",
    [STR_ID_SETTINGS_PASSWORD_CHANGE] = "Change Password",
    [STR_ID_SETTINGS_PASSWORD_SIMPLE] = "Simple Password",
    [STR_ID_SETTINGS_PASSWORD_SIMPLE_COMMENT] = "4-digit numeric passcode. Turn off for 6-digit.",
    [STR_ID_LOCK_SCREEN_ENTER_PASSCODE] = "Enter Passcode",
    [STR_ID_LOCK_SCREEN_WRONG_PASSCODE] = "Wrong Passcode",
    [STR_ID_SETTINGS_PASSWORD_ENTER_NEW] = "Enter New Passcode",
    [STR_ID_SETTINGS_PASSWORD_REENTER] = "Re-enter Passcode",
    [STR_ID_SETTINGS_PASSWORD_MISMATCH] = "Passcodes do not match",
    /* Permission system */
    [STR_ID_PERM_NAME_LOCATION] = "Location",
    [STR_ID_PERM_NAME_SENSOR] = "Sensor",
    [STR_ID_PERM_NAME_NOTIFICATION] = "Notification",
    [STR_ID_PERM_NAME_STORAGE] = "Storage",
    [STR_ID_PERM_NAME_BLUETOOTH] = "Bluetooth",
    [STR_ID_PERM_NAME_AUDIO] = "Audio",
    [STR_ID_PERM_NAME_HEALTH] = "Health",
    [STR_ID_PERM_NAME_CONTACTS] = "Contacts",
    [STR_ID_PERM_NAME_CALENDAR] = "Calendar",
    [STR_ID_PERM_DESC_LOCATION] = "Your location information is used to provide location-based services and features.",
    [STR_ID_PERM_DESC_SENSOR] = "Sensor data is used to detect your motion and environmental state.",
    [STR_ID_PERM_DESC_NOTIFICATION] = "Allow this app to send and manage notifications.",
    [STR_ID_PERM_DESC_STORAGE] = "Allow this app to read and write to storage.",
    [STR_ID_PERM_DESC_BLUETOOTH] = "Allow this app to use Bluetooth for connection and data transfer.",
    [STR_ID_PERM_DESC_AUDIO] = "Allow this app to record audio or control playback.",
    [STR_ID_PERM_DESC_HEALTH] = "Allow this app to read your health data.",
    [STR_ID_PERM_DESC_CONTACTS] = "Allow this app to access your contacts.",
    [STR_ID_PERM_DESC_CALENDAR] = "Allow this app to access your calendar information.",
    [STR_ID_PERM_REQUEST_TITLE] = "Allow \"%s\" to use your %s?",
    [STR_ID_PERM_ALLOW_ONCE] = "Allow Once",
    [STR_ID_PERM_ALLOW_FOREGROUND] = "Allow While Using App",
    [STR_ID_PERM_DENY] = "Don't Allow",
    [STR_ID_PERM_STATE_DENIED] = "Denied",
    [STR_ID_PERM_STATE_ALLOW_ONCE] = "Allowed (Once)",
    [STR_ID_PERM_STATE_ALLOW_FOREGROUND] = "Allowed (Foreground)",
    [STR_ID_PERM_STATE_ALLOW_ALWAYS] = "Always Allowed",
    [STR_ID_PERM_NO_PERMISSIONS] = "No special permissions required",
    [STR_ID_PERM_MANAGEMENT] = "Permission Management",
    [STR_ID_PERM_TITLE] = "Permission",
    [STR_ID_APP_RESTART] = "Restart App",
    [STR_ID_APP_EXIT] = "Exit App",
    [STR_ID_RESTART] = "Restart",
    /* Developer Options */
    [STR_ID_SETTINGS_DEVELOPER_OPTIONS] = "Developer Options",
    [STR_ID_SETTINGS_DEVELOPER_VIEW_LOGS] = "View Logs",
    [STR_ID_SETTINGS_DEVELOPER_FPS] = "FPS Display",
    [STR_ID_SETTINGS_DEVELOPER_MEMORY] = "Memory",
    [STR_ID_SETTINGS_DEVELOPER_MEMORY_USED] = "Used Memory",
    [STR_ID_SETTINGS_DEVELOPER_MEMORY_FREE] = "Free Memory",
    [STR_ID_SETTINGS_DEVELOPER_OBJS] = "OBJS Display",
    [STR_ID_SETTINGS_DEVELOPER_TOUCH] = "Touch Coordinates",
    // Add new string IDs and English translations here as needed
};

/**
 * @brief Chinese language array
 * @note Add new strings here
 */
const char *lang_zh[STR_ID_MAX_NUMBER] = {
    [STR_ID_LANGUAGE] = "简体中文",
    [STR_ID_ERROR] = "出错了",
    [STR_ID_OK] = "确定",
    [STR_ID_DONE] = "完成",
    [STR_ID_CANCEL] = "取消",
    [STR_ID_OFF] = "关闭",
    [STR_ID_NORMAL] = "正常",
    [STR_ID_INTENSE] = "强",
    [STR_ID_MSG_LIST_CLEAR_ALL] = "全部清除",
    [STR_ID_MSG_LIST_NO_MSG] = "没有消息",
    [STR_ID_MSG_LIST_ITEM_MARK_AS_READ] = "标记为已读",
    [STR_ID_BACK] = "返回",
    [STR_ID_TEST_LANG_STR] = "在夏末的午后，风把阳台上的风铃吹得叮当作响，像是某种不经意的暗号。",
    [STR_ID_APP_RUN_ERR_TITLE] = "应用程序已停止运行",
    [STR_ID_APP_RUN_ERR] = "应用程序在运行过程中出现了严重错误，无法继续执行。请将此问题反馈给开发者。",
    [STR_ID_APP_RUN_ERR_BACKTRACE] = "错误回溯信息",
    [STR_ID_WATCHFACE_RUN_ERR_TITLE] = "表盘已停止运行",
    [STR_ID_WATCHFACE_RUN_ERR] = "表盘在运行过程中出现了严重错误，无法继续执行。请将此问题反馈给开发者。",
    [STR_ID_WATCHFACE_SWITCH] = "切换表盘",
    [STR_ID_SETTINGS] = "设置",
    [STR_ID_SETTINGS_BLUETOOTH] = "蓝牙",
    [STR_ID_SETTINGS_BLUETOOTH_ENABLE] = "蓝牙",
    [STR_ID_SETTINGS_DISPLAY] = "显示",
    [STR_ID_SETTINGS_DISPLAY_BRIGHTNESS] = "亮度",
    [STR_ID_SETTINGS_DISPLAY_AOD] = "全天候显示",
    [STR_ID_SETTINGS_DISPLAY_AOD_COMMENT] = "Elenix Watch可始终显示时间。",
    [STR_ID_SETTINGS_WAKE] = "唤醒",
    [STR_ID_SETTINGS_WAKE_DURATION] = "唤醒时长",
    [STR_ID_SETTINGS_WAKE_ON_RAISE] = "抬腕时唤醒",
    [STR_ID_SETTINGS_WAKE_FOR_N_SECONDS] = "唤醒 %d 秒",
    [STR_ID_SETTINGS_WAKE_ON_TAP] = "轻点时",
    [STR_ID_SETTINGS_WAKE_ON_TAP_COMMENT] = "选择轻点唤醒Elenix Watch的屏幕后，保持唤醒的时长。",
    [STR_ID_SETTINGS_HAPTICS_STRENGTH] = "触感强度",
    [STR_ID_SETTINGS_HAPTICS] = "触感",
    [STR_ID_SETTINGS_NOTIFICATION] = "通知",
    [STR_ID_SETTINGS_APPS] = "应用程序",
    [STR_ID_SETTINGS_APPS_DETAILS] = "应用详情",
    [STR_ID_SETTINGS_APPS_APPID] = "ID",
    [STR_ID_SETTINGS_APPS_AUTHOR] = "作者",
    [STR_ID_SETTINGS_APPS_VERSION] = "版本",
    [STR_ID_SETTINGS_APPS_DESCRIPTON] = "描述",
    [STR_ID_SETTINGS_APPS_UNINSTALL] = "卸载",
    [STR_ID_SETTINGS_APPS_UNINSTALL_MSG] = "删除此应用将同时移除其所有数据。",
    [STR_ID_SETTINGS_APPS_CLEAR_DATA] = "删除应用数据",
    [STR_ID_SETTINGS_APPS_SDK] = "SDK",
    [STR_ID_SETTINGS_GENERAL] = "通用",
    [STR_ID_SETTINGS_GENERAL_LANGUAGE] = "语言",
    [STR_ID_SETTINGS_GENERAL_DEVICE_INFO] = "设备信息",
    [STR_ID_SETTINGS_GENERAL_DEVICE_NAME] = "设备名称",
    [STR_ID_SETTINGS_GENERAL_EOS_VER] = "ElenixOS 版本",
    [STR_ID_SETTINGS_GENERAL_MARKETING_NAME] = "型号名称",
    [STR_ID_SETTINGS_GENERAL_MODEL_NUMBER] = "型号",
    [STR_ID_SETTINGS_GENERAL_OPEN_SOURCE] = "开源信息",
    [STR_ID_SETTINGS_GENERAL_OPEN_SOURCE_CONTENT] = "ElenixOS 已经在 GitHub 开源：\n"
                                                    "https://github.com/\nElenixOS/ElenixOS",
    [STR_ID_SETTINGS_GENERAL_LEGAL_INFO] = "法律信息",
    [STR_ID_SETTINGS_GENERAL_LEGAL_INFO_CONTENT] = "ElenixOS 采用 Apache License 2.0 授权。\n"
                                                   "更多信息请查看：\n"
                                                   "https://www.apache.org/\nlicenses/LICENSE-2.0",
    [STR_ID_SETTINGS_SOUNDS_AND_HAPTICS] = "声效与触感反馈",
    [STR_ID_SETTINGS_SOUNDS_AND_ALERTS] = "铃声与提醒",
    [STR_ID_SETTINGS_SOUNDS_AND_HAPTICS_SILENT_MODE] = "静音模式",
    [STR_ID_SETTINGS_SOUNDS_AND_HAPTICS_VOLUME] = "音量",
    [STR_ID_SETTINGS_APPS_CLEAR_DATA_SUCCESS] = "应用数据已清除",
    [STR_ID_APP_FLASH_LIGHT_DISMISS] = "忽略",
    [STR_ID_TOAST_SHOW_MUTE] = "已开启静音",
    [STR_ID_TOAST_SHOW_UNMUTE] = "已关闭静音",
    [STR_ID_SECOND] = "秒",
    [STR_ID_SYS_INIT_FAILED] = "系统初始化失败",
    [STR_ID_SYS_INIT_FAILED_CONTENT] = "系统初始化过程中发生了致命错误，请重启设备或联系开发者。",
    [STR_ID_SENSOR_ACCE] = "加速度传感器",
    [STR_ID_SENSOR_GYRO] = "重力传感器",
    [STR_ID_SENSOR_MAG] = "磁传感器",
    [STR_ID_SENSOR_TEMP] = "温度传感器",
    [STR_ID_SENSOR_HUMI] = "相对湿度传感器",
    [STR_ID_SENSOR_BARO] = "气压传感器",
    [STR_ID_SENSOR_LIGHT] = "环境光传感器",
    [STR_ID_SENSOR_PROXIMITY] = "距离传感器",
    [STR_ID_SENSOR_HR] = "心率传感器",
    [STR_ID_SENSOR_TVOC] = "TVOC传感器",
    [STR_ID_SENSOR_NOISE] = "噪声传感器",
    [STR_ID_SENSOR_STEP] = "计步传感器",
    [STR_ID_SENSOR_FORCE] = "力传感器",
    [STR_ID_SENSOR_BAT] = "电池传感器",
    [STR_ID_SETTINGS_PASSWORD] = "密码",
    [STR_ID_SETTINGS_PASSWORD_ENABLE] = "开启密码",
    [STR_ID_SETTINGS_PASSWORD_DISABLE] = "关闭密码",
    [STR_ID_SETTINGS_PASSWORD_CHANGE] = "更改密码",
    [STR_ID_SETTINGS_PASSWORD_SIMPLE] = "简单密码",
    [STR_ID_SETTINGS_PASSWORD_SIMPLE_COMMENT] = "开启后仅需4位数字密码。关闭后为6位。",
    [STR_ID_LOCK_SCREEN_ENTER_PASSCODE] = "输入密码",
    [STR_ID_LOCK_SCREEN_WRONG_PASSCODE] = "密码错误",
    [STR_ID_SETTINGS_PASSWORD_ENTER_NEW] = "输入新密码",
    [STR_ID_SETTINGS_PASSWORD_REENTER] = "再次输入密码",
    [STR_ID_SETTINGS_PASSWORD_MISMATCH] = "两次输入的密码不一致",
    /* Permission system */
    [STR_ID_PERM_NAME_LOCATION] = "位置信息",
    [STR_ID_PERM_NAME_SENSOR] = "传感器",
    [STR_ID_PERM_NAME_NOTIFICATION] = "通知",
    [STR_ID_PERM_NAME_STORAGE] = "存储",
    [STR_ID_PERM_NAME_BLUETOOTH] = "蓝牙",
    [STR_ID_PERM_NAME_AUDIO] = "音频",
    [STR_ID_PERM_NAME_HEALTH] = "健康数据",
    [STR_ID_PERM_NAME_CONTACTS] = "通讯录",
    [STR_ID_PERM_NAME_CALENDAR] = "日历",
    [STR_ID_PERM_DESC_LOCATION] = "你的位置信息用于提供基于位置的服务和功能。",
    [STR_ID_PERM_DESC_SENSOR] = "传感器数据用于检测您的运动和环境状态。",
    [STR_ID_PERM_DESC_NOTIFICATION] = "允许此应用发送和管理通知。",
    [STR_ID_PERM_DESC_STORAGE] = "允许此应用读取和写入存储空间。",
    [STR_ID_PERM_DESC_BLUETOOTH] = "允许此应用使用蓝牙进行连接和数据传输。",
    [STR_ID_PERM_DESC_AUDIO] = "允许此应用录制音频或控制播放。",
    [STR_ID_PERM_DESC_HEALTH] = "允许此应用读取您的健康数据。",
    [STR_ID_PERM_DESC_CONTACTS] = "允许此应用访问您的通讯录。",
    [STR_ID_PERM_DESC_CALENDAR] = "允许此应用访问您的日历信息。",
    [STR_ID_PERM_REQUEST_TITLE] = "允许\"%s\"使用你的%s吗？",
    [STR_ID_PERM_ALLOW_ONCE] = "允许一次",
    [STR_ID_PERM_ALLOW_FOREGROUND] = "使用App时允许",
    [STR_ID_PERM_DENY] = "不允许",
    [STR_ID_PERM_STATE_DENIED] = "已拒绝",
    [STR_ID_PERM_STATE_ALLOW_ONCE] = "已允许(一次)",
    [STR_ID_PERM_STATE_ALLOW_FOREGROUND] = "已允许(前台)",
    [STR_ID_PERM_STATE_ALLOW_ALWAYS] = "始终允许",
    [STR_ID_PERM_NO_PERMISSIONS] = "此应用无需特殊权限",
    [STR_ID_PERM_MANAGEMENT] = "权限管理",
    [STR_ID_PERM_TITLE] = "权限",
    [STR_ID_APP_RESTART] = "重启应用",
    [STR_ID_APP_EXIT] = "退出应用",
    [STR_ID_RESTART] = "重启",
    /* Developer Options */
    [STR_ID_SETTINGS_DEVELOPER_OPTIONS] = "开发者选项",
    [STR_ID_SETTINGS_DEVELOPER_VIEW_LOGS] = "查看日志",
    [STR_ID_SETTINGS_DEVELOPER_FPS] = "FPS显示",
    [STR_ID_SETTINGS_DEVELOPER_MEMORY] = "内存",
    [STR_ID_SETTINGS_DEVELOPER_MEMORY_USED] = "已用内存",
    [STR_ID_SETTINGS_DEVELOPER_MEMORY_FREE] = "空闲内存",
    [STR_ID_SETTINGS_DEVELOPER_OBJS] = "OBJS显示",
    [STR_ID_SETTINGS_DEVELOPER_TOUCH] = "触摸坐标",
    // Add new string IDs and Chinese translations here as needed
};

static const char *const language_list[LANG_MAX_NUMBER] = {[LANG_EN] = "English", [LANG_ZH] = "简体中文"};

static const char **current_lang = NULL; // Current language pointer
static bool lang_initialized = false; // Language system initialized flag

/* Function Implementations -----------------------------------*/
static void lang_event_eos_cb(eos_event_t *e);
void eos_lang_set_current_id(language_id_t lang);
language_id_t eos_lang_parse_name(const char *language_name);

void eos_lang_init(void)
{
    EOS_LOG_D("Init eos_lang");
    if (!lang_initialized)
    {
        /**
         * During first system startup, use the default language from eos_config.h.
         * If config service has a language setting, it will be used;
         * otherwise, the default language is applied.
         */
        const char *default_lang = EOS_CONFIG_DEFAULT_LANGUAGE == 1 ? "简体中文" : "English";
        char *lang_str = eos_config_get_string(EOS_CONFIG_KEY_LANGUAGE_STR, default_lang);
        eos_lang_set_current_id(eos_lang_parse_name(lang_str));
        lang_initialized = true;
        EOS_LOG_I("Language initialized: %s (default: %s)", lang_str, default_lang);
        eos_free(lang_str);
    }
}

void eos_lang_set_current_id(language_id_t lang)
{
    switch (lang)
    {
        case LANG_EN:
            current_lang = lang_en;
            break;
        case LANG_ZH:
            current_lang = lang_zh;
            break;
        default:
            current_lang = lang_en;
            break;
    }

    // Use event broadcast system to refresh all labels
    eos_event_post(EOS_EVENT_LANGUAGE_CHANGED, NULL, NULL);

    EOS_LOG_D("Language changed");
}

language_id_t eos_lang_get_current_id(void)
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
        return LANG_EN;
    }
}

language_id_t eos_lang_parse_name(const char *language_name)
{
    EOS_CHECK_PTR_RETURN_VAL(language_name, LANG_EN);

    if (strcmp(language_list[LANG_EN], language_name) == 0)
    {
        return LANG_EN;
    }
    else if (strcmp(language_list[LANG_ZH], language_name) == 0)
    {
        return LANG_ZH;
    }
    else
    {
        EOS_LOG_E("Language not found: %s", language_name);
        return LANG_EN;
    }
}

const char *eos_lang_get_name(language_id_t lang)
{
    // Get language name by ID
    if (lang < 0 || lang >= LANG_MAX_NUMBER)
    {
        return language_list[LANG_EN];
    }

    return language_list[lang];
}

const char *eos_lang_get_current_name(void)
{
    return eos_lang_get_name(eos_lang_get_current_id());
}

const char *eos_lang_get_text(lang_string_id_t id)
{
    if (id < 0 || id >= STR_ID_MAX_NUMBER || !current_lang || !current_lang[id])
        return NULL;

    return current_lang[id];
}

language_id_t eos_lang_get_current_id_with_str(const char *language_str)
{
    return eos_lang_parse_name(language_str);
}

static void lang_event_eos_cb(eos_event_t *e)
{
    lv_obj_t *label = eos_event_get_obj(e);
    if (!label || !lv_obj_is_valid(label))
    {
        return;
    }

    lang_string_id_t str_id = (lang_string_id_t)(uintptr_t)eos_event_get_user_data(e);
    const char *text = eos_lang_get_text(str_id);
    if (text)
    {
        lv_label_set_text(label, text);
    }
}

static void _lang_label_deleted_cb(lv_event_t *e)
{
    lv_obj_t *label = lv_event_get_target(e);
    eos_event_unsubscribe_with_obj(EOS_EVENT_LANGUAGE_CHANGED, lang_event_eos_cb, label);
}

void eos_label_set_text_id(lv_obj_t *label, lang_string_id_t str_id)
{
    EOS_CHECK_PTR_RETURN(label);

    const char *text = eos_lang_get_text(str_id);
    if (text)
    {
        lv_label_set_text(label, text);
    }

    eos_event_unsubscribe_with_obj(EOS_EVENT_LANGUAGE_CHANGED, lang_event_eos_cb, label);
    eos_event_subscribe_ex(EOS_EVENT_LANGUAGE_CHANGED, lang_event_eos_cb, (void *)(uintptr_t)str_id, label);
    lv_obj_add_event_cb(label, _lang_label_deleted_cb, LV_EVENT_DELETE, NULL);
}

lv_obj_t *eos_lang_label_create(lv_obj_t *parent, lang_string_id_t str_id)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    // Create label
    lv_obj_t *label = lv_label_create(parent);
    if (!label)
        return NULL;

    eos_label_set_text_id(label, str_id);

    return label;
}
