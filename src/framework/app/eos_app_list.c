/**
 * @file eos_app_list.c
 * @brief App list page - using bubble_grid layout
 */

#include "eos_config.h"
#include "eos_app_list.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "cJSON.h"
#define EOS_LOG_TAG "AppList"
#include "eos_log.h"
#include "eos_app.h"
#include "eos_basic_widgets.h"
#include "eos_pkg_mgr.h"
#include "eos_image.h"
#include "eos_port.h"
#include "eos_anim.h"
#include "spm.h"
#include "eos_service_config.h"
#include "eos_event.h"
#include "eos_lang.h"
#include "eos_settings.h"
#include "eos_flash_light.h"
#include "eos_service_storage.h"
#include "eos_app_header.h"
#include "eos_mem.h"
#include "eos_crown.h"
#include "eos_theme.h"
#include "eos_icon.h"
#include "eos_font.h"
#include "eos_std_widgets.h"
#include "eos_activity.h"
#include "eos_bubble_grid.h"
#include "eos_accordion.h"
#include "eos_overlay_layer.h"
#ifdef EOS_ENABLE_TEST_APP
#include "eos_test.h"
#endif
/* Macros and Definitions -------------------------------------*/
#define _APP_ICON_ANIM_DURATION 200
#define _APP_ICON_ANIM_DELAY 75

#define _APP_LIST_ANIM_DURATION 350
#define _APP_LIST_ANIM_FOCUS_SCALE 1024
#define _APP_LIST_ANIM_MIN_SACLE 64
#define _APP_LIST_ANIM_SPLIT_PCT 15
#define _APP_LIST_ANIM_FROM_OPA_START 255
#define _APP_LIST_ANIM_FROM_OPA_END 0
#define _APP_LIST_ANIM_TO_OPA_START 0
#define _APP_LIST_ANIM_TO_OPA_END 255
/* Variables --------------------------------------------------*/

const char *eos_sys_app_id_list[EOS_SYS_APP_LAST] = {
    "sys.settings",
    "sys.flash_light",
#ifdef EOS_ENABLE_TEST_APP
    "sys.test"
#endif
};

const char *eos_sys_app_icon_list[EOS_SYS_APP_LAST] = {
    EOS_IMG_SETTINGS,
    EOS_IMG_FLASH_LIGHT,
#ifdef EOS_ENABLE_TEST_APP
    EOS_IMG_APP
#endif
};

const eos_sys_app_entry_t eos_sys_app_entry_list[EOS_SYS_APP_LAST] = {
    eos_settings_enter,
    eos_flash_light_enter,
#ifdef EOS_ENABLE_TEST_APP
    eos_test_start
#endif
};

static void _app_list_on_resueme(eos_activity_t *a);

static void _app_on_enter(eos_activity_t *a);
static eos_activity_lifecycle_t app_list_lifecycle = {
    .on_enter = NULL,
    .on_destroy = NULL,
    .on_pause = NULL,
    .on_resume = _app_list_on_resueme,
};

static void _app_on_destroy(eos_activity_t *a);

static eos_activity_lifecycle_t app_lifecycle = {
    .on_enter = _app_on_enter,
    .on_destroy = _app_on_destroy,
    .on_pause = NULL,
    .on_resume = NULL,
};

typedef struct
{
    script_pkg_t pkg;
    char *app_id;
} app_launch_ctx_t;

/* Function Implementations -----------------------------------*/
static void _app_list_icon_clicked_cb(lv_event_t *e);
static void _app_installed_cb(eos_event_t *e);
static void _app_uninstalled_cb(eos_event_t *e);
static void _container_delete_cb(lv_event_t *e);
static void _app_list_refresh(lv_obj_t *bubble_grid);
static void _app_list_open_app_anim_cb(lv_anim_timeline_t *at, eos_activity_t *from, eos_activity_t *to);
static void _app_list_close_app_anim_cb(lv_anim_timeline_t *at, eos_activity_t *from, eos_activity_t *to);
static void _register_anim_routes_once(void);
static const char *_app_list_get_launch_app_id(eos_activity_t *activity);
static void _app_list_set_last_launch_app_id(const char *app_id);
static lv_obj_t *_app_list_get_bubble_grid(eos_activity_t *activity);
static void _app_list_record_icon_center_point(int32_t x, int32_t y);
static bool _app_list_calc_focus_pivot(lv_obj_t *snapshot_obj, lv_obj_t *icon_obj, int32_t *pivot_x, int32_t *pivot_y);
static bool _app_list_calc_focus_pivot_by_global_center(lv_obj_t *obj, int32_t *pivot_x, int32_t *pivot_y);
static void _app_list_set_transform_scale_cb(void *var, int32_t value);
static void _app_list_set_image_scale_cb(void *var, int32_t value);
static void _app_list_set_translate_x_cb(void *var, int32_t value);
static void _app_list_set_translate_y_cb(void *var, int32_t value);
static void _app_list_set_opa_cb(void *var, int32_t value);
static void _app_list_init_scale_anim(lv_anim_t *anim, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration);
static void _app_list_init_image_scale_anim(lv_anim_t *anim, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration);
static void _app_list_init_translate_x_anim(lv_anim_t *anim, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration);
static void _app_list_init_translate_y_anim(lv_anim_t *anim, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration);
static void _app_list_init_opa_anim(lv_anim_t *anim, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration);
static void _app_list_play_transition_anim(lv_anim_timeline_t *at, eos_activity_t *from, eos_activity_t *to, bool opening);
static void _app_list_cleanup_extra_cb(lv_event_t *e);
static int32_t _app_list_find_sys_app(const char *app_id);
static eos_result_t _app_list_build_script_pkg(const char *app_id, script_pkg_t *pkg);
static eos_result_t _app_list_launch_script_app(const char *app_id);

