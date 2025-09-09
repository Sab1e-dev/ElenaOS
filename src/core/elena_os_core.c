/**
 * @file elena_os_core.c
 * @brief Elena OS 核心代码实现
 * @author Sab1e
 * @date 2025-08-10
 */

/**
 * TODO:
 * 编码器索引
 * screen 切换渐变（LVGL自带）
 * 
 */

#include "elena_os_core.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "lvgl.h"
#include "cJSON.h"
#include "elena_os_img.h"
#include "elena_os_msg_list.h"
#include "elena_os_lang.h"
#include "elena_os_log.h"
#include "elena_os_nav.h"
#include "elena_os_base_item.h"
#include "elena_os_event.h"
#include "elena_os_test.h"
#include "elena_os_version.h"
#include "elena_os_port.h"
#include "elena_os_swipe_panel.h"
#include "elena_os_sys.h"
#include "elena_os_app.h"
#include "script_engine_core.h"
#include "elena_os_watchface.h"
#include "elena_os_misc.h"
#include "elena_os_watchface_list.h"
#include "elena_os_app_list.h"
#include "script_engine_nav.h"
#include "elena_os_theme.h"
// Macros and Definitions
typedef enum
{
    ENTRY_NULL = 0,
    ENTRY_APP_LIST,
    ENTRY_WATCHFACE_LIST,
} screen_entry_t;
// Variables
script_pkg_t script_pkg = {0}; // 当前运行的脚本包
lv_group_t *encoder_group;
lv_obj_t *root_scr;
screen_entry_t next_screen_type;
static eos_side_btn_state_t side_btn_state = SIDE_BTN_RELEASED;
// Function Implementations

static void _watchface_long_pressed_cb(lv_event_t *e)
{
    script_engine_request_stop();
    next_screen_type = ENTRY_WATCHFACE_LIST;
}

static lv_indev_t *_get_key_indev()
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev)
    {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD)
        {
            return indev;
        }
        indev = lv_indev_get_next(indev);
    }
    EOS_LOG_W("Not found input device: key");
}

void eos_side_btn_handler(eos_side_btn_state_t state)
{
    static bool side_btn_processing = false;
    if (side_btn_processing)
        return;
    side_btn_processing = true;
    switch (state)
    {
    case SIDE_BTN_CLICKED:
        lv_obj_t *scr = lv_screen_active();
        if (scr == root_scr)
        {
            script_engine_request_stop();
            next_screen_type = ENTRY_APP_LIST;
            side_btn_state = SIDE_BTN_RELEASED;
        }
        else
        {
            side_btn_state = SIDE_BTN_CLICKED;
        }
    default:
        break;
    }
    side_btn_processing = false;
}

