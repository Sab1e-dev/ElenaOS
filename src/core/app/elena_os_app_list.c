/**
 * @file elena_os_app_list.c
 * @brief 应用列表页面
 * @author Sab1e
 * @date 2025-08-21
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
#include "elena_os_basic_widgets.h"
#include "elena_os_misc.h"
#include "elena_os_img.h"
#include "elena_os_port.h"
#include "elena_os_anim.h"
#include "script_engine_core.h"
#include "elena_os_sys.h"
#include "elena_os_event.h"
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
    script_pkg_t pkg = {0};
    pkg.type = SCRIPT_TYPE_APPLICATION;
    if (script_engine_get_manifest(manifest_path, &pkg) != SE_OK)
    {
        EOS_LOG_E("Read manifest failed: %s", manifest_path);
        return;
    }
    EOS_LOG_D("App Info:\n"
              "id=%s | name=%s | version=%s |\n"
              "author:%s | description:%s",
              pkg.id, pkg.name, pkg.version,
              pkg.version, pkg.description);
    char script_path[PATH_MAX];
    snprintf(script_path, sizeof(script_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_SCRIPT_ENTRY_FILE_NAME,
             app_id);
    if (!eos_is_file(script_path))
    {
        EOS_LOG_E("Can't find script: %s", script_path);
        return;
    }

    pkg.script_str = eos_read_file(script_path);

    memcpy(&script_pkg, &pkg, sizeof(script_pkg_t));

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

static lv_obj_t *_app_icon_create(lv_obj_t *parent, const char *icon_path)
{
    lv_obj_t *app_icon = lv_image_create(parent);
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

    return app_icon;
}

static void _app_installed_cb(lv_event_t *e)
{
    lv_obj_t *parent = lv_event_get_target(e);
    const char *installed_app_id = (const char *)lv_event_get_param(e);
    EOS_CHECK_PTR_RETURN(parent && installed_app_id);
    char icon_path[PATH_MAX];
    snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME,
             installed_app_id);
    if (!eos_is_file(icon_path))
    {
        memcpy(icon_path, EOS_IMG_APP, sizeof(EOS_IMG_APP));
    }
    lv_obj_t *app_icon = _app_icon_create(parent, icon_path);
    EOS_LOG_D("app_icon ptr = %p", app_icon);
    lv_obj_add_event_cb(app_icon, _app_list_icon_clicked_cb, LV_EVENT_CLICKED, (void *)installed_app_id);
    eos_app_obj_auto_delete(app_icon, installed_app_id);
}

static void _container_delete_cb(lv_event_t *e)
{
    lv_obj_t *container = lv_event_get_target(e);
    EOS_CHECK_PTR_RETURN(container);
    eos_event_remove_cb(container, eos_event_get_code(EOS_EVENT_APP_INSTALLED), _app_installed_cb);
}

// 修改应用列表创建函数，按JSON顺序显示应用
void eos_app_list_create(void)
{
    // 创建新的页面用于绘制应用列表
    lv_obj_t *scr = eos_nav_scr_create();
    lv_screen_load(scr);
    
    // 加载应用顺序
    char *json_str = eos_read_file(EOS_APP_LIST_APP_ORDER_PATH);
    cJSON *app_order = json_str ? cJSON_Parse(json_str) : NULL;
    eos_free_large(json_str);
    
    lv_obj_t *container = lv_list_create(scr);
    lv_obj_set_style_pad_all(container, 20, 0);
    lv_obj_set_style_pad_column(container, 20, 0); // 列间距
    lv_obj_set_style_pad_row(container, 20, 0);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_set_scroll_dir(container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(container);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(container,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_add_event_cb(container, _container_delete_cb, LV_EVENT_DELETE, NULL);
    eos_event_add_cb(container, _app_installed_cb, eos_event_get_code(EOS_EVENT_APP_INSTALLED), NULL);
    
    // 按JSON顺序添加其他应用
    if (app_order) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, app_order) {
            if (cJSON_IsString(item)) {
                const char *app_id = item->valuestring;
                
                // 跳过系统设置应用（已经添加过了）
                if (strcmp(app_id, "sys.settings") == 0) {
                    char icon_path[PATH_MAX];
                    memcpy(icon_path, EOS_IMG_SETTINGS, sizeof(EOS_IMG_SETTINGS));
                    lv_obj_t *settings_icon = _app_icon_create(container, icon_path);
                    lv_obj_add_event_cb(settings_icon, _app_list_settings_cb, LV_EVENT_CLICKED, NULL);
                    // 设置系统应用的ID
                    lv_obj_set_user_data(settings_icon, (void*)"sys.settings");
                    continue;
                }
                
                // 检查应用是否存在
                if (!eos_app_list_contains(app_id)) {
                    continue;
                }
                
                char icon_path[PATH_MAX];
                snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME,
                         app_id);
                if (!eos_is_file(icon_path)) {
                    memcpy(icon_path, EOS_IMG_APP, sizeof(EOS_IMG_APP));
                }
                lv_obj_t *app_icon = _app_icon_create(container, icon_path);
                lv_obj_add_event_cb(app_icon, _app_list_icon_clicked_cb, LV_EVENT_CLICKED, (void *)app_id);
                eos_app_obj_auto_delete(app_icon, app_id);
            }
        }
        cJSON_Delete(app_order);
    } else {
        // 如果没有JSON顺序文件，按默认顺序添加
        size_t app_list_size = eos_app_list_size();
        for (size_t i = 0; i < app_list_size; i++) {
            const char *app_id = eos_app_list_get_id(i);
            char icon_path[PATH_MAX];
            snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME,
                     app_id);
            if (!eos_is_file(icon_path)) {
                memcpy(icon_path, EOS_IMG_APP, sizeof(EOS_IMG_APP));
            }
            lv_obj_t *app_icon = _app_icon_create(container, icon_path);
            lv_obj_add_event_cb(app_icon, _app_list_icon_clicked_cb, LV_EVENT_CLICKED, (void *)app_id);
            eos_app_obj_auto_delete(app_icon, app_id);
        }
    }
}