static bool _anim_routes_registered = false;
static bool _app_list_last_icon_center_valid = false;
static int32_t _app_list_last_icon_center_x = 0;
static int32_t _app_list_last_icon_center_y = 0;
static int32_t _app_list_last_click_index = -1;
static char _app_list_last_launch_app_id[64] = {0};
static uint32_t _app_list_icon_count = 0;

/************************** Lifecycle **************************/

static void _app_on_destroy(eos_activity_t *a)
{
    // Stop the app script (if any) before destroying context
    spm_app_stop();

    app_launch_ctx_t *ctx = eos_activity_get_user_data(a);
    if (ctx)
    {
        eos_pkg_free(&ctx->pkg);
        eos_free(ctx->app_id);
        eos_free(ctx);
        eos_activity_set_user_data(a, NULL);
    }

}

static void _app_on_enter(eos_activity_t *a)
{
    app_launch_ctx_t *ctx = eos_activity_get_user_data(a);
    EOS_CHECK_PTR_RETURN(ctx);

    eos_result_t ret = spm_app_run(&ctx->pkg);
    if (ret != EOS_OK)
    {
        // Determine error type based on error code
        eos_script_error_type_t error_type = EOS_SCRIPT_FAULT_ERROR_EXCEPTION;
        if (ret == EOS_ERR_TIMEOUT) {
            error_type = EOS_SCRIPT_FAULT_UNRESPONSIVE;
        }
        else
        {
            // Check error info for timeout if code doesn't indicate it
            const char *error_info = script_engine_get_error_info();
            if (error_info && strstr(error_info, "timeout")) {
                error_type = EOS_SCRIPT_FAULT_UNRESPONSIVE;
            }
        }

        // Only handle error if it hasn't been handled already (timeout is handled inside script_engine_run)
        if (ret != EOS_ERR_TIMEOUT) {
            eos_app_handle_script_error(error_type, ret, ctx->app_id, NULL);
        }
        EOS_LOG_E("Application encounter a fatal error");
    }
}

static int32_t _app_list_find_sys_app(const char *app_id)
{
    if (!app_id)
    {
        return -1;
    }

    for (int32_t i = 0; i < EOS_SYS_APP_LAST; i++)
    {
        if (strcmp(app_id, eos_sys_app_id_list[i]) == 0)
        {
            return i;
        }
    }

    return -1;
}

