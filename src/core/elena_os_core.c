/**
 * @file elena_os_core.c
 * @brief Elena OS 核心代码实现
 * @author Sab1e
 * @date 2025-08-10
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
// Macros and Definitions

// Variables
script_pkg_t *script_pkg_ptr; // 当前运行的脚本包指针
// Function Implementations

static void _watchface_long_pressed_cb(lv_event_t *e)
{
    // 彻底删除当前运行的脚本表盘
    script_engine_request_stop();
    // 创建表盘选择页面
    eos_watchface_list_create();
}

eos_result_t eos_run()
{
    lv_obj_t *root_scr = lv_screen_active();
    eos_app_init();
    eos_watchface_init();
    eos_sys_init();
    eos_event_init();
    eos_lang_init();
    // 加载导航
    eos_nav_init(root_scr);
    eos_lang_set(LANG_EN);
    script_pkg_ptr = eos_mem_alloc(sizeof(script_pkg_t));
    // 加载表盘
    while (1)
    {
        // JSON中获取表盘id
        char *wf_id=eos_sys_cfg_get_string(EOS_SYS_CFG_KEY_WATCHFACE_ID, "cn.sab1e.clock");
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
        eos_mem_free(manifest_json); // 解析完立即释放原始字符串
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

        script_pkg_ptr = eos_mem_alloc(sizeof(script_pkg_t));
        if (!script_pkg_ptr)
        {
            EOS_LOG_E("memory alloc failed");
            return -EOS_FAILED;
        }
        script_pkg_ptr->id = eos_strdup(id->valuestring);
        script_pkg_ptr->name = eos_strdup(name->valuestring);
        script_pkg_ptr->type = SCRIPT_TYPE_APPLICATION;
        script_pkg_ptr->version = eos_strdup(version->valuestring);
        script_pkg_ptr->author = eos_strdup(author->valuestring);
        script_pkg_ptr->description = eos_strdup(description->valuestring);
        script_pkg_ptr->script_str = eos_read_file(script_path);

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
        script_engine_result_t ret = script_engine_run(script_pkg_ptr);
        eos_pkg_free(script_pkg_ptr);
        lv_obj_clean(root_scr);
        script_pkg_ptr = NULL;
        if (ret != SE_OK)
        {
            EOS_LOG_E("Script encounter a fatal error");
        }
        // 无限循环
        // 直到长按表盘或打开应用列表，此时脚本已结束
        // 清理表盘数据
        // 加载表盘脚本
        while (1)
        {
            uint32_t d = lv_timer_handler();
            eos_delay(d);
            if(lv_screen_active() == root_scr){
                // 判断有没有回到表盘页面，如果回到了，就退出刷新
                break;
            }
        }
    }
}
