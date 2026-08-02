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
#define _APP_LIST_ANIM_DURATION 350
#define _APP_LIST_ANIM_FOCUS_SCALE 2048
#define _APP_LIST_ANIM_MIN_SACLE 64

/* ease_out_quint cubic-bezier: (0.22, 1, 0.36, 1) in LVGL fixed-point */
#define _EASE_OUT_QUINT_BX1 225
#define _EASE_OUT_QUINT_BY1 1024
#define _EASE_OUT_QUINT_BX2 368
#define _EASE_OUT_QUINT_BY2 1024
/* Variables --------------------------------------------------*/

const char *eos_sys_app_id_list[EOS_SYS_APP_LAST] = {"sys.settings",
                                                     "sys.flash_light",
#ifdef EOS_ENABLE_TEST_APP
                                                     "sys.test"
#endif
};

const char *eos_sys_app_icon_list[EOS_SYS_APP_LAST] = {EOS_IMG_SETTINGS,
                                                       EOS_IMG_FLASH_LIGHT,
#ifdef EOS_ENABLE_TEST_APP
                                                       EOS_IMG_APP
#endif
};

const eos_sys_app_entry_t eos_sys_app_entry_list[EOS_SYS_APP_LAST] = {eos_settings_enter,
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
static void _app_list_open_app_anim_cb(eos_anim_group_t *group, eos_activity_t *from, eos_activity_t *to);
static void _app_list_close_app_anim_cb(eos_anim_group_t *group, eos_activity_t *from, eos_activity_t *to);
static void _register_anim_routes_once(void);
static const char *_app_list_get_launch_app_id(eos_activity_t *activity);
static void _app_list_set_last_launch_app_id(const char *app_id);
static lv_obj_t *_app_list_get_bubble_grid(eos_activity_t *activity);
static void _app_list_record_icon_center_point(int32_t x, int32_t y);
static bool _app_list_calc_focus_pivot(lv_obj_t *snapshot_obj, lv_obj_t *icon_obj, int32_t *pivot_x, int32_t *pivot_y);
static bool _app_list_calc_focus_pivot_by_global_center(lv_obj_t *obj, int32_t *pivot_x, int32_t *pivot_y);
static void _app_list_play_transition_anim(eos_anim_group_t *group,
                                           eos_activity_t *from,
                                           eos_activity_t *to,
                                           bool opening);
static void _app_list_cleanup_extra_cb(lv_event_t *e);
static void _app_list_paired_fade_exec_cb(void *var, int32_t v);
static void _app_list_paired_fade_ready_cb(lv_anim_t *a);
static void _app_list_start_paired_fade(lv_obj_t *obj_a,
                                        lv_obj_t *obj_b,
                                        lv_obj_t *icon_clone,
                                        lv_obj_t *app_snapshot,
                                        int32_t focus_translate_x,
                                        int32_t focus_translate_y,
                                        bool opening,
                                        uint32_t duration,
                                        eos_anim_group_t *group,
                                        lv_obj_t *focus_icon,
                                        uint32_t delay);
static int32_t _app_list_find_sys_app(const char *app_id);
static eos_result_t _app_list_build_script_pkg(const char *app_id, script_pkg_t *pkg);
static eos_result_t _app_list_launch_script_app(const char *app_id);
static void _app_list_restart_cb(lv_event_t *e);
static void _app_list_exit_cb(lv_event_t *e);

static bool _anim_routes_registered = false;
static bool _app_list_last_icon_center_valid = false;
static int32_t _app_list_last_icon_center_x = 0;
static int32_t _app_list_last_icon_center_y = 0;
static int32_t _app_list_last_click_index = -1;
static char _app_list_last_launch_app_id[64] = {0};
static uint32_t _app_list_icon_count = 0;

/* Paired crossfade context — one master lv_anim drives opacity + translate in sync.
 * obj_a fades 255→0, obj_b fades 0→255 (complementary).
 * icon_clone and app_snapshot translates to keep their centers aligned. */
typedef struct
{
    lv_obj_t *obj_a; /* fades out (255→0) */
    lv_obj_t *obj_b; /* fades in  (0→255) */
    lv_obj_t *icon_clone; /* icon clone object */
    lv_obj_t *app_snapshot; /* app snapshot object */
    int32_t icon_trans_start_x; /* icon_clone translate_x at t=0 */
    int32_t icon_trans_start_y; /* icon_clone translate_y at t=0 */
    int32_t icon_trans_end_x; /* icon_clone translate_x at t=1 */
    int32_t icon_trans_end_y; /* icon_clone translate_y at t=1 */
    int32_t snap_trans_start_x; /* app_snapshot translate_x at t=0 */
    int32_t snap_trans_start_y; /* app_snapshot translate_y at t=0 */
    int32_t snap_trans_end_x; /* app_snapshot translate_x at t=1 */
    int32_t snap_trans_end_y; /* app_snapshot translate_y at t=1 */
    bool opening; /* true=open anim, false=close anim */
    lv_anim_t lv_anim;
    eos_anim_group_t *group;
    lv_obj_t *focus_icon;
} _paired_fade_ctx_t;

/* Lifecycle --------------------------------------------------*/

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
        if (ret == EOS_ERR_TIMEOUT)
        {
            error_type = EOS_SCRIPT_FAULT_UNRESPONSIVE;
        }
        else
        {
            // Check error info for timeout if code doesn't indicate it
            const char *error_info = script_engine_get_error_info();
            if (error_info && strstr(error_info, "timeout"))
            {
                error_type = EOS_SCRIPT_FAULT_UNRESPONSIVE;
            }
        }

        // Only handle error if it hasn't been handled already (timeout is handled inside script_engine_run)
        if (ret != EOS_ERR_TIMEOUT)
        {
            eos_script_error_handler_cfg_t app_cfg = {
                .confirm_btn_id = STR_ID_APP_RESTART,
                .confirm_cb = _app_list_restart_cb,
                .cancel_btn_id = STR_ID_APP_EXIT,
                .cancel_cb = _app_list_exit_cb,
            };
            eos_app_handle_script_error(error_type, ret, ctx->app_id, &app_cfg);
        }
        EOS_LOG_E("Application encounter a fatal error");
    }
}

static void _app_list_restart_cb(lv_event_t *e)
{
    (void)e;
    char app_id[64] = {0};

    /* Try crash context first (for engine crash recovery path) */
    const spm_crash_state_t *crash = spm_get_crash_state();
    if (crash && crash->has_crash && crash->script_id[0])
    {
        snprintf(app_id, sizeof(app_id), "%s", crash->script_id);
    }
    else
    {
        /* Fallback: get app_id from current activity (initial load error path) */
        eos_activity_t *current = eos_activity_get_current();
        if (current)
        {
            app_launch_ctx_t *ctx = eos_activity_get_user_data(current);
            if (ctx && ctx->app_id)
            {
                snprintf(app_id, sizeof(app_id), "%s", ctx->app_id);
            }
        }
    }

    spm_clear_crash_state();

    if (app_id[0])
    {
        EOS_LOG_I("Restarting app from error panel: %s", app_id);
        eos_activity_t *current = eos_activity_get_current();
        eos_app_restart_in_place(app_id, current);
    }
    else
    {
        EOS_LOG_W("Restart app failed: no app_id available");
        eos_activity_back_to_watchface();
    }
}

static void _app_list_exit_cb(lv_event_t *e)
{
    (void)e;
    EOS_LOG_I("Exiting app from error panel");
    spm_clear_crash_state();
    /* Try to return to the previous activity (app list) with animation.
     * Falls back to watchface if the stack is empty or transition fails. */
    if (eos_activity_back() != EOS_OK)
    {
        EOS_LOG_W("Activity back failed, falling back to watchface");
        eos_activity_back_to_watchface();
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
    snprintf(manifest_path, sizeof(manifest_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_MANIFEST_FILE_NAME, app_id);

    pkg->type = SCRIPT_TYPE_APPLICATION;
    if (script_engine_get_manifest(manifest_path, pkg) != EOS_OK)
    {
        EOS_LOG_E("Read manifest failed: %s", manifest_path);
        return EOS_FAILED;
    }

    char script_path[EOS_FS_PATH_MAX];
    snprintf(script_path, sizeof(script_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_SCRIPT_ENTRY_FILE_NAME, app_id);

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

eos_result_t eos_app_restart_in_place(const char *app_id, eos_activity_t *activity)
{
    LV_UNUSED(app_id);
    EOS_CHECK_PTR_RETURN_VAL(activity, EOS_FAILED);

    app_launch_ctx_t *ctx = (app_launch_ctx_t *)eos_activity_get_user_data(activity);
    if (!ctx || !ctx->app_id || !ctx->pkg.script_str)
    {
        EOS_LOG_E("Restart in-place failed: invalid launch context");
        return EOS_FAILED;
    }

    EOS_LOG_I("Restarting app in-place: %s", ctx->app_id);

    /* Clear fault panel reference BEFORE cleaning view.
     * lv_obj_clean will delete the panel's container widget, which triggers
     * _eos_fault_panel_container_delete_cb → eos_free(fault_panel).
     * We must clear the activity's pointer first to avoid a dangling reference. */
    eos_activity_set_fault_panel(activity, NULL);

    /* Clean all old widgets from the view (including fault panel UI).
     * After an engine crash, the view still holds orphaned LVGL objects
     * from the crashed script — their JS callbacks are inert (engine gen
     * change), but the widgets themselves need to be removed so the
     * restarted script gets a clean slate. */
    lv_obj_t *view = eos_activity_get_view(activity);
    if (view && lv_obj_is_valid(view))
    {
        lv_obj_clean(view);
    }

    /* Re-run the app script on the same activity */
    return spm_app_run(&ctx->pkg);
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

    snprintf(_app_list_last_launch_app_id, sizeof(_app_list_last_launch_app_id), "%s", app_id);
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

    /* Ensure fresh coords: lv_obj_get_coords reads cached obj->coords which
     * may be stale right after lv_obj_set_pos before next layout pass. */
    lv_obj_update_layout(obj);

    /* Pivot is in image's own coordinate system: global icon center minus
     * image's global top-left. Both must be in the same (global) coordinate
     * system for the pivot to place the zoom center at the icon position. */
    lv_area_t obj_area;
    lv_obj_get_coords(obj, &obj_area);

    *pivot_x = _app_list_last_icon_center_x - obj_area.x1;
    *pivot_y = _app_list_last_icon_center_y - obj_area.y1;
    return true;
}

static bool _app_list_calc_focus_pivot(lv_obj_t *snapshot_obj, lv_obj_t *icon_obj, int32_t *pivot_x, int32_t *pivot_y)
{
    if (!(snapshot_obj && pivot_x && pivot_y))
    {
        return false;
    }

    /* Ensure fresh coords before reading back cached global positions. */
    lv_obj_update_layout(snapshot_obj);
    if (icon_obj)
    {
        lv_obj_update_layout(icon_obj);
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

static void _app_list_cleanup_extra_cb(lv_event_t *e)
{
    lv_obj_t *extra = (lv_obj_t *)lv_event_get_user_data(e);
    if (extra && lv_obj_is_valid(extra))
    {
        lv_obj_delete(extra);
    }
}

/* Paired crossfade + center-aligned translate:
 * One lv_anim drives:
 *  1. Opacity remapped by scale: icon reaches full transparency at
 *     FOCUS_SCALE/2 (scale 1024). Opening: t=0→109 (43% eased progress),
 *     Closing: t=146→255 (last 43% eased progress). App opacity is always
 *     complementary (sum=255).
 *  2. icon_clone translate: lerp between pre-computed start/end
 *  3. app_snapshot translate: lerp between pre-computed start/end
 *
 * v goes from 255 to 0; progress t = 255-v, t: 0→1 eased.
 *
 * Scale goes 256↔2048 (range 1792). FOCUS_SCALE/2 = 1024.
 * Opening (256→2048): scale=1024 at f=768/1792=3/7, t=255*3/7≈109.
 * Closing (2048→256): scale=1024 at f=1024/1792=4/7, t=255*4/7≈146.
 */
static void _app_list_paired_fade_exec_cb(void *var, int32_t v)
{
    _paired_fade_ctx_t *ctx = (_paired_fade_ctx_t *)var;
    int32_t t = 255 - v; /* t: 0→255 eased (same curve as scale) */

    /* Icon opacity driven by scale: reaches 0/255 at FOCUS_SCALE/2.
     * Opening: icon fades out 255→0 over t=0..109, then stays 0.
     * Closing: icon stays 0 until t=146, then fades in 0→255. */
    int32_t icon_opa;
    if (ctx->opening)
    {
        /* scale 256→1024 in first 109/255 of eased progress: fast fade-out */
        if (t < 109)
            icon_opa = 255 - (t * 255 / 109);
        else
            icon_opa = 0;
    }
    else
    {
        /* scale 1024→256 in last 109/255 of eased progress: fast fade-in */
        if (t < 146)
            icon_opa = 0;
        else
            icon_opa = 255 * (t - 146) / 109;
    }
    int32_t app_opa = 255 - icon_opa;

    /* opening: obj_a=icon_clone(fading out), obj_b=app_snapshot(fading in)
     * closing: obj_a=app_snapshot(fading out), obj_b=icon_clone(fading in) */
    if (ctx->obj_a && lv_obj_is_valid(ctx->obj_a))
    {
        lv_obj_set_style_opa(ctx->obj_a, (lv_opa_t)(ctx->opening ? icon_opa : app_opa), 0);
    }
    if (ctx->obj_b && lv_obj_is_valid(ctx->obj_b))
    {
        lv_obj_set_style_opa(ctx->obj_b, (lv_opa_t)(ctx->opening ? app_opa : icon_opa), 0);
    }

    /* icon_clone translate: pure lerp */
    if (ctx->icon_clone && lv_obj_is_valid(ctx->icon_clone))
    {
        int32_t tx = ctx->icon_trans_start_x + ((ctx->icon_trans_end_x - ctx->icon_trans_start_x) * t + 127) / 255;
        int32_t ty = ctx->icon_trans_start_y + ((ctx->icon_trans_end_y - ctx->icon_trans_start_y) * t + 127) / 255;
        lv_obj_set_style_translate_x(ctx->icon_clone, tx, 0);
        lv_obj_set_style_translate_y(ctx->icon_clone, ty, 0);
    }

    /* app_snapshot translate: pure lerp */
    if (ctx->app_snapshot && lv_obj_is_valid(ctx->app_snapshot))
    {
        int32_t tx = ctx->snap_trans_start_x + ((ctx->snap_trans_end_x - ctx->snap_trans_start_x) * t + 127) / 255;
        int32_t ty = ctx->snap_trans_start_y + ((ctx->snap_trans_end_y - ctx->snap_trans_start_y) * t + 127) / 255;
        lv_obj_set_style_translate_x(ctx->app_snapshot, tx, 0);
        lv_obj_set_style_translate_y(ctx->app_snapshot, ty, 0);
    }
}

static void _free_paired_fade_ctx(lv_timer_t *t)
{
    _paired_fade_ctx_t *ctx = lv_timer_get_user_data(t);
    eos_free(ctx);
}

static void _app_list_paired_fade_ready_cb(lv_anim_t *a)
{
    _paired_fade_ctx_t *ctx = (_paired_fade_ctx_t *)lv_anim_get_user_data(a);

    /* Restore focus icon when closing animation finishes */
    if (ctx->focus_icon && lv_obj_is_valid(ctx->focus_icon))
    {
        lv_obj_remove_flag(ctx->focus_icon, LV_OBJ_FLAG_HIDDEN);
    }

    /* Notify group */
    if (ctx->group)
    {
        eos_anim_group_t *g = ctx->group;
        ctx->group = NULL;
        g->completed++;
        EOS_LOG_E("Paired fade group[%p]: completed=%d expected=%d", g, g->completed, g->expected);
        if (g->completed >= g->expected && g->callback)
        {
            g->callback(g->user_data);
        }
    }

    /* Defer free to avoid use-after-free in lv_anim internals */
    lv_timer_t *t = lv_timer_create(_free_paired_fade_ctx, 10, ctx);
    lv_timer_set_repeat_count(t, 1);
}

static void _app_list_start_paired_fade(lv_obj_t *obj_a,
                                        lv_obj_t *obj_b,
                                        lv_obj_t *icon_clone,
                                        lv_obj_t *app_snapshot,
                                        int32_t focus_translate_x,
                                        int32_t focus_translate_y,
                                        bool opening,
                                        uint32_t duration,
                                        eos_anim_group_t *group,
                                        lv_obj_t *focus_icon,
                                        uint32_t delay)
{
    if (!obj_a || !obj_b || !icon_clone || !app_snapshot || duration == 0)
    {
        return;
    }

    _paired_fade_ctx_t *ctx = eos_malloc_zeroed(sizeof(_paired_fade_ctx_t));
    if (!ctx)
    {
        return;
    }

    ctx->obj_a = obj_a;
    ctx->obj_b = obj_b;
    ctx->icon_clone = icon_clone;
    ctx->app_snapshot = app_snapshot;
    ctx->group = group;
    ctx->focus_icon = focus_icon;
    ctx->opening = opening;

    /* icon_clone: positioned at icon coords, pivot at own center.
     * Its visual center = icon_center + translate.
     * Translate from icon_center → display_center (opening) or reverse (closing).
     */
    ctx->icon_trans_start_x = opening ? 0 : focus_translate_x;
    ctx->icon_trans_start_y = opening ? 0 : focus_translate_y;
    ctx->icon_trans_end_x = opening ? focus_translate_x : 0;
    ctx->icon_trans_end_y = opening ? focus_translate_y : 0;

    /* app_snapshot: positioned at (0,0), pivot at own center (W/2, H/2).
     * Its visual center = display_center + translate.
     * To align with icon_clone center (= icon_center + icon_translate):
     *   snap_translate = icon_center + icon_translate - display_center
     *                   = icon_translate - focus_translate
     * Opening: icon_translate goes 0→focus_translate, so snap -focus_translate→0.
     * Closing: icon_translate goes focus_translate→0, so snap 0→-focus_translate.
     */
    ctx->snap_trans_start_x = opening ? -focus_translate_x : 0;
    ctx->snap_trans_start_y = opening ? -focus_translate_y : 0;
    ctx->snap_trans_end_x = opening ? 0 : -focus_translate_x;
    ctx->snap_trans_end_y = opening ? 0 : -focus_translate_y;

    /* Set initial values */
    lv_obj_set_style_opa(obj_a, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(obj_b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_translate_x(icon_clone, ctx->icon_trans_start_x, 0);
    lv_obj_set_style_translate_y(icon_clone, ctx->icon_trans_start_y, 0);
    lv_obj_set_style_translate_x(app_snapshot, ctx->snap_trans_start_x, 0);
    lv_obj_set_style_translate_y(app_snapshot, ctx->snap_trans_start_y, 0);

    lv_anim_init(&ctx->lv_anim);
    lv_anim_set_var(&ctx->lv_anim, ctx);
    lv_anim_set_values(&ctx->lv_anim, 255, 0);
    lv_anim_set_exec_cb(&ctx->lv_anim, _app_list_paired_fade_exec_cb);
    lv_anim_set_path_cb(&ctx->lv_anim, lv_anim_path_custom_bezier3);
    lv_anim_set_bezier3_param(&ctx->lv_anim,
                              _EASE_OUT_QUINT_BX1,
                              _EASE_OUT_QUINT_BY1,
                              _EASE_OUT_QUINT_BX2,
                              _EASE_OUT_QUINT_BY2);
    lv_anim_set_duration(&ctx->lv_anim, duration);
    lv_anim_set_delay(&ctx->lv_anim, delay);
    lv_anim_set_completed_cb(&ctx->lv_anim, _app_list_paired_fade_ready_cb);
    lv_anim_set_user_data(&ctx->lv_anim, ctx);

    if (group)
    {
        group->expected++;
        EOS_LOG_I("Paired fade attached to group[%p] (expected=%d)", group, group->expected);
    }

    lv_anim_start(&ctx->lv_anim);
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

    /* Ensure fresh coords before calculating clone position */
    lv_obj_update_layout(focus_icon);

    lv_area_t icon_coords;
    lv_obj_get_coords(focus_icon, &icon_coords);
    int32_t bw = lv_area_get_width(&icon_coords);
    int32_t bh = lv_area_get_height(&icon_coords);

    eos_activity_t *icon_activity = eos_activity_from_widget(focus_icon);
    lv_obj_t *icon_parent = icon_activity ? eos_activity_get_snap_container(icon_activity) : NULL;
    if (!icon_parent)
        icon_parent = eos_overlay_get_snapshot_layer();

    /* Parent may have been just created; refresh its layout so global coords are valid */
    lv_obj_update_layout(icon_parent);

    lv_obj_t *icon_clone = lv_obj_create(icon_parent);
    lv_obj_set_size(icon_clone, bw, bh);
    /* icon_coords are global coords from lv_obj_get_coords; convert to parent-local */
    lv_area_t parent_coords;
    lv_obj_get_coords(icon_parent, &parent_coords);
    lv_obj_set_pos(icon_clone, icon_coords.x1 - parent_coords.x1, icon_coords.y1 - parent_coords.y1);
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

static void _app_list_play_transition_anim(eos_anim_group_t *group,
                                           eos_activity_t *from,
                                           eos_activity_t *to,
                                           bool opening)
{
    if (!(group && from && to))
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
            EOS_LOG_E("CLOSE ANIM: app_snapshot FAILED from %p", from);
            return;
        }
        EOS_LOG_E("CLOSE ANIM: app_snapshot CREATED from %p", from);
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
            _app_list_record_icon_center_point(area.x1 + lv_area_get_width(&area) / 2,
                                               area.y1 + lv_area_get_height(&area) / 2);
        }

        icon_clone = _app_list_create_icon_clone(focus_icon);
        /* focus_icon will be restored by paired fade ready_cb */

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

    if (focus_icon && !focus_icon_hidden_flag && opening)
    {
        lv_obj_remove_flag(focus_icon, LV_OBJ_FLAG_HIDDEN);
    }

    if (opening)
    {
        app_snapshot = eos_activity_take_snapshot(to, include_header_in_snapshot);
        if (!app_snapshot)
        {
            EOS_LOG_E("OPEN ANIM: app_snapshot FAILED to %p", to);
            if (icon_clone)
            {
                lv_obj_delete(icon_clone);
                icon_clone = NULL;
            }
            return;
        }
        EOS_LOG_E("OPEN ANIM: app_snapshot CREATED to %p", to);
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

    /* Set app_snapshot pivot to its own center so visual center = pos + translate,
     * independent of scale. This keeps alignment with icon_clone simple and correct
     * — both centers follow the same translate-driven path. */
    app_pivot_x = lv_obj_get_width(app_snapshot) / 2;
    app_pivot_y = lv_obj_get_height(app_snapshot) / 2;
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

    int32_t focus_translate_x = 0;
    int32_t focus_translate_y = 0;
    if (_app_list_last_icon_center_valid)
    {
        int32_t view_center_x = EOS_DISPLAY_WIDTH / 2;
        int32_t view_center_y = EOS_DISPLAY_HEIGHT / 2;
        focus_translate_x = view_center_x - _app_list_last_icon_center_x;
        focus_translate_y = view_center_y - _app_list_last_icon_center_y;
    }
    EOS_LOG_E("FOCUS: valid=%d last_x=%d last_y=%d view_center=(%d,%d) translate=(%d,%d)",
              _app_list_last_icon_center_valid,
              _app_list_last_icon_center_x,
              _app_list_last_icon_center_y,
              EOS_DISPLAY_WIDTH / 2,
              EOS_DISPLAY_HEIGHT / 2,
              focus_translate_x,
              focus_translate_y);

    if (opening)
    {
        /* from side: list_snapshot zoom out + move -------------------*/
        if (list_snapshot)
        {
            lv_image_set_scale(list_snapshot, 256);
            lv_obj_set_style_translate_x(list_snapshot, 0, 0);
            lv_obj_set_style_translate_y(list_snapshot, 0, 0);

            eos_anim_t *anim =
                eos_anim_image_scale_create(list_snapshot, 256, _APP_LIST_ANIM_FOCUS_SCALE, total_duration, false);
            if (anim)
            {
                eos_anim_set_path_bezier3(anim,
                                          _EASE_OUT_QUINT_BX1,
                                          _EASE_OUT_QUINT_BY1,
                                          _EASE_OUT_QUINT_BX2,
                                          _EASE_OUT_QUINT_BY2);
                eos_anim_group_attach(anim, group);
                eos_anim_start(anim);
            }

            anim =
                eos_anim_move_create(list_snapshot, 0, 0, focus_translate_x, focus_translate_y, total_duration, false);
            if (anim)
            {
                eos_anim_set_path_bezier3(anim,
                                          _EASE_OUT_QUINT_BX1,
                                          _EASE_OUT_QUINT_BY1,
                                          _EASE_OUT_QUINT_BX2,
                                          _EASE_OUT_QUINT_BY2);
                eos_anim_group_attach(anim, group);
                eos_anim_start(anim);
            }
        }

        /* icon_clone: scale up (fade + translate handled by paired fade, all in sync) -*/
        if (icon_clone)
        {
            lv_obj_set_style_transform_scale(icon_clone, 256, 0);

            eos_anim_t *anim =
                eos_anim_transform_scale_create(icon_clone, 256, _APP_LIST_ANIM_FOCUS_SCALE, total_duration, false);
            if (anim)
            {
                eos_anim_set_path_bezier3(anim,
                                          _EASE_OUT_QUINT_BX1,
                                          _EASE_OUT_QUINT_BY1,
                                          _EASE_OUT_QUINT_BX2,
                                          _EASE_OUT_QUINT_BY2);
                eos_anim_group_attach(anim, group);
                eos_anim_start(anim);
            }
        }

        /* to side: app_snapshot zoom in (scale + translate run in sync) -*/
        lv_image_set_scale(app_snapshot, _APP_LIST_ANIM_MIN_SACLE);

        eos_anim_t *anim =
            eos_anim_image_scale_create(app_snapshot, _APP_LIST_ANIM_MIN_SACLE, 256, total_duration, false);
        if (anim)
        {
            eos_anim_set_path_bezier3(anim,
                                      _EASE_OUT_QUINT_BX1,
                                      _EASE_OUT_QUINT_BY1,
                                      _EASE_OUT_QUINT_BX2,
                                      _EASE_OUT_QUINT_BY2);
            eos_anim_group_attach(anim, group);
            eos_anim_start(anim);
        }
    }
    else
    {
        /* from side: app_snapshot zoom out (scale + translate run in sync) -*/
        lv_image_set_scale(app_snapshot, 256);

        eos_anim_t *anim =
            eos_anim_image_scale_create(app_snapshot, 256, _APP_LIST_ANIM_MIN_SACLE, total_duration, false);
        if (anim)
        {
            eos_anim_set_path_bezier3(anim,
                                      _EASE_OUT_QUINT_BX1,
                                      _EASE_OUT_QUINT_BY1,
                                      _EASE_OUT_QUINT_BX2,
                                      _EASE_OUT_QUINT_BY2);
            eos_anim_group_attach(anim, group);
            eos_anim_start(anim);
        }

        /* to side: list_snapshot zoom in + move ----------------------*/
        if (list_snapshot)
        {
            lv_image_set_scale(list_snapshot, _APP_LIST_ANIM_FOCUS_SCALE);
            lv_obj_set_style_translate_x(list_snapshot, focus_translate_x, 0);
            lv_obj_set_style_translate_y(list_snapshot, focus_translate_y, 0);

            eos_anim_t *anim =
                eos_anim_image_scale_create(list_snapshot, _APP_LIST_ANIM_FOCUS_SCALE, 256, total_duration, false);
            if (anim)
            {
                eos_anim_set_path_bezier3(anim,
                                          _EASE_OUT_QUINT_BX1,
                                          _EASE_OUT_QUINT_BY1,
                                          _EASE_OUT_QUINT_BX2,
                                          _EASE_OUT_QUINT_BY2);
                eos_anim_group_attach(anim, group);
                eos_anim_start(anim);
            }

            anim =
                eos_anim_move_create(list_snapshot, focus_translate_x, focus_translate_y, 0, 0, total_duration, false);
            if (anim)
            {
                eos_anim_set_path_bezier3(anim,
                                          _EASE_OUT_QUINT_BX1,
                                          _EASE_OUT_QUINT_BY1,
                                          _EASE_OUT_QUINT_BX2,
                                          _EASE_OUT_QUINT_BY2);
                eos_anim_group_attach(anim, group);
                eos_anim_start(anim);
            }
        }

        /* icon_clone: scale down (fade + translate handled by paired fade, all in sync) -*/
        if (icon_clone)
        {
            lv_obj_set_style_transform_scale(icon_clone, _APP_LIST_ANIM_FOCUS_SCALE, 0);

            eos_anim_t *anim =
                eos_anim_transform_scale_create(icon_clone, _APP_LIST_ANIM_FOCUS_SCALE, 256, total_duration, false);
            if (anim)
            {
                eos_anim_set_path_bezier3(anim,
                                          _EASE_OUT_QUINT_BX1,
                                          _EASE_OUT_QUINT_BY1,
                                          _EASE_OUT_QUINT_BX2,
                                          _EASE_OUT_QUINT_BY2);
                eos_anim_group_attach(anim, group);
                eos_anim_start(anim);
            }
        }
    }

    /* --- Paired crossfade + center-aligned translate:
     * One master lv_anim drives opacity (complementary) + translate for both
     * icon_clone and app_snapshot, keeping their visual centers aligned.
     *
     * opening: icon center → screen center; icon_clone fades out, app_snapshot fades in
     * closing: screen center → icon center; app_snapshot fades out, icon_clone fades in
     */
    if (icon_clone && app_snapshot)
    {
        lv_obj_t *fade_obj_a = opening ? icon_clone : app_snapshot;
        lv_obj_t *fade_obj_b = opening ? app_snapshot : icon_clone;
        lv_obj_t *paired_focus_icon = opening ? NULL : focus_icon;

        _app_list_start_paired_fade(fade_obj_a,
                                    fade_obj_b,
                                    icon_clone,
                                    app_snapshot,
                                    focus_translate_x,
                                    focus_translate_y,
                                    opening,
                                    total_duration,
                                    group,
                                    paired_focus_icon,
                                    0);

        /* Cleanup: when app_snapshot is deleted, also delete icon_clone */
        lv_obj_add_event_cb(app_snapshot, _app_list_cleanup_extra_cb, LV_EVENT_DELETE, icon_clone);
    }

    EOS_LOG_E("Transition ANIMS: group[%p] expected=%d opening=%d", group, group->expected, opening);
}

static void _app_list_on_resueme(eos_activity_t *a)
{
    // Initialize app list
    lv_obj_t *bubble_grid = _app_list_get_bubble_grid(a);
    EOS_CHECK_PTR_RETURN(bubble_grid);
    _app_list_refresh(bubble_grid);
}

/* App Entry --------------------------------------------------*/
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
        _app_list_record_icon_center_point(area.x1 + lv_area_get_width(&area) / 2,
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

/* Refresh App List -------------------------------------------*/
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
                snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME, app_id);
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
            snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME, app_id);
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

/* Animation --------------------------------------------------*/

static void _app_list_open_app_anim_cb(eos_anim_group_t *group, eos_activity_t *from, eos_activity_t *to)
{
    _app_list_play_transition_anim(group, from, to, true);
}

static void _app_list_close_app_anim_cb(eos_anim_group_t *group, eos_activity_t *from, eos_activity_t *to)
{
    _app_list_play_transition_anim(group, from, to, false);
}

/* Helper Functions -------------------------------------------*/

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