static eos_result_t _app_list_build_script_pkg(const char *app_id, script_pkg_t *pkg)
{
    if (!(app_id && pkg))
    {
        return EOS_ERR_SCRIPT_NULL_PACKAGE;
    }

    char manifest_path[EOS_FS_PATH_MAX];
    snprintf(manifest_path, sizeof(manifest_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_MANIFEST_FILE_NAME,
             app_id);

    pkg->type = SCRIPT_TYPE_APPLICATION;
    if (script_engine_get_manifest(manifest_path, pkg) != EOS_OK)
    {
        EOS_LOG_E("Read manifest failed: %s", manifest_path);
        return EOS_FAILED;
    }

    char script_path[EOS_FS_PATH_MAX];
    snprintf(script_path, sizeof(script_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_SCRIPT_ENTRY_FILE_NAME,
             app_id);

    char base_path[EOS_FS_PATH_MAX];
    snprintf(base_path, sizeof(base_path), EOS_APP_INSTALLED_DIR "%s/", app_id);
    pkg->base_path = eos_strdup(base_path);
    if (!pkg->base_path)
    {
        eos_pkg_free(pkg);
        return EOS_ERR_MEM;
    }

    if (!eos_storage_is_file(script_path))
    {
        EOS_LOG_E("Can't find script: %s", script_path);
        eos_pkg_free(pkg);
        return EOS_FAILED;
    }

    pkg->script_str = eos_storage_read_file(script_path);
    if (!pkg->script_str)
    {
        EOS_LOG_E("Failed to read script: %s", script_path);
        eos_pkg_free(pkg);
        return EOS_FAILED;
    }

    return EOS_OK;
}

static eos_result_t _app_list_launch_script_app(const char *app_id)
{
    script_pkg_t pkg = {0};
    if (_app_list_build_script_pkg(app_id, &pkg) != EOS_OK)
    {
        return EOS_FAILED;
    }

    app_launch_ctx_t *ctx = eos_malloc_zeroed(sizeof(app_launch_ctx_t));
    if (!ctx)
    {
        EOS_LOG_E("Failed to allocate app launch context");
        eos_pkg_free(&pkg);
        return EOS_FAILED;
    }

    ctx->pkg = pkg;
    ctx->app_id = eos_strdup(app_id);
    if (!ctx->app_id)
    {
        EOS_LOG_E("Failed to copy app id");
        eos_pkg_free(&ctx->pkg);
        eos_free(ctx);
        return EOS_FAILED;
    }

    eos_activity_t *a = eos_activity_create(&app_lifecycle);
    if (!a)
    {
        EOS_LOG_E("Failed to create activity");
        eos_pkg_free(&ctx->pkg);
        eos_free(ctx->app_id);
        eos_free(ctx);
        return EOS_FAILED;
    }

    lv_obj_t *app_view = eos_activity_get_view(a);
    lv_obj_set_size(app_view, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    eos_activity_set_type(a, EOS_ACTIVITY_TYPE_APP);
    eos_activity_set_user_data(a, ctx);
    eos_activity_set_title(a, pkg.name);
    eos_activity_set_app_header_visible(a, true);

    EOS_LOG_D("view_size: %d, %d", lv_obj_get_width(app_view), lv_obj_get_height(app_view));

    eos_activity_enter(a);
    return EOS_OK;
}

/**
 * @brief Return to app list from within an app and any sub-activities
 * @return eos_result_t EOS_OK success, EOS_FAILED failed or not in app
 * @note If not in app context, returns EOS_FAILED; otherwise clears the activity stack
 */
static eos_result_t _app_list_pop_to_app_list(void)
{
    eos_activity_t *current = eos_activity_get_current();
    if (!current)
    {
        return EOS_FAILED;
    }

    eos_activity_type_t current_type = eos_activity_get_type(current);

    // If already at app list, no need to pop
    if (current_type == EOS_ACTIVITY_TYPE_APP_LIST)
    {
        return EOS_OK;
    }

    // If not in app context (not in app or app list), return failure
    // This prevents unexpected navigation from watchface or other contexts
    if (current_type != EOS_ACTIVITY_TYPE_APP)
    {
        return EOS_FAILED;
    }

    // Currently in app (possibly with sub-activities), return to watchface
    // This will clean up all app-related activities on the stack
    eos_activity_back_to_watchface();
    return EOS_OK;
}

eos_result_t eos_app_launch_immediately(const char *app_id)
{
    if (!(app_id && app_id[0]))
    {
        EOS_LOG_E("Invalid app id");
        return EOS_FAILED;
    }

    if (eos_activity_is_transition_in_progress())
    {
        EOS_LOG_W("Cannot launch app while activity transition is in progress");
        return EOS_FAILED;
    }

    _register_anim_routes_once();
    _app_list_set_last_launch_app_id(app_id);

    int32_t sys_app_index = _app_list_find_sys_app(app_id);
    if (sys_app_index >= 0)
    {
        if (eos_sys_app_entry_list[sys_app_index])
        {
            eos_sys_app_entry_list[sys_app_index]();
            return EOS_OK;
        }
        return EOS_FAILED;
    }

    if (!eos_app_list_contains(app_id))
    {
        EOS_LOG_E("App not found: %s", app_id);
        return EOS_FAILED;
    }

    // If currently inside an app (or app sub-activity), pop back to watchface first
    // This ensures all app-internal activities are cleaned up before launching new app
    eos_activity_type_t current_type = eos_activity_get_type(eos_activity_get_current());
    if (current_type == EOS_ACTIVITY_TYPE_APP)
    {
        EOS_LOG_I("Returning to app list before launching new app");
        _app_list_pop_to_app_list();
    }


    return _app_list_launch_script_app(app_id);
}

static const char *_app_list_get_launch_app_id(eos_activity_t *activity)
{
    app_launch_ctx_t *ctx = eos_activity_get_user_data(activity);
    if (ctx && ctx->app_id)
    {
        return ctx->app_id;
    }

    return _app_list_last_launch_app_id[0] ? _app_list_last_launch_app_id : NULL;
}

static void _app_list_set_last_launch_app_id(const char *app_id)
{
    if (!app_id)
    {
        _app_list_last_launch_app_id[0] = '\0';
        return;
    }

    snprintf(_app_list_last_launch_app_id,
             sizeof(_app_list_last_launch_app_id),
             "%s",
             app_id);
}

static lv_obj_t *_app_list_get_bubble_grid(eos_activity_t *activity)
{
    if (!activity)
    {
        return NULL;
    }

    lv_obj_t *bubble_grid = (lv_obj_t *)eos_activity_get_user_data(activity);
    if (bubble_grid)
    {
        return bubble_grid;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return NULL;
    }

    return lv_obj_get_child(view, 0);
}

static void _app_list_record_icon_center_point(int32_t x, int32_t y)
{
    _app_list_last_icon_center_x = x;
    _app_list_last_icon_center_y = y;
    _app_list_last_icon_center_valid = true;
}

static bool _app_list_calc_focus_pivot_by_global_center(lv_obj_t *obj, int32_t *pivot_x, int32_t *pivot_y)
{
    if (!(obj && pivot_x && pivot_y && _app_list_last_icon_center_valid))
    {
        return false;
    }

    int32_t max_x = lv_obj_get_width(obj);
    int32_t max_y = lv_obj_get_height(obj);
    int32_t local_x = _app_list_last_icon_center_x;
    int32_t local_y = _app_list_last_icon_center_y;

    if (local_x < 0)
        local_x = 0;
    if (local_y < 0)
        local_y = 0;

    /* Defensive: if the object reports zero width/height (e.g. snapshot
     * image not yet laid out), use the known display dimensions. */
    if (max_x <= 0)
        max_x = EOS_DISPLAY_WIDTH;
    if (max_y <= 0)
        max_y = EOS_DISPLAY_HEIGHT;

    if (local_x > max_x)
        local_x = max_x;
    if (local_y > max_y)
        local_y = max_y;

    *pivot_x = local_x;
    *pivot_y = local_y;
    return true;
}

static bool _app_list_calc_focus_pivot(lv_obj_t *snapshot_obj, lv_obj_t *icon_obj, int32_t *pivot_x, int32_t *pivot_y)
{
    if (!(snapshot_obj && pivot_x && pivot_y))
    {
        return false;
    }

    lv_area_t snapshot_area;
    lv_obj_get_coords(snapshot_obj, &snapshot_area);

    if (!icon_obj)
    {
        *pivot_x = lv_area_get_width(&snapshot_area) / 2;
        *pivot_y = lv_area_get_height(&snapshot_area) / 2;
        return false;
    }

    lv_area_t icon_area;
    lv_obj_get_coords(icon_obj, &icon_area);

    int32_t icon_mid_x = icon_area.x1 + (lv_area_get_width(&icon_area) / 2);
    int32_t icon_mid_y = icon_area.y1 + (lv_area_get_height(&icon_area) / 2);

    *pivot_x = icon_mid_x - snapshot_area.x1;
    *pivot_y = icon_mid_y - snapshot_area.y1;
    return true;
}

static void _app_list_set_transform_scale_cb(void *var, int32_t value)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)var, value, 0);
}

