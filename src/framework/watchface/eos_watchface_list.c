/**
 * @file eos_watchface_list.c
 * @brief Watchface list
 */

#include "eos_watchface_list.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "cJSON.h"
#define EOS_LOG_TAG "WatchfaceList"
#include "eos_log.h"
#include "eos_watchface.h"
#include "eos_basic_widgets.h"
#include "eos_pkg_mgr.h"
#include "eos_image.h"
#include "eos_port.h"
#include "eos_anim.h"
#include "script_engine_core.h"
#include "eos_service_config.h"
#include "eos_service_storage.h"
#include "eos_activity.h"
#include "eos_mem.h"
/* Macros and Definitions -------------------------------------*/

#define _SNAPSHOT_CONTAINER_PAD 12
#define _SNAPSHOT_CONTAINER_BORDER 5
#define _SNAPSHOT_CONTAINER_W ((EOS_DISPLAY_WIDTH * 69) / 100)
#define _SNAPSHOT_CONTAINER_H ((EOS_DISPLAY_HEIGHT * 69) / 100)
#define _SNAPSHOT_PRESS_ZOOM 245

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

/**
 * @brief Watchface press callback in watchface list
 */
static void _watchface_list_btn_cb(lv_event_t *e)
{
    const char *watchface_id = (const char *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(watchface_id);

    EOS_LOG_I("Watchface list: User selected watchface: %s", watchface_id);

    // Switch to the selected watchface
    eos_watchface_switch_to(watchface_id);
}

void eos_watchface_list_enter(void)
{
    // Create new page for drawing watchface list
    eos_activity_t *a = eos_activity_create(NULL);
    eos_activity_set_type(a, EOS_ACTIVITY_TYPE_WATCHFACE_LIST);
    lv_obj_t *wf_list_view = eos_activity_get_view(a);
    size_t watchface_list_size = eos_watchface_list_size();

    lv_obj_t *cont = lv_list_create(wf_list_view);
    lv_obj_set_style_pad_all(cont, 24, 0);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_HOR);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_START,   // Main axis (horizontal) start
                          LV_FLEX_ALIGN_CENTER,  // Cross axis (vertical) center
                          LV_FLEX_ALIGN_CENTER); // Content center
    lv_obj_set_scroll_snap_x(cont, LV_SCROLL_SNAP_CENTER);

    lv_obj_t *selected_item = NULL;
    char *current_wf_id = eos_config_get_string(EOS_CONFIG_KEY_WATCHFACE_ID_STR, NULL);

    for (size_t i = 0; i < watchface_list_size; i++)
    {
        const char *watchface_id = eos_watchface_list_get_id(i);
        lv_obj_t *item = lv_obj_create(cont);
        lv_obj_set_size(item, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN); // Vertical layout
        lv_obj_set_style_pad_all(item, 0, 0);
        lv_obj_set_style_margin_left(item, 50, 0);
        lv_obj_set_style_pad_gap(item, 20, 0); // Space between label and snapshot
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_shadow_width(item, 0, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(item, LV_OBJ_FLAG_SNAPPABLE);
        lv_obj_set_flex_align(item,
                              LV_FLEX_ALIGN_START,   // Main axis (horizontal) start
                              LV_FLEX_ALIGN_CENTER,  // Cross axis (vertical) center
                              LV_FLEX_ALIGN_CENTER); // Content center

        if (current_wf_id && watchface_id && strcmp(watchface_id, current_wf_id) == 0)
        {
            selected_item = item;
        }

        char icon_path[EOS_FS_PATH_MAX];
        if (watchface_id && strcmp(watchface_id, EOS_WATCHFACE_BUILTIN_FALLBACK_ID) == 0)
        {
            memcpy(icon_path, EOS_IMG_WATCHFACE, sizeof(EOS_IMG_WATCHFACE));
        }
        else
        {
            snprintf(icon_path, sizeof(icon_path), EOS_WATCHFACE_INSTALLED_DIR "%s/" EOS_WATCHFACE_SNAPSHOT_FILE_NAME,
                     watchface_id);
            EOS_LOG_D("WFPATH:%s", icon_path);
            if (!eos_storage_is_file(icon_path))
            {
                EOS_LOG_W("Watchface snapshot not found!");
                memcpy(icon_path, EOS_IMG_WATCHFACE, sizeof(EOS_IMG_WATCHFACE));
            }
        }

        // Outer container: used to display border, rounded corners and corner clipping, snapshot has 12px padding inside the container
        lv_obj_t *snapshot_container = lv_obj_create(item);
        lv_obj_set_size(snapshot_container, _SNAPSHOT_CONTAINER_W, _SNAPSHOT_CONTAINER_H);
        lv_obj_set_style_border_width(snapshot_container, _SNAPSHOT_CONTAINER_BORDER, 0);
        lv_obj_set_style_radius(snapshot_container, EOS_DISPLAY_RADIUS, 0);
        lv_obj_set_style_clip_corner(snapshot_container, true, 0);
        lv_obj_set_style_pad_all(snapshot_container, _SNAPSHOT_CONTAINER_PAD, 0);
        lv_obj_set_style_shadow_width(snapshot_container, 0, 0);
        lv_obj_set_style_bg_opa(snapshot_container, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(snapshot_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(snapshot_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(snapshot_container, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_center(snapshot_container);

        // Image clip container: responsible for snapshot layout, corner clipping and rounded corners
        lv_obj_t *snapshot_clip_container = lv_obj_create(snapshot_container);
        lv_obj_set_size(snapshot_clip_container,
                _SNAPSHOT_CONTAINER_W - (_SNAPSHOT_CONTAINER_PAD * 2),
                _SNAPSHOT_CONTAINER_H - (_SNAPSHOT_CONTAINER_PAD * 2));
        lv_obj_set_style_radius(snapshot_clip_container, EOS_DISPLAY_RADIUS, 0);
        lv_obj_set_style_clip_corner(snapshot_clip_container, true, 0);
        lv_obj_set_style_border_width(snapshot_clip_container, 0, 0);
        lv_obj_set_style_shadow_width(snapshot_clip_container, 0, 0);
        lv_obj_set_style_margin_all(snapshot_clip_container, 0, 0);
        lv_obj_set_style_pad_all(snapshot_clip_container, 0, 0);
        lv_obj_set_style_bg_opa(snapshot_clip_container, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(snapshot_clip_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(snapshot_clip_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(snapshot_clip_container, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_center(snapshot_clip_container);

        // Snapshot fills the new parent container
        lv_obj_t *watchface_snapshot = lv_image_create(snapshot_clip_container);
        lv_obj_set_size(watchface_snapshot, lv_pct(100), lv_pct(100));
        lv_obj_set_style_shadow_width(watchface_snapshot, 0, 0);
        lv_obj_set_style_margin_all(watchface_snapshot, 0, 0);
        lv_obj_set_style_pad_all(watchface_snapshot, 0, 0);
        lv_obj_center(watchface_snapshot);
        // Remove CLICKABLE flag to let touch events pass to parent object
        lv_obj_remove_flag(watchface_snapshot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(watchface_snapshot, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_image_set_src(watchface_snapshot, icon_path);
        eos_img_set_size(watchface_snapshot,
                 _SNAPSHOT_CONTAINER_W - (_SNAPSHOT_CONTAINER_PAD * 2),
                 _SNAPSHOT_CONTAINER_H - (_SNAPSHOT_CONTAINER_PAD * 2));
        lv_obj_center(watchface_snapshot);

        // Add click event handler to parent object so that when user clicks on image, click event passes to parent
        lv_obj_add_event_cb(item, _watchface_list_btn_cb, LV_EVENT_CLICKED, (void *)eos_watchface_list_get_id(i));
        // Ensure parent object is clickable
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(item, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        // Display name
        lv_obj_t *label = lv_label_create(item);
        if (watchface_id && strcmp(watchface_id, EOS_WATCHFACE_BUILTIN_FALLBACK_ID) == 0)
        {
            lv_label_set_text(label, "Fallback Watchface");
        }
        else
        {
            char manifest_path[EOS_FS_PATH_MAX];
            snprintf(manifest_path, sizeof(manifest_path), EOS_WATCHFACE_INSTALLED_DIR "%s/" EOS_WATCHFACE_MANIFEST_FILE_NAME,
                     watchface_id);
            script_pkg_t pkg = {0};
            pkg.type = SCRIPT_TYPE_WATCHFACE;
            if (script_engine_get_manifest(manifest_path, &pkg) != EOS_OK)
            {
                EOS_LOG_E("Read manifest failed: %s", manifest_path);
                eos_pkg_free(&pkg);
                continue;
            }
            lv_label_set_text(label, pkg.name);
            eos_pkg_free(&pkg);
        }
        lv_obj_set_width(label, LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_update_snap(cont, LV_ANIM_OFF);

    if (selected_item)
    {
        lv_obj_scroll_to_view(selected_item, LV_ANIM_OFF);
        lv_obj_update_snap(cont, LV_ANIM_OFF);
    }

    eos_free(current_wf_id);

    eos_activity_enter(a);
}
