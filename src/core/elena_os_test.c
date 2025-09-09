/**
 * @file elena_os_test.c
 * @brief 系统功能测试
 * @author Sab1e
 * @date 2025-08-20
 */

/**
 * TODO:
 * 新增上拉快捷控制台（先做设置App）
 */

#include "elena_os_test.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elena_os_swipe_panel.h"
#include "lvgl.h"
#include "elena_os_img.h"
#include "elena_os_msg_list.h"
#include "elena_os_lang.h"
#include "elena_os_log.h"
#include "elena_os_nav.h"
#include "elena_os_basic_widgets.h"
#include "elena_os_event.h"
#include "elena_os_port.h"
#include "elena_os_app_list.h"
#include "elena_os_core.h"
#include "script_engine_core.h"
#include "script_engine_nav.h"
#include "elena_os_misc.h"
#include "elena_os_watchface_list.h"
// Macros and Definitions
// #define TEST_USE_ZH_FONT
#ifdef TEST_USE_ZH_FONT
LV_FONT_DECLARE(eos_font_resource_han_rounded_30);
#endif
// Variables
static lv_obj_t *img = NULL;         // 全局图片对象
static lv_obj_t *ta = NULL;          // 全局文本输入框对象
extern script_pkg_t script_pkg; // 脚本包
// Function Implementations

void _create_new_scr()
{
    lv_obj_t *scr = eos_nav_scr_create();
    lv_screen_load(scr);
}

static void _test_msg_list_cb(lv_event_t *e)
{
    msg_list_t *msg_list = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(msg_list);
    char *message = "Sab1e: No one's born being good at all things."
                    "You become good at things through hard work. "
                    "You're not a varsity athlete the first time "
                    "you play a new sport.";

    // 添加消息项
    msg_list_item_t *item = eos_msg_list_item_create(msg_list);
    // 设置内容
    eos_msg_list_item_set_title(item, "WeChat");
    eos_msg_list_item_set_msg(item, message);
    eos_msg_list_item_set_time(item, "12:30");

    eos_msg_list_item_icon_set_src(item, "/wx.bin");

    msg_list_item_t *item1 = eos_msg_list_item_create(msg_list);
    eos_msg_list_item_set_title(item1, "QQ");
    eos_msg_list_item_set_msg(item1, message);
    eos_msg_list_item_set_time(item1, "21:00");
}

static void _test_msg_list()
{
    _create_new_scr();
    msg_list_t *msg_list = eos_msg_list_create(lv_scr_act());
    EOS_CHECK_PTR_RETURN(msg_list);
    lv_obj_t *btn = lv_button_create(lv_scr_act());
    lv_obj_center(btn);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, LV_SYMBOL_PLUS " Add new message");
    lv_obj_add_event_cb(btn, _test_msg_list_cb, LV_EVENT_CLICKED, msg_list);
}

static void _test_nav_cb_1(lv_event_t *e)
{
    _create_new_scr();

    lv_obj_t *back_btn = eos_back_btn_create(lv_scr_act(), true);
    lv_obj_center(back_btn);
}

static void _test_font()
{
    _create_new_scr();

    const char *test_str = /* 中文符号测试 */ "，。、：；？！“”‘’（）【】《》〈〉——……·＋－×÷＝≠＞＜≥≤≈±￥％‰℃°＠＃＆☆★●○■□▲△▼▽"
                                              /* 英文符号测试 */ "~!@#$%^&*()-_=+[]{}\\|;:'\",./<>?`©®™"
                                              /* 希腊字母测试 */ "ΑαΒβΓγΔδΕεΖζΗηΘθΙιΚκΛλΜμΝνΞξΟοΠπΡρΣσΤτΥυΦφΧχΨψΩω"
                                              /* 英文数字测试 */ "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"
                                              /* 常用汉字测试 */ "在夏末的午后，风把阳台上的风铃吹得叮当作响，像是某种不经意的暗号。"
                                              /* 罕见汉字测试 */ "霡霂淅沥，薜荔葳蕤。彳亍踟蹰，睥睨娉婷。觊觎饕餮，倥偬倜傥。菡萏猗傩，蘼芜菁菁。";

    lv_obj_t *container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_t *font_label = lv_label_create(container);
    lv_label_set_text(font_label, test_str);
    lv_obj_set_width(font_label, lv_pct(100));
    lv_label_set_long_mode(font_label, LV_LABEL_LONG_WRAP);
#ifdef TEST_USE_ZH_FONT
    lv_obj_set_style_text_font(font_label, &eos_font_resource_han_rounded_30, LV_PART_MAIN);
#endif
}

