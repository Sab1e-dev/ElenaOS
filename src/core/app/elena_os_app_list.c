/**
 * @file elena_os_app_list.c
 * @brief 应用列表页面
 * @author Sab1e
 * @date 2025-08-21
 */

/**
 * TODO:
 * 解析应用的 manifest
 * 调用 Script Engine
 * 应用列表需要支持系统应用（c应用）
 */

#include "elena_os_app_list.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "elena_os_nav.h"
#include "elena_os_log.h"
#include "elena_os_app.h"
#include "elena_os_base_item.h"
#include "elena_os_misc.h"
#include "elena_os_img.h"
#include "elena_os_port.h"
#include "elena_os_anim.h"
#include "cJSON.h"
// Macros and Definitions

// Variables
// Function Implementations

static void _app_list_btn_cb(lv_event_t *e)
{
    const char *app_id = (const char *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(app_id);

    // 创建新的页面用于绘制应用详情页
    lv_obj_t *scr = eos_nav_scr_create();
    lv_screen_load(scr);

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
    eos_mem_free(manifest_json); // 解析完立即释放原始字符串
    if (!root)
    {
        EOS_LOG_E("parse error: %s\n", cJSON_GetErrorPtr());
        return;
    }

    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (!cJSON_IsString(name) || name->valuestring == NULL)
    {
        EOS_LOG_E("Get \"name\" failed");
        cJSON_Delete(root);
        return;
    }
    EOS_LOG_I("name = %s\n", name->valuestring);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, name->valuestring);
    lv_obj_center(label);
    cJSON_Delete(root);
}

void eos_app_list_create()
{
    // 创建新的页面用于绘制应用列表
    lv_obj_t *scr = eos_nav_scr_create();
    lv_screen_load(scr);
    // lv_obj_t *app_list = lv_list_create(scr);
    // lv_obj_set_size(app_list, lv_pct(100), lv_pct(100));

    size_t app_list_size = eos_app_list_size();

    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_style_pad_all(cont, 20, 0);
    lv_obj_set_style_pad_column(cont, 20, 0); // 列间距
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    // lv_obj_set_style_margin_top(cont,20,0);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_START,  // 主轴(水平方向)居中
                          LV_FLEX_ALIGN_START,  // 交叉轴(垂直方向)居中
                          LV_FLEX_ALIGN_START); // 内容居中
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
        lv_obj_remove_flag(app_icon, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_add_flag(app_icon, LV_OBJ_FLAG_CLICKABLE);
        eos_img_set_src(app_icon, icon_path);
        lv_image_set_scale(app_icon, 400);
        lv_obj_center(app_icon);
        lv_obj_add_event_cb(app_icon, _app_list_btn_cb, LV_EVENT_CLICKED, (void *)eos_app_list_get_id(i));
    }
}