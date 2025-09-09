/**
 * @file elena_os_app_list.c
 * @brief 应用列表页面
 * @author Sab1e
 * @date 2025-08-21
 */

/**
 * TODO:
 * 应用列表需要支持系统应用（c应用）
 * 广播通知应用安装与卸载
 * 应用列表需要支持应用排序
 */

#include "elena_os_app_list.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "cJSON.h"
#include "elena_os_nav.h"
#include "elena_os_log.h"
#include "elena_os_app.h"
#include "elena_os_base_item.h"
#include "elena_os_misc.h"
#include "elena_os_img.h"
#include "elena_os_port.h"
#include "elena_os_anim.h"
#include "script_engine_core.h"
#include "elena_os_sys.h"
// Macros and Definitions

// Variables
extern script_pkg_t script_pkg; // 脚本包
extern lv_group_t *encoder_group;
// Function Implementations

/**
 * @brief 应用图标按下后的回调
 */
static void _app_list_icon_clicked_cb(lv_event_t *e)
{
    if (script_engine_get_state() != SCRIPT_STATE_STOPPED)
    {
        EOS_LOG_E("Another script running");
        return;
    }
    const char *app_id = (const char *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(app_id);

    // 获取清单文件
    char manifest_path[PATH_MAX];
    snprintf(manifest_path, sizeof(manifest_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_MANIFEST_FILE_NAME,
             app_id);
    char *manifest_json = eos_read_file(manifest_path);
    if (!manifest_json)
    {
        EOS_LOG_E("Read manifest.json failed");
        return;
    }
    // 获取根节点
    cJSON *root = cJSON_Parse(manifest_json);
    eps_free_large(manifest_json); // 解析完立即释放原始字符串
    if (!root)
    {
        EOS_LOG_E("parse error: %s\n", cJSON_GetErrorPtr());
        return;
    }
    // 读取脚本包相关信息
    cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (!cJSON_IsString(id) || id->valuestring == NULL)
    {
        EOS_LOG_E("Get \"id\" failed");
        cJSON_Delete(root);
        return;
    }
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (!cJSON_IsString(name) || name->valuestring == NULL)
    {
        EOS_LOG_E("Get \"name\" failed");
        cJSON_Delete(root);
        return;
    }
    cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (!cJSON_IsString(version) || version->valuestring == NULL)
    {
        EOS_LOG_E("Get \"version\" failed");
        cJSON_Delete(root);
        return;
    }
    cJSON *author = cJSON_GetObjectItemCaseSensitive(root, "author");
    if (!cJSON_IsString(author) || author->valuestring == NULL)
    {
        EOS_LOG_E("Get \"author\" failed");
        cJSON_Delete(root);
        return;
    }
    cJSON *description = cJSON_GetObjectItemCaseSensitive(root, "description");
    if (!cJSON_IsString(description) || description->valuestring == NULL)
    {
        EOS_LOG_E("Get \"description\" failed");
        cJSON_Delete(root);
        return;
    }
    EOS_LOG_D("App Info:\n"
              "id=%s | name=%s | version=%s |\n"
              "author:%s | description:%s",
              id->valuestring, name->valuestring, version->valuestring,
              author->valuestring, description->valuestring);
    char script_path[PATH_MAX];
    snprintf(script_path, sizeof(script_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_SCRIPT_ENTRY_FILE_NAME,
             app_id);
    if (!eos_is_file(script_path))
    {
        EOS_LOG_E("Can't find script: %s", script_path);
        cJSON_Delete(root);
        return;
    }

    script_pkg.id = eos_strdup(id->valuestring);
    script_pkg.name = eos_strdup(name->valuestring);
    script_pkg.type = SCRIPT_TYPE_APPLICATION;
    script_pkg.version = eos_strdup(version->valuestring);
    script_pkg.author = eos_strdup(author->valuestring);
    script_pkg.description = eos_strdup(description->valuestring);
    script_pkg.script_str = eos_read_file(script_path);
    cJSON_Delete(root);
    if (!script_engine_request_ready())
    {
        EOS_LOG_E("Request ready failed");
        return;
    }
}

/**
 * @brief 设置图标按下时的回调 
 */
static void _app_list_settings_cb(lv_event_t *e)
{
    eos_sys_settings_create();
}

void eos_app_list_create(void)
{
    // 创建新的页面用于绘制应用列表
    lv_obj_t *scr = eos_nav_scr_create();
    lv_screen_load(scr);
    size_t app_list_size = eos_app_list_size();

    lv_obj_t *cont = lv_list_create(scr);
    lv_obj_set_style_pad_all(cont, 20, 0);
    lv_obj_set_style_pad_column(cont, 20, 0); // 列间距
    lv_obj_set_style_pad_row(cont, 20, 0);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    // 系统设置
    char icon_path[PATH_MAX];
    memcpy(icon_path, EOS_IMG_SETTINGS, sizeof(EOS_IMG_SETTINGS));
    lv_obj_t *settings_icon = lv_image_create(cont);
    lv_obj_set_size(settings_icon, 100, 100);
    lv_obj_set_style_shadow_width(settings_icon, 0, 0);
    lv_obj_set_style_margin_all(settings_icon, 0, 0);
    lv_obj_set_style_pad_all(settings_icon, 0, 0);
    lv_obj_remove_flag(settings_icon, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(settings_icon, LV_OBJ_FLAG_CLICKABLE);
    eos_img_set_src(settings_icon, icon_path);
    eos_img_set_size(settings_icon, 100, 100);
    lv_obj_center(settings_icon);
    lv_obj_add_event_cb(settings_icon, _app_list_settings_cb, LV_EVENT_CLICKED, NULL);
    if (encoder_group)
    {
        lv_group_add_obj(encoder_group, settings_icon);
    }
    // 脚本应用
    for (size_t i = 0; i < app_list_size; i++)
    {
        char icon_path[PATH_MAX];
        snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME,
                 eos_app_list_get_id(i));
        if (!eos_is_file(icon_path))
        {
            memcpy(icon_path, EOS_IMG_APP, sizeof(EOS_IMG_APP));
        }
        lv_obj_t *app_icon = lv_image_create(cont);
        lv_obj_set_size(app_icon, 100, 100);
        lv_obj_set_style_shadow_width(app_icon, 0, 0);
        lv_obj_set_style_margin_all(app_icon, 0, 0);
        lv_obj_set_style_pad_all(app_icon, 0, 0);
        // lv_obj_remove_flag(app_icon, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_add_flag(app_icon, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(app_icon, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        eos_img_set_src(app_icon, icon_path);
        eos_img_set_size(app_icon, 100, 100);
        lv_obj_center(app_icon);
        lv_obj_add_event_cb(app_icon, _app_list_icon_clicked_cb, LV_EVENT_CLICKED, (void *)eos_app_list_get_id(i));
        if (encoder_group)
        {
            lv_group_add_obj(encoder_group, app_icon);
        }
    }
}