static void _app_list_set_image_scale_cb(void *var, int32_t value)
{
    lv_image_set_scale((lv_obj_t *)var, value);
}

static void _app_list_set_translate_x_cb(void *var, int32_t value)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, value, 0);
}

static void _app_list_set_translate_y_cb(void *var, int32_t value)
{
    lv_obj_set_style_translate_y((lv_obj_t *)var, value, 0);
}

static void _app_list_set_opa_cb(void *var, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)value, 0);
}

static void _app_list_init_scale_anim(lv_anim_t *anim, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration)
{
    lv_anim_init(anim);
    lv_anim_set_var(anim, obj);
    lv_anim_set_values(anim, start, end);
    lv_anim_set_exec_cb(anim, _app_list_set_transform_scale_cb);
    lv_anim_set_path_cb(anim, lv_anim_path_ease_in_out);
    lv_anim_set_duration(anim, duration);
}

static void _app_list_init_image_scale_anim(lv_anim_t *anim, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration)
{
    lv_anim_init(anim);
    lv_anim_set_var(anim, obj);
    lv_anim_set_values(anim, start, end);
    lv_anim_set_exec_cb(anim, _app_list_set_image_scale_cb);
    lv_anim_set_path_cb(anim, lv_anim_path_ease_in_out);
    lv_anim_set_duration(anim, duration);
}

static void _app_list_init_translate_x_anim(lv_anim_t *anim, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration)
{
    lv_anim_init(anim);
    lv_anim_set_var(anim, obj);
    lv_anim_set_values(anim, start, end);
    lv_anim_set_exec_cb(anim, _app_list_set_translate_x_cb);
    lv_anim_set_path_cb(anim, lv_anim_path_ease_in_out);
    lv_anim_set_duration(anim, duration);
}

static void _app_list_init_translate_y_anim(lv_anim_t *anim, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration)
{
    lv_anim_init(anim);
    lv_anim_set_var(anim, obj);
    lv_anim_set_values(anim, start, end);
    lv_anim_set_exec_cb(anim, _app_list_set_translate_y_cb);
    lv_anim_set_path_cb(anim, lv_anim_path_ease_in_out);
    lv_anim_set_duration(anim, duration);
}

static void _app_list_init_opa_anim(lv_anim_t *anim, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration)
{
    lv_anim_init(anim);
    lv_anim_set_var(anim, obj);
    lv_anim_set_values(anim, start, end);
    lv_anim_set_exec_cb(anim, _app_list_set_opa_cb);
    lv_anim_set_path_cb(anim, lv_anim_path_ease_in_out);
    lv_anim_set_duration(anim, duration);
}

static void _app_list_cleanup_extra_cb(lv_event_t *e)
{
    lv_obj_t *extra = (lv_obj_t *)lv_event_get_user_data(e);
    if (extra && lv_obj_is_valid(extra))
    {
        lv_obj_delete(extra);
    }
}