eos_result_t eos_run()
{
    /************************** 变量初始化 **************************/
    root_scr = lv_screen_active();
    /************************** 系统组件初始化 **************************/
    eos_event_init();
    // eos_theme_init();
    eos_theme_set(lv_palette_main(LV_PALETTE_BLUE),
                  lv_palette_main(LV_PALETTE_RED),
                  &lv_font_montserrat_30);
    eos_app_init();
    eos_watchface_init();
    eos_sys_init();
    eos_lang_init();
    // 加载导航
    eos_nav_init(root_scr);
    eos_lang_set(LANG_EN);

    lv_indev_t *indev = _get_key_indev();
    if (indev)
    {
        encoder_group = lv_indev_get_group(indev);
        lv_group_add_obj(encoder_group, root_scr);
    }
    else
    {
        EOS_LOG_W("Input device not found");
    }

    if (eos_watchface_list_size() == 0)
    {
        EOS_LOG_E("Watchface not found");
        while (1)
        {
            if (eos_watchface_list_size() > 0)
                break;
            eos_delay(5000);
        }
    }
    /************************** 基础部件初始化 **************************/
    eos_app_header_init();
    
    /************************** 系统启动 **************************/
    // 加载表盘
    while (1)
    {
        // JSON中获取表盘id
        char *wf_id = eos_sys_cfg_get_string(EOS_SYS_CFG_KEY_WATCHFACE_ID, "cn.sab1e.clock");
        if (!wf_id)
        {
            EOS_LOG_E("NULL wf_id");
            return -EOS_FAILED;
        }
        // 直接通过表盘id 获取相关信息并存储到script_package
        char manifest_path[PATH_MAX];
        snprintf(manifest_path, sizeof(manifest_path), EOS_WATCHFACE_INSTALLED_DIR "%s/" EOS_WATCHFACE_MANIFEST_FILE_NAME,
                 wf_id);
        char *manifest_json = eos_read_file(manifest_path);
        if (!manifest_json)
        {
            EOS_LOG_E("Read manifest.json failed");
            return -EOS_FAILED;
        }
        // 获取根节点
        cJSON *root = cJSON_Parse(manifest_json);
        eps_free_large(manifest_json); // 解析完立即释放原始字符串
        if (!root)
        {
            EOS_LOG_E("parse error: %s\n", cJSON_GetErrorPtr());
            return -EOS_FAILED;
        }
        // 读取脚本包相关信息
        cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
        if (!cJSON_IsString(id) || id->valuestring == NULL)
        {
            EOS_LOG_E("Get \"id\" failed");
            cJSON_Delete(root);
            return -EOS_FAILED;
        }
        cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
        if (!cJSON_IsString(name) || name->valuestring == NULL)
        {
            EOS_LOG_E("Get \"name\" failed");
            cJSON_Delete(root);
            return -EOS_FAILED;
        }
        cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
        if (!cJSON_IsString(version) || version->valuestring == NULL)
        {
            EOS_LOG_E("Get \"version\" failed");
            cJSON_Delete(root);
            return -EOS_FAILED;
        }
        cJSON *author = cJSON_GetObjectItemCaseSensitive(root, "author");
        if (!cJSON_IsString(author) || author->valuestring == NULL)
        {
            EOS_LOG_E("Get \"author\" failed");
            cJSON_Delete(root);
            return -EOS_FAILED;
        }
        cJSON *description = cJSON_GetObjectItemCaseSensitive(root, "description");
        if (!cJSON_IsString(description) || description->valuestring == NULL)
        {
            EOS_LOG_E("Get \"description\" failed");
            cJSON_Delete(root);
            return -EOS_FAILED;
        }
        EOS_LOG_D("Watchface Info:\n"
                  "id=%s | name=%s | version=%s |\n"
                  "author:%s | description:%s",
                  id->valuestring, name->valuestring, version->valuestring,
                  author->valuestring, description->valuestring);
        char script_path[PATH_MAX];
        snprintf(script_path, sizeof(script_path), EOS_WATCHFACE_INSTALLED_DIR "%s/" EOS_WATCHFACE_SCRIPT_ENTRY_FILE_NAME,
                 wf_id);
        free((void *)wf_id);
        if (!eos_is_file(script_path))
        {
            EOS_LOG_E("Can't find script: %s", script_path);
            cJSON_Delete(root);
            return -EOS_FAILED;
        }

        script_pkg.id = eos_strdup(id->valuestring);
        script_pkg.name = eos_strdup(name->valuestring);
        script_pkg.type = SCRIPT_TYPE_WATCHFACE;
        script_pkg.version = eos_strdup(version->valuestring);
        script_pkg.author = eos_strdup(author->valuestring);
        script_pkg.description = eos_strdup(description->valuestring);
        script_pkg.script_str = eos_read_file(script_path);

        // 设置下拉面板
        msg_list_t *msg_list = eos_msg_list_create(root_scr);
        if (!msg_list)
        {
            EOS_LOG_E("Create msg_list failed");
            return -SE_FAILED;
        }
        // 设置上拉面板

        // 设置长按回调 进入 watchface list 使用普通 nav 导航
        lv_obj_add_event_cb(root_scr, _watchface_long_pressed_cb, LV_EVENT_LONG_PRESSED, NULL);
        // 正式运行表盘脚本
        script_engine_result_t ret = script_engine_run(&script_pkg);
        eos_pkg_free(&script_pkg);
        lv_obj_clean(root_scr);
        if (ret != SE_OK)
        {
            EOS_LOG_E("Script encounter a fatal error");
            while(1);
        }

        switch (next_screen_type)
        {
        case ENTRY_APP_LIST:
            eos_app_list_create();
            break;
        case ENTRY_WATCHFACE_LIST:
            eos_watchface_list_create();
            break;
        default:
            EOS_LOG_W("Entry not found");
            break;
        }
        while (1)
        {
            uint32_t d = lv_timer_handler();
            eos_delay(d);
            if (script_engine_get_state() == SCRIPT_STATE_READY)
            {
                script_engine_nav_init(lv_screen_active());
                script_engine_result_t ret = script_engine_run(&script_pkg);
                eos_pkg_free(&script_pkg);
                if (ret != SE_OK)
                {
                    EOS_LOG_E("Script encounter a fatal error");
                    lv_obj_t *mbox = lv_msgbox_create(NULL);
                    lv_obj_set_width(mbox, lv_pct(80));
                    lv_msgbox_add_title(mbox, "Scrip Runtime");

                    lv_msgbox_add_text(mbox, current_lang[STR_ID_SCRIPT_RUN_ERR]);
                    lv_msgbox_add_close_button(mbox);
                }
                script_engine_nav_clean_up();
                EOS_LOG_D("Script OK");
            }
            if (lv_screen_active() == root_scr)
            {
                // 判断有没有回到表盘页面，如果回到了，就退出刷新
                break;
            }
            if (side_btn_state == SIDE_BTN_CLICKED)
            {
                side_btn_state = SIDE_BTN_RELEASED;
                if (lv_screen_active() != root_scr)
                {
                    eos_nav_back_clean();
                }
            }
        }
        next_screen_type = ENTRY_NULL; // 立即清理状态
    }
}
