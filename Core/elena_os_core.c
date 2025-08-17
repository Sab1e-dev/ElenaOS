/**
 * @file elena_os_core.c
 * @brief Elena OS 核心代码实现
 * @author Sab1e
 * @date 2025-08-10
 */

/**
 * TODO:
 * 上拉快捷控制台
 * 注册右往左滑回退
 * 中文显示
 * 应用列表
 */

#include "elena_os_core.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "elena_os_drag_item.h"
#include "lvgl.h"
#include "elena_os_img.h"
#include "elena_os_msg_list.h"
#include "elena_os_lang.h"
#include "elena_os_log.h"
#include "elena_os_nav.h"
#include "elena_os_base_item.h"
// Macros and Definitions
// 图片缓冲区大小
// #define LV_FS_CACHE_SIZE  32*1024

// Variables
msg_list_t *my_list;
// Function Implementations
static void _test_msg_list_cb(lv_event_t *e)
{
    char *message = "Sab1e: No one's born being good at all things."
                    "You become good at things through hard work. "
                    "You're not a varsity athlete the first time "
                    "you play a new sport.";

    // 添加消息项
    msg_list_item_t *item = eos_msg_list_item_create(my_list);
    // 设置内容
    eos_msg_list_item_set_title(item, "WeChat");
    eos_msg_list_item_set_msg(item, message);
    eos_msg_list_item_set_time(item, "12:30");

    lv_obj_t *icon = lv_img_create(lv_scr_act());
    eos_img_set_src(icon, "/wx.bin");
    eos_msg_list_item_set_icon_obj(item, icon);

    msg_list_item_t *item1 = eos_msg_list_item_create(my_list);
    eos_msg_list_item_set_title(item1, "WeChat");
    eos_msg_list_item_set_msg(item1, message);
    eos_msg_list_item_set_time(item1, "12:30");

    // lv_obj_t *icon1 = lv_img_create(lv_scr_act());
    // eos_img_set_src(icon1, "/wx.bin");
    // eos_msg_list_item_set_icon_obj(item1, icon);
}

static void _test_msg_list(){
    my_list = eos_msg_list_create(lv_scr_act());

    drag_item_t *drag_item1 = eos_drag_item_create(lv_scr_act());
    eos_drag_item_set_dir(drag_item1, DRAG_DIR_UP);
    eos_drag_item_hide_touch_bar(drag_item1);
}

static void _test_nav_cb_1(lv_event_t *e){
    lv_obj_t *scr1 = eos_nav_scr_create();
    lv_screen_load(scr1);

    lv_obj_t *back_btn = eos_back_btn_create(scr1,false);
    lv_obj_center(back_btn);
}

ElenaOSResult_t eos_run()
{
    eos_lang_init();
    eos_nav_init(lv_scr_act());
    eos_lang_set(LANG_EN);

    lv_theme_t *th = lv_theme_default_init(lv_disp_get_default(),lv_palette_main(LV_PALETTE_BLUE),
                                       lv_palette_main(LV_PALETTE_RED),
                                       true,  /* 深色模式 */
                                       LV_FONT_DEFAULT);
    lv_disp_set_theme(NULL, th);
    
    lv_obj_t *scr = lv_scr_act();

    lv_display_t *disp = lv_disp_get_default();

    lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "09:15");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);

    // lv_obj_t *img = lv_img_create(lv_scr_act());
    // eos_img_set_src(img, "/bg.bin");
    // lv_obj_center(img);
    // lv_obj_move_background(img);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_add_event_cb(btn, _test_msg_list_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(btn);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "New scr");
    lv_obj_set_style_text_font(btn_label,&lv_font_montserrat_48,0);
    lv_obj_center(btn_label);

    _test_msg_list();

    while (1)
    {
        uint32_t d = lv_timer_handler();
        rt_thread_mdelay(d);
    }
}