static lv_obj_t *_app_list_create_icon_clone(lv_obj_t *focus_icon)
{
    if (!focus_icon)
    {
        return NULL;
    }

    lv_obj_t *icon_img = lv_obj_get_child(focus_icon, 0);
    if (!icon_img)
    {
        return NULL;
    }

    const void *img_src = lv_image_get_src(icon_img);
    if (!img_src)
    {
        return NULL;
    }

    lv_area_t icon_coords;
    lv_obj_get_coords(focus_icon, &icon_coords);
    int32_t bw = lv_area_get_width(&icon_coords);
    int32_t bh = lv_area_get_height(&icon_coords);

    lv_obj_t *icon_clone = lv_obj_create(eos_overlay_get_snapshot_layer());
    lv_obj_set_size(icon_clone, bw, bh);
    lv_obj_set_pos(icon_clone, icon_coords.x1, icon_coords.y1);
    lv_obj_set_style_radius(icon_clone, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(icon_clone, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(icon_clone, lv_obj_get_style_bg_color(focus_icon, 0), 0);
    lv_obj_set_style_border_width(icon_clone, 0, 0);
    lv_obj_set_style_pad_all(icon_clone, 0, 0);
    lv_obj_set_style_clip_corner(icon_clone, true, 0);
    lv_obj_set_style_transform_pivot_x(icon_clone, bw / 2, 0);
    lv_obj_set_style_transform_pivot_y(icon_clone, bh / 2, 0);
    lv_obj_remove_flag(icon_clone, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(icon_clone, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon_img_clone = lv_image_create(icon_clone);
    lv_image_set_src(icon_img_clone, img_src);
    lv_image_set_scale_x(icon_img_clone, lv_image_get_scale_x(icon_img));
    lv_image_set_scale_y(icon_img_clone, lv_image_get_scale_y(icon_img));
    lv_obj_set_size(icon_img_clone, bw, bh);
    lv_image_set_inner_align(icon_img_clone, LV_IMAGE_ALIGN_CENTER);
    lv_obj_center(icon_img_clone);
    lv_obj_set_style_bg_opa(icon_img_clone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon_img_clone, 0, 0);
    lv_obj_remove_flag(icon_img_clone, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(icon_img_clone, LV_OBJ_FLAG_CLICKABLE);

    return icon_clone;
}

static void _app_list_play_transition_anim(lv_anim_timeline_t *at, eos_activity_t *from, eos_activity_t *to, bool opening)
{
    if (!(at && from && to))
    {
        return;
    }

    lv_obj_t *list_view = opening ? eos_activity_get_view(from) : eos_activity_get_view(to);
    lv_obj_t *bubble_grid = _app_list_get_bubble_grid(opening ? from : to);
    lv_obj_t *focus_icon = NULL;
    if (bubble_grid && _app_list_last_click_index >= 0)
    {
        focus_icon = eos_bubble_get_icon_obj(bubble_grid, (uint32_t)_app_list_last_click_index);
    }

    /* Closing animation should not render app header on top. */
    bool include_header_in_snapshot = opening;

    lv_obj_t *list_snapshot = NULL;
    bool focus_icon_hidden_flag = false;
    if (focus_icon && lv_obj_has_flag(focus_icon, LV_OBJ_FLAG_HIDDEN))
    {
        focus_icon_hidden_flag = true;
    }

    lv_obj_t *icon_clone = NULL;
    lv_obj_t *app_snapshot = NULL;

    if (opening)
    {
        icon_clone = _app_list_create_icon_clone(focus_icon);

        if (focus_icon)
        {
            lv_obj_add_flag(focus_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else
    {
        app_snapshot = eos_activity_take_snapshot(from, include_header_in_snapshot);
        if (!app_snapshot)
        {
            return;
        }
        lv_obj_move_foreground(app_snapshot);

        if (list_view)
        {
            lv_obj_remove_flag(list_view, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(list_view);
        }

        if (focus_icon)
        {
            lv_area_t area;
            lv_obj_get_coords(focus_icon, &area);
            _app_list_record_icon_center_point(
                area.x1 + lv_area_get_width(&area) / 2,
                area.y1 + lv_area_get_height(&area) / 2);
        }

        icon_clone = _app_list_create_icon_clone(focus_icon);
        if (icon_clone)
        {
            lv_obj_set_style_opa(icon_clone, LV_OPA_TRANSP, 0);
        }

        if (focus_icon)
        {
            lv_obj_add_flag(focus_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }

    eos_activity_t *list_activity = opening ? from : to;
    if (list_activity)
    {
        list_snapshot = eos_activity_take_snapshot(list_activity, false);
    }

    if (!opening && app_snapshot)
    {
        lv_obj_move_foreground(app_snapshot);
    }

    if (icon_clone)
    {
        lv_obj_move_foreground(icon_clone);
    }

    if (focus_icon && !focus_icon_hidden_flag)
    {
        lv_obj_remove_flag(focus_icon, LV_OBJ_FLAG_HIDDEN);
    }

    if (opening)
    {
        app_snapshot = eos_activity_take_snapshot(to, include_header_in_snapshot);
        if (!app_snapshot)
        {
            if (icon_clone)
            {
                lv_obj_delete(icon_clone);
                icon_clone = NULL;
            }
            return;
        }
    }

    int32_t list_pivot_x = 0;
    int32_t list_pivot_y = 0;
    int32_t app_pivot_x = 0;
    int32_t app_pivot_y = 0;

    if (list_snapshot)
    {
        if (!_app_list_calc_focus_pivot_by_global_center(list_snapshot, &list_pivot_x, &list_pivot_y))
        {
            _app_list_calc_focus_pivot(list_snapshot, focus_icon, &list_pivot_x, &list_pivot_y);
        }
        lv_image_set_pivot(list_snapshot, list_pivot_x, list_pivot_y);
    }

    if (!_app_list_calc_focus_pivot_by_global_center(app_snapshot, &app_pivot_x, &app_pivot_y))
    {
        _app_list_calc_focus_pivot(app_snapshot, focus_icon, &app_pivot_x, &app_pivot_y);
    }
    lv_image_set_pivot(app_snapshot, app_pivot_x, app_pivot_y);

    if (icon_clone)
    {
        lv_obj_move_foreground(icon_clone);
    }
    lv_obj_move_foreground(app_snapshot);

    uint32_t total_duration = (uint32_t)_APP_LIST_ANIM_DURATION;
    if (total_duration == 0U)
    {
        total_duration = 1U;
    }

    uint32_t split_delay = (total_duration * (uint32_t)_APP_LIST_ANIM_SPLIT_PCT) / 100U;
    if (split_delay >= total_duration)
    {
        split_delay = total_duration > 1U ? total_duration - 1U : 0U;
    }

    uint32_t from_scale_duration = total_duration;
    uint32_t to_duration = (total_duration > split_delay) ? (total_duration - split_delay) : 1U;

    int32_t focus_translate_x = 0;
    int32_t focus_translate_y = 0;
    if (_app_list_last_icon_center_valid)
    {
        int32_t view_center_x = EOS_DISPLAY_WIDTH / 2;
        int32_t view_center_y = EOS_DISPLAY_HEIGHT / 2;
        focus_translate_x = view_center_x - _app_list_last_icon_center_x;
        focus_translate_y = view_center_y - _app_list_last_icon_center_y;
    }

    lv_anim_t list_scale_anim;
    lv_anim_t list_translate_x_anim;
    lv_anim_t list_translate_y_anim;
    lv_anim_t icon_scale_anim;
    lv_anim_t icon_translate_x_anim;
    lv_anim_t icon_translate_y_anim;
    lv_anim_t icon_opa_anim;
    lv_anim_t app_scale_anim;
    lv_anim_t app_opa_anim;

    if (opening)
    {
        if (list_snapshot)
        {
            lv_image_set_scale(list_snapshot, 256);
            lv_obj_set_style_translate_x(list_snapshot, 0, 0);
            lv_obj_set_style_translate_y(list_snapshot, 0, 0);
            _app_list_init_image_scale_anim(&list_scale_anim, list_snapshot, 256, _APP_LIST_ANIM_FOCUS_SCALE, from_scale_duration);
            _app_list_init_translate_x_anim(&list_translate_x_anim, list_snapshot, 0, focus_translate_x, from_scale_duration);
            _app_list_init_translate_y_anim(&list_translate_y_anim, list_snapshot, 0, focus_translate_y, from_scale_duration);
            lv_anim_timeline_add(at, 0, &list_scale_anim);
            lv_anim_timeline_add(at, 0, &list_translate_x_anim);
            lv_anim_timeline_add(at, 0, &list_translate_y_anim);
        }

        if (icon_clone)
        {
            lv_obj_set_style_transform_scale(icon_clone, 256, 0);
            lv_obj_set_style_translate_x(icon_clone, 0, 0);
            lv_obj_set_style_translate_y(icon_clone, 0, 0);
            lv_obj_set_style_opa(icon_clone, (lv_opa_t)_APP_LIST_ANIM_FROM_OPA_START, 0);
            _app_list_init_scale_anim(&icon_scale_anim, icon_clone, 256, _APP_LIST_ANIM_FOCUS_SCALE, from_scale_duration);
            _app_list_init_translate_x_anim(&icon_translate_x_anim, icon_clone, 0, focus_translate_x, from_scale_duration);
            _app_list_init_translate_y_anim(&icon_translate_y_anim, icon_clone, 0, focus_translate_y, from_scale_duration);
            _app_list_init_opa_anim(&icon_opa_anim,
                                    icon_clone,
                                    _APP_LIST_ANIM_FROM_OPA_START,
                                    _APP_LIST_ANIM_FROM_OPA_END,
                                    total_duration);
            lv_anim_timeline_add(at, 0, &icon_scale_anim);
            lv_anim_timeline_add(at, 0, &icon_translate_x_anim);
            lv_anim_timeline_add(at, 0, &icon_translate_y_anim);
            lv_anim_timeline_add(at, 0, &icon_opa_anim);
        }

        lv_image_set_scale(app_snapshot, _APP_LIST_ANIM_MIN_SACLE);
        lv_obj_set_style_opa(app_snapshot, (lv_opa_t)_APP_LIST_ANIM_TO_OPA_START, 0);
        _app_list_init_image_scale_anim(&app_scale_anim, app_snapshot, _APP_LIST_ANIM_MIN_SACLE, 256, to_duration);
        _app_list_init_opa_anim(&app_opa_anim,
                                app_snapshot,
                                _APP_LIST_ANIM_TO_OPA_START,
                                _APP_LIST_ANIM_TO_OPA_END,
                                to_duration);
        lv_anim_timeline_add(at, split_delay, &app_scale_anim);
        lv_anim_timeline_add(at, split_delay, &app_opa_anim);
    }
    else
    {
        if (list_snapshot)
        {
            lv_image_set_scale(list_snapshot, _APP_LIST_ANIM_FOCUS_SCALE);
            lv_obj_set_style_translate_x(list_snapshot, focus_translate_x, 0);
            lv_obj_set_style_translate_y(list_snapshot, focus_translate_y, 0);
            _app_list_init_image_scale_anim(&list_scale_anim, list_snapshot, _APP_LIST_ANIM_FOCUS_SCALE, 256, to_duration);
            _app_list_init_translate_x_anim(&list_translate_x_anim, list_snapshot, focus_translate_x, 0, to_duration);
            _app_list_init_translate_y_anim(&list_translate_y_anim, list_snapshot, focus_translate_y, 0, to_duration);
            lv_anim_timeline_add(at, split_delay, &list_scale_anim);
            lv_anim_timeline_add(at, split_delay, &list_translate_x_anim);
            lv_anim_timeline_add(at, split_delay, &list_translate_y_anim);
        }

        if (icon_clone)
        {
            lv_obj_set_style_transform_scale(icon_clone, _APP_LIST_ANIM_FOCUS_SCALE, 0);
            lv_obj_set_style_translate_x(icon_clone, focus_translate_x, 0);
            lv_obj_set_style_translate_y(icon_clone, focus_translate_y, 0);
            lv_obj_set_style_opa(icon_clone, (lv_opa_t)_APP_LIST_ANIM_FROM_OPA_END, 0);
            _app_list_init_scale_anim(&icon_scale_anim, icon_clone, _APP_LIST_ANIM_FOCUS_SCALE, 256, to_duration);
            _app_list_init_translate_x_anim(&icon_translate_x_anim, icon_clone, focus_translate_x, 0, to_duration);
            _app_list_init_translate_y_anim(&icon_translate_y_anim, icon_clone, focus_translate_y, 0, to_duration);
            _app_list_init_opa_anim(&icon_opa_anim,
                                    icon_clone,
                                    _APP_LIST_ANIM_FROM_OPA_END,
                                    _APP_LIST_ANIM_FROM_OPA_START,
                                    to_duration);
            lv_anim_timeline_add(at, split_delay, &icon_scale_anim);
            lv_anim_timeline_add(at, split_delay, &icon_translate_x_anim);
            lv_anim_timeline_add(at, split_delay, &icon_translate_y_anim);
            lv_anim_timeline_add(at, split_delay, &icon_opa_anim);
        }

        lv_image_set_scale(app_snapshot, 256);
        lv_obj_set_style_opa(app_snapshot, (lv_opa_t)_APP_LIST_ANIM_TO_OPA_END, 0);
        _app_list_init_image_scale_anim(&app_scale_anim, app_snapshot, 256, _APP_LIST_ANIM_MIN_SACLE, to_duration);
        _app_list_init_opa_anim(&app_opa_anim,
                                app_snapshot,
                                _APP_LIST_ANIM_TO_OPA_END,
                                _APP_LIST_ANIM_TO_OPA_START,
                                to_duration);
        lv_anim_timeline_add(at, 0, &app_scale_anim);
        lv_anim_timeline_add(at, 0, &app_opa_anim);
    }

    if (icon_clone && app_snapshot)
    {
        lv_obj_add_event_cb(app_snapshot, _app_list_cleanup_extra_cb, LV_EVENT_DELETE, icon_clone);
    }
}

static void _app_list_on_resueme(eos_activity_t *a)
{
    // Initialize app list
    lv_obj_t *bubble_grid = _app_list_get_bubble_grid(a);
    EOS_CHECK_PTR_RETURN(bubble_grid);
    _app_list_refresh(bubble_grid);
}

/************************** App Entry **************************/
/**
 * @brief App click event callback (handles system apps and script apps)
 * Gets app ID from bubble_grid's LV_EVENT_CLICKED event
 */
static void _app_list_icon_clicked_cb(lv_event_t *e)
{
    lv_obj_t *bubble_grid = lv_event_get_current_target(e);
    EOS_CHECK_PTR_RETURN(bubble_grid);

    eos_bubble_click_event_t *click_event = (eos_bubble_click_event_t *)lv_event_get_param(e);
    EOS_CHECK_PTR_RETURN(click_event);

    const char *app_id = (const char *)click_event->icon_user_data;
    EOS_CHECK_PTR_RETURN(app_id);

    _app_list_set_last_launch_app_id(app_id);
    _app_list_last_click_index = (int32_t)click_event->index;

    /* Use the icon's center, not the click point, so animation pivot/translate
     * is consistent regardless of where the user touches. */
    lv_obj_t *clicked_bubble = eos_bubble_get_icon_obj(bubble_grid, click_event->index);
    if (clicked_bubble)
    {
        lv_area_t area;
        lv_obj_get_coords(clicked_bubble, &area);
        _app_list_record_icon_center_point(
            area.x1 + lv_area_get_width(&area) / 2,
            area.y1 + lv_area_get_height(&area) / 2);
    }

    if (eos_app_launch_immediately(app_id) != EOS_OK)
    {
        EOS_LOG_E("Launch app failed: %s", app_id);
    }
}

static void _register_anim_routes_once(void)
{
    if (_anim_routes_registered)
    {
        return;
    }

    eos_activity_register_anim_route(EOS_ACTIVITY_TYPE_APP_LIST, EOS_ACTIVITY_TYPE_APP, _app_list_open_app_anim_cb);
    eos_activity_register_anim_route(EOS_ACTIVITY_TYPE_APP, EOS_ACTIVITY_TYPE_APP_LIST, _app_list_close_app_anim_cb);
    _anim_routes_registered = true;
}

/************************** Refresh App List **************************/
/**
 * @brief Refresh app list - using bubble_grid
 * @param bubble_grid App list's bubble_grid object
 */
static void _app_list_refresh(lv_obj_t *bubble_grid)
{
    if (!bubble_grid)
    {
        return;
    }

    // Clear previous icon slots to avoid dangling pointers from deleting internal objects.
    for (uint32_t i = 0; i < _app_list_icon_count; ++i)
    {
        eos_bubble_set_icon_src(bubble_grid, i, NULL);
        eos_bubble_set_icon_user_data(bubble_grid, i, NULL);
    }

    uint32_t icon_index = 0;

    // Load application order from config
    cJSON *app_order = eos_config_get_json(EOS_CONFIG_KEY_APP_ORDER_ARRAY);

    // Add icons according to JSON order
    if (app_order && cJSON_IsArray(app_order))
    {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, app_order)
        {
            if (cJSON_IsString(item))
            {
                const char *order_id = item->valuestring;

                // If it's a system app, use built-in icon and skip installed app check
                bool is_sys = false;
                for (int si = 0; si < EOS_SYS_APP_LAST; si++)
                {
                    if (strcmp(order_id, eos_sys_app_id_list[si]) == 0)
                    {
                        eos_bubble_set_icon_src(bubble_grid, icon_index, eos_sys_app_icon_list[si]);
                        eos_bubble_set_icon_user_data(bubble_grid, icon_index, (void *)eos_sys_app_id_list[si]);
                        icon_index++;
                        is_sys = true;
                        break;
                    }
                }
                if (is_sys)
                    continue;

                // Non-system app: look up existing ID in installed list
                const char *app_id = eos_app_list_get_existing_id(order_id);
                if (!app_id)
                {
                    continue;
                }

                char icon_path[EOS_FS_PATH_MAX];
                snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME,
                         app_id);
                if (!eos_storage_is_file(icon_path))
                {
                    snprintf(icon_path, sizeof(icon_path), "%s", EOS_IMG_APP);
                }
                eos_bubble_set_icon_src(bubble_grid, icon_index, icon_path);
                eos_bubble_set_icon_user_data(bubble_grid, icon_index, (void *)app_id);
                icon_index++;
            }
        }
        cJSON_Delete(app_order);
    }
    else
    {
        // If no JSON order file is found, add apps in default order
        size_t app_list_size = eos_app_get_installed();
        for (size_t i = 0; i < app_list_size; i++)
        {
            const char *app_id = eos_app_list_get_id(i);
            if (!app_id)
                continue;

            // System built-in apps use built-in icons
            bool is_sys = false;
            for (int si = 0; si < EOS_SYS_APP_LAST; si++)
            {
                if (strcmp(app_id, eos_sys_app_id_list[si]) == 0)
                {
                    eos_bubble_set_icon_src(bubble_grid, icon_index, eos_sys_app_icon_list[si]);
                    eos_bubble_set_icon_user_data(bubble_grid, icon_index, (void *)eos_sys_app_id_list[si]);
                    icon_index++;
                    is_sys = true;
                    break;
                }
            }
            if (is_sys)
                continue;

            // Non-system app
            char icon_path[EOS_FS_PATH_MAX];
            snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME,
                     app_id);
            if (!eos_storage_is_file(icon_path))
            {
                snprintf(icon_path, sizeof(icon_path), "%s", EOS_IMG_APP);
            }
            eos_bubble_set_icon_src(bubble_grid, icon_index, icon_path);
            eos_bubble_set_icon_user_data(bubble_grid, icon_index, (void *)app_id);
            icon_index++;
        }
    }

    _app_list_icon_count = icon_index;
}

/************************** Animation **************************/

static void _app_list_open_app_anim_cb(lv_anim_timeline_t *at, eos_activity_t *from, eos_activity_t *to)
{
    _app_list_play_transition_anim(at, from, to, true);
}

static void _app_list_close_app_anim_cb(lv_anim_timeline_t *at, eos_activity_t *from, eos_activity_t *to)
{
    _app_list_play_transition_anim(at, from, to, false);
}

/************************** Helper Functions **************************/

/**
 * @brief This callback is automatically called when an app is installed to display the new app
 */
static void _app_installed_cb(eos_event_t *e)
{
    lv_obj_t *bubble_grid = eos_event_get_obj(e);
    EOS_CHECK_PTR_RETURN(bubble_grid);
    _app_list_refresh(bubble_grid);
}

static void _app_uninstalled_cb(eos_event_t *e)
{
    lv_obj_t *bubble_grid = eos_event_get_obj(e);
    EOS_CHECK_PTR_RETURN(bubble_grid);
    _app_list_refresh(bubble_grid);
}

static void _container_delete_cb(lv_event_t *e)
{
    lv_obj_t *bubble_grid = lv_event_get_target(e);
    EOS_CHECK_PTR_RETURN(bubble_grid);
    eos_event_unsubscribe_with_obj(EOS_EVENT_APP_INSTALLED, _app_installed_cb, bubble_grid);
    eos_event_unsubscribe_with_obj(EOS_EVENT_APP_UNINSTALLED, _app_uninstalled_cb, bubble_grid);
}

void eos_app_list_enter(void)
{
    _register_anim_routes_once();
    _app_list_icon_count = 0;

    eos_activity_t *a = eos_activity_create(&app_list_lifecycle);
    if (!a)
    {
        EOS_LOG_E("Failed to create activity");
        return;
    }
    eos_activity_set_type(a, EOS_ACTIVITY_TYPE_APP_LIST);

    lv_obj_t *view = eos_activity_get_view(a);
    lv_obj_set_size(view, lv_pct(100), lv_pct(100));

    // Create bubble_grid as app list container
    lv_obj_t *bubble_grid = eos_bubble_create(view);
    if (!bubble_grid)
    {
        EOS_LOG_E("Failed to create bubble_grid");
        eos_activity_back();
        return;
    }

    // Set bubble_grid size and position
    lv_obj_set_size(bubble_grid, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_center(bubble_grid);
    eos_activity_set_user_data(a, bubble_grid);

    // Register click event callback
    lv_obj_add_event_cb(bubble_grid, _app_list_icon_clicked_cb, LV_EVENT_CLICKED, NULL);

    // Set callback
    lv_obj_add_event_cb(bubble_grid, _container_delete_cb, LV_EVENT_DELETE, NULL);
    eos_event_subscribe_ex(EOS_EVENT_APP_INSTALLED, _app_installed_cb, NULL, bubble_grid);
    eos_event_subscribe_ex(EOS_EVENT_APP_UNINSTALLED, _app_uninstalled_cb, NULL, bubble_grid);

    // Refresh app list
    _app_list_refresh(bubble_grid);

    eos_activity_enter(a);
}