static void _test_lang_cb(lv_event_t *e)
{
    if (eos_lang_get() == LANG_ZH)
    {
        eos_lang_set(LANG_EN);
    }
    else
    {
        eos_lang_set(LANG_ZH);
    }
}

static void _test_lang(lv_event_t *e)
{
    _create_new_scr();

    lv_obj_t *label = eos_lang_label_create(lv_scr_act(), STR_ID_TEST_LANG_STR);
    lv_obj_set_width(label, lv_pct(100));
#ifdef TEST_USE_ZH_FONT
    lv_obj_set_style_text_font(label, &eos_font_resource_han_rounded_30, LV_PART_MAIN);
#endif
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_t *btn = lv_button_create(lv_scr_act());
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, LV_SYMBOL_LOOP " Switch Language");
    lv_obj_add_event_cb(btn, _test_lang_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void _test_vkb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    lv_obj_t *kb = lv_event_get_user_data(e);

    if (code == LV_EVENT_FOCUSED)
    {
        if (lv_indev_get_type(lv_indev_active()) != LV_INDEV_TYPE_KEYPAD)
        {
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else if (code == LV_EVENT_CANCEL)
    {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_state(ta, LV_STATE_FOCUSED);
        lv_indev_reset(NULL, ta); /*To forget the last clicked object to make it focusable again*/
    }
}

static void _test_vkb()
{
    _create_new_scr();
    lv_obj_t *pinyin_ime = lv_ime_pinyin_create(lv_screen_active());
#ifdef TEST_USE_ZH_FONT
    lv_obj_set_style_text_font(pinyin_ime, &eos_font_resource_han_rounded_30, 0);
#endif
    // lv_ime_pinyin_set_dict(pinyin_ime, your_dict); // Use a custom dictionary. If it is not set, the built-in dictionary will be used.

    /* ta1 */
    lv_obj_t *ta1 = lv_textarea_create(lv_screen_active());
    lv_textarea_set_one_line(ta1, true);
#ifdef TEST_USE_ZH_FONT
    lv_obj_set_style_text_font(ta1, &eos_font_resource_han_rounded_30, 0);
#endif
    lv_obj_align(ta1, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_width(ta1, lv_pct(100));

    /*Create a keyboard and add it to ime_pinyin*/
    lv_obj_t *kb = lv_keyboard_create(lv_screen_active());
    lv_ime_pinyin_set_keyboard(pinyin_ime, kb);
    lv_keyboard_set_textarea(kb, ta1);

    lv_obj_add_event_cb(ta1, _test_vkb_event_cb, LV_EVENT_ALL, kb);

    /*Get the cand_panel, and adjust its size and position*/
    lv_obj_t *cand_panel = lv_ime_pinyin_get_cand_panel(pinyin_ime);
    lv_obj_set_size(cand_panel, LV_PCT(100), LV_PCT(10));
    lv_obj_align_to(cand_panel, kb, LV_ALIGN_OUT_TOP_MID, 0, 0);
}

static void _test_image_input_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *kb = lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED)
    {
        // 点击文本框时显示键盘
        lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
    else if (code == LV_EVENT_READY || code == LV_EVENT_DEFOCUSED)
    {
        // 按下确认键或失去焦点时处理
        const char *path = lv_textarea_get_text(ta);

        if (strlen(path) > 0)
        {
            // 设置图片源
            eos_img_set_src(img, path);

            // 清除文本框内容
            lv_textarea_set_text(ta, "");
        }

        // 隐藏键盘
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void _test_image()
{
    _create_new_scr();
    // 创建图片对象但不设置源
    img = lv_image_create(lv_scr_act());
    lv_obj_center(img);
    lv_obj_move_background(img);

    // 创建文本输入框
    ta = lv_textarea_create(lv_scr_act());
    lv_obj_set_size(ta, LV_HOR_RES - 40, 80);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 20);
    lv_textarea_set_placeholder_text(ta, "Input image path here.(e.g. /flower.bin)");
    lv_textarea_set_one_line(ta, true);

    // 添加键盘
    lv_obj_t *kb = lv_keyboard_create(lv_scr_act());
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN); // 默认隐藏键盘

    // 添加事件回调
    lv_obj_add_event_cb(ta, _test_image_input_cb, LV_EVENT_ALL, kb);
}

static void _test_app_list()
{
    eos_app_list_create();
}

static void _test_watchface_list(){
    eos_watchface_list_create();
}

void eos_test_start(void)
{
#ifdef DEBUG_USE_ZH_FONT

    lv_theme_t *th = lv_theme_default_init(lv_disp_get_default(), lv_palette_main(LV_PALETTE_BLUE),
                                           lv_palette_main(LV_PALETTE_RED),
                                           true, /* 深色模式 */
                                           &eos_font_resource_han_rounded_30);
    
#else
    lv_theme_t *th = lv_theme_default_init(lv_disp_get_default(), lv_palette_main(LV_PALETTE_BLUE),
                                           lv_palette_main(LV_PALETTE_RED),
                                           true, /* 深色模式 */
                                           &lv_font_montserrat_24);
#endif
    lv_disp_set_theme(NULL, th);

    lv_obj_t *scr = lv_scr_act();

    // lv_display_t *disp = lv_disp_get_default();

    // lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // lv_obj_t *img = lv_image_create(lv_scr_act());
    // eos_img_set_src(img, "/bg.bin");
    // lv_obj_center(img);
    // lv_obj_move_background(img);

    lv_obj_t *test_list = lv_list_create(scr);
    lv_obj_set_size(test_list, lv_pct(100), lv_pct(100));

    lv_obj_t *btn;
    lv_list_add_text(test_list, "[ElenaOS Test List]");
    // 测试导航功能
    btn = lv_list_add_button(test_list, LV_SYMBOL_HOME, "Navigation");
    lv_obj_add_event_cb(btn, _test_nav_cb_1, LV_EVENT_CLICKED, NULL);
    // 测试消息列表
    btn = lv_list_add_button(test_list, LV_SYMBOL_LIST, "Message List");
    lv_obj_add_event_cb(btn, _test_msg_list, LV_EVENT_CLICKED, NULL);
    // 测试字体
    btn = lv_list_add_button(test_list, LV_SYMBOL_FILE, "Font");
    lv_obj_add_event_cb(btn, _test_font, LV_EVENT_CLICKED, NULL);
    // 测试语言切换
    btn = lv_list_add_button(test_list, LV_SYMBOL_COPY, "Language");
    lv_obj_add_event_cb(btn, _test_lang, LV_EVENT_CLICKED, NULL);
    // 测试虚拟键盘
    btn = lv_list_add_button(test_list, LV_SYMBOL_KEYBOARD, "Virtual Keyboard");
    lv_obj_add_event_cb(btn, _test_vkb, LV_EVENT_CLICKED, NULL);
    // 测试图像显示
    btn = lv_list_add_button(test_list, LV_SYMBOL_IMAGE, "Image");
    lv_obj_add_event_cb(btn, _test_image, LV_EVENT_CLICKED, NULL);
    // 测试应用列表
    btn = lv_list_add_button(test_list, LV_SYMBOL_DRIVE, "App List");
    lv_obj_add_event_cb(btn, _test_app_list, LV_EVENT_CLICKED, NULL);
    // 测试应用列表
    btn = lv_list_add_button(test_list, LV_SYMBOL_LIST, "Watchface List");
    lv_obj_add_event_cb(btn, _test_watchface_list, LV_EVENT_CLICKED, NULL);

    while (1)
    {
        uint32_t d = lv_timer_handler();
        if (script_engine_get_state()==SCRIPT_STATE_READY)
        {
            script_engine_nav_init(scr);
            script_engine_result_t ret = script_engine_run(&script_pkg);
            eos_pkg_free(&script_pkg);
            if (ret != SE_OK)
            {
                EOS_LOG_E("Script encounter a fatal error");
                lv_obj_t *mbox = lv_msgbox_create(NULL);
                lv_obj_set_width(mbox,lv_pct(80));
                lv_msgbox_add_title(mbox, "Scrip Runtime");

                lv_msgbox_add_text(mbox, current_lang[STR_ID_SCRIPT_RUN_ERR]);
                lv_msgbox_add_close_button(mbox);
            }
            script_engine_nav_clean_up();
            EOS_LOG_D("Script OK");
        }
        eos_delay(d);
    }
}