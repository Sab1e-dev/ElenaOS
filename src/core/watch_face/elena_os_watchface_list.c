/**
 * @file elena_os_watchface_list.c
 * @brief 表盘列表
 * @author Sab1e
 * @date 2025-08-25
 */

/**
 * TODO:
 * 图片按下不灵敏，原因未知
 */

#include "elena_os_watchface_list.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "cJSON.h"
#include "elena_os_nav.h"
#include "elena_os_log.h"
#include "elena_os_watchface.h"
#include "elena_os_basic_widgets.h"
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

static void _watchface_list_btn_cb(lv_event_t *e)
{
    if (script_engine_get_state() != SCRIPT_STATE_STOPPED)
    {
        EOS_LOG_E("Another script running");
        return;
    }
    const char *watchface_id = (const char *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(watchface_id);
    eos_sys_cfg_set_string(EOS_SYS_CFG_KEY_WATCHFACE_ID, watchface_id);
    eos_nav_back_clean();
}
void eos_watchface_list_create(void)
{
    // 创建新的页面用于绘制应用列表
    lv_obj_t *scr = eos_nav_scr_create();
    lv_screen_load(scr);
    size_t watchface_list_size = eos_watchface_list_size();

    lv_obj_t *cont = lv_list_create(scr);
    lv_obj_set_style_pad_all(cont, 24, 0);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_START,   // 主轴(水平方向)居中
                          LV_FLEX_ALIGN_CENTER,  // 交叉轴(垂直方向)居中
                          LV_FLEX_ALIGN_CENTER); // 内容居中

    for (size_t i = 0; i < watchface_list_size; i++)
    {
        lv_obj_t *item = lv_obj_create(cont);
        lv_obj_set_size(item, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN); // 垂直布局
        lv_obj_set_style_pad_all(item, 0, 0);
        lv_obj_set_style_margin_left(item, 50, 0);
        lv_obj_set_style_pad_gap(item, 20, 0); // snapshot 和 label 的间距
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_shadow_width(item, 0, 0);
        lv_obj_set_style_bg_opa(item,LV_OPA_TRANSP,0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_align(item,
                              LV_FLEX_ALIGN_START,   // 主轴(水平方向)居中
                              LV_FLEX_ALIGN_CENTER,  // 交叉轴(垂直方向)居中
                              LV_FLEX_ALIGN_CENTER); // 内容居中
        
        char icon_path[PATH_MAX];
        snprintf(icon_path, sizeof(icon_path), EOS_WATCHFACE_INSTALLED_DIR "%s/" EOS_WATCHFACE_SNAPSHOT_FILE_NAME,
                 eos_watchface_list_get_id(i));
        EOS_LOG_D("WFPATH:%s", icon_path);
        if (!eos_is_file(icon_path))
        {
            memcpy(icon_path, EOS_IMG_APP, sizeof(EOS_IMG_APP));
        }

        lv_obj_t *watchface_snapshot = lv_image_create(item);
        lv_obj_set_size(watchface_snapshot, 268, 310);
        lv_obj_set_style_shadow_width(watchface_snapshot, 0, 0);
        lv_obj_set_style_margin_all(watchface_snapshot, 0, 0);
        lv_obj_center(watchface_snapshot);
        lv_obj_set_style_pad_all(watchface_snapshot, 0, 0);
        // lv_obj_remove_flag(watchface_snapshot, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_add_flag(watchface_snapshot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(watchface_snapshot, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        eos_img_set_src(watchface_snapshot, icon_path);
        eos_img_set_size(watchface_snapshot, 268, 310);
        lv_obj_center(watchface_snapshot);
        lv_obj_add_event_cb(watchface_snapshot, _watchface_list_btn_cb, LV_EVENT_CLICKED, (void *)eos_watchface_list_get_id(i));
        lv_obj_set_style_clip_corner(watchface_snapshot, false, 0);
        if (encoder_group)
        {
            lv_group_add_obj(encoder_group, watchface_snapshot);
        }
        // 显示名称
        char manifest_path[PATH_MAX];
        snprintf(manifest_path, sizeof(manifest_path), EOS_WATCHFACE_INSTALLED_DIR "%s/" EOS_WATCHFACE_MANIFEST_FILE_NAME,
                 eos_watchface_list_get_id(i));
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
        cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
        if (!cJSON_IsString(name) || name->valuestring == NULL)
        {
            EOS_LOG_E("Get \"name\" failed");
            cJSON_Delete(root);
            return;
        }
        lv_obj_t *label = lv_label_create(item);
        lv_label_set_text(label, name->valuestring);
        lv_obj_set_width(label, LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        cJSON_Delete(root);
    }
}