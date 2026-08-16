/**
 * @file eos_app_list.c
 * @brief App list page - using bubble_grid layout
 */

#include "eos_config.h"
#include "eos_app_list.h"

/* Includes ---------------------------------------------------*/
#include <stdbool.h>
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
#include "eos_loading_spinner.h"
#include "eos_overlay_layer.h"
#include "eos_recent_apps.h"
#ifdef EOS_ENABLE_TEST_APP
#include "eos_test.h"
#endif
/* Macros and Definitions -------------------------------------*/
#define _APP_LIST_ANIM_DURATION 350
#define _APP_LIST_ANIM_FOCUS_SCALE 2048
#define _APP_LIST_ANIM_MIN_SACLE 64

/* Loading screen: poll interval and minimum display time.
 * MIN_MS must be >= ANIM_DURATION so fast apps don't flash. */
#define _APP_LIST_LOADING_TICK_MS 20
#define _APP_LIST_LOADING_MIN_MS 3000

/* Phased loading spinner */
#define _APP_LIST_LOADING_SPINNER_SIZE 60
#define _APP_LIST_LOADING_PHASE_WAIT 0
#define _APP_LIST_LOADING_PHASE_READ 1
#define _APP_LIST_LOADING_PHASE_ENGINE 2

/* Custom ease-out curve: P1=(0.22,1) keeps the snap/elastic feel,
 * P2=(0.43,1) avoids the quintic tail-flatness that causes a
 * perceptible one-frame stall at max simulator zoom. */
#define _EASE_OUT_SNAP_BX1 225
#define _EASE_OUT_SNAP_BY1 1024
#define _EASE_OUT_SNAP_BX2 440
#define _EASE_OUT_SNAP_BY2 1024
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
static void _app_on_pause(eos_activity_t *a);
static void _app_on_resume(eos_activity_t *a);
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
    .on_pause = _app_on_pause,
    .on_resume = _app_on_resume,
};

#define _APP_LAUNCH_CTX_MAGIC 0xE05A7070 /* "EOS App" */

typedef struct
{
    uint32_t magic; /**< Magic sentinel: _APP_LAUNCH_CTX_MAGIC */
    script_pkg_t pkg;
    char *app_id;
    lv_timer_t *loading_timer; /* Phase B polling / chunk-read timer */
    lv_obj_t *loading_widget; /* Loading placeholder root container */
    eos_loading_spinner_t *loading_spinner; /**< Phased radial loading spinner */
    lv_obj_t *status_label; /* Status text ("Reading app...", etc.) */
    uint32_t loading_start_ms; /* Tick when loading began (eos_tick_get) */
    bool launch_running; /* Guard against re-entrant JS launch */
    bool is_resuming; /* True when resuming from recents (skip loading UI) */
    /* Chunked I/O state */
    eos_file_t script_file; /* Open file handle during chunked read */
    char *script_buf; /* Accumulated file buffer */
    uint32_t script_size; /* Total file size in bytes */
    uint32_t script_read; /* Bytes read so far */
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

/* Forward-declare the unified-animation context so the start-function
 * prototype can use the typedef rather than the raw struct tag. */
typedef struct _transition_anim_ctx_t _transition_anim_ctx_t;

static void _transition_anim_exec_cb(void *var, int32_t v);
static void _transition_anim_ready_cb(lv_anim_t *a);
static void _transition_anim_start(_transition_anim_ctx_t *ctx, uint32_t duration, uint32_t delay);
static int32_t _app_list_find_sys_app(const char *app_id);
static eos_result_t _app_list_build_manifest(const char *app_id, script_pkg_t *pkg);
static eos_result_t _app_list_launch_script_app(const char *app_id);
static void _app_list_restart_cb(lv_event_t *e);
static void _app_list_exit_cb(lv_event_t *e);
static void _app_list_show_loading(eos_activity_t *a, app_launch_ctx_t *ctx);
static void _app_list_loading_tick(lv_timer_t *t);
static void _app_list_start_chunked_read(app_launch_ctx_t *ctx);
static void _app_list_read_chunk_tick(lv_timer_t *t);
static void _app_list_do_launch_script(app_launch_ctx_t *ctx);
static void _app_handle_script_run_result(eos_activity_t *a, app_launch_ctx_t *ctx, eos_result_t ret);
static void _app_list_update_loading_status(app_launch_ctx_t *ctx, const char *msg);

static bool _anim_routes_registered = false;
static bool _app_list_last_icon_center_valid = false;
static int32_t _app_list_last_icon_center_x = 0;
static int32_t _app_list_last_icon_center_y = 0;
static int32_t _app_list_last_click_index = -1;
static char _app_list_last_launch_app_id[64] = {0};
static uint32_t _app_list_icon_count = 0;

/* Unified transition-animation context.
 * ONE lv_anim drives ALL properties (scale + translate + opacity) for every
 * layer in lockstep.  This eliminates the per-frame rounding drift that occurs
 * when independent lv_anim instances with different value ranges run against
 * the same easing curve — small-range anims (e.g. app_snapshot 64→256,
 * range=192) would quantise to zero delta at the tail, causing a visible
 * one-frame stall.
 *
 * t = 255 - v  goes 0→255 (eased).  All lerps use (val * t + 127) / 255. */
struct _transition_anim_ctx_t
{
    /* layers -----------------------------------------------------*/
    lv_obj_t *list_snapshot; /* app-list screenshot (may be NULL)   */
    lv_obj_t *icon_clone; /* cloned icon                         */
    lv_obj_t *app_snapshot; /* target-activity screenshot          */
    lv_obj_t *obj_a; /* fades out (255→0)                   */
    lv_obj_t *obj_b; /* fades in  (0→255)                   */

    /* list_snapshot ----------------------------------------------*/
    int32_t list_scale_start, list_scale_end; /* lv_image_set_scale   */
    int32_t list_tx_start, list_tx_end; /* translate_x          */
    int32_t list_ty_start, list_ty_end; /* translate_y          */

    /* icon_clone -------------------------------------------------*/
    int32_t icon_scale_start, icon_scale_end; /* transform_scale      */
    int32_t icon_tx_start, icon_tx_end; /* translate_x          */
    int32_t icon_ty_start, icon_ty_end; /* translate_y          */

    /* app_snapshot -----------------------------------------------*/
    int32_t app_scale_start, app_scale_end; /* lv_image_set_scale   */
    int32_t app_tx_start, app_tx_end; /* translate_x          */
    int32_t app_ty_start, app_ty_end; /* translate_y          */

    bool opening;
    lv_anim_t lv_anim;
    eos_anim_group_t *group;
    lv_obj_t *focus_icon; /* restored when closing anim completes  */
};

/* Lifecycle --------------------------------------------------*/

static void _app_on_pause(eos_activity_t *a)
{
    app_launch_ctx_t *ctx = eos_activity_get_user_data(a);
    if (!ctx || !ctx->app_id)
        return;

    /* Only AppRoot handles SPM suspend.
     * Sub-activities share the realm; their on_pause fires via the
     * lifecycle callback if set from JS. */
    if (eos_activity_get_type(a) == EOS_ACTIVITY_TYPE_APP)
    {
        /* Suspend the SPM program (pauses sni_ctx, preserves realm) */
        eos_result_t ret = spm_app_suspend();
        if (ret != EOS_OK)
        {
            EOS_LOG_W("spm_app_suspend failed for '%s': %d", ctx->app_id, ret);
        }
    }
}

static void _app_on_resume(eos_activity_t *a)
{
    app_launch_ctx_t *ctx = eos_activity_get_user_data(a);
    if (!ctx || !ctx->app_id)
        return;

    /* Only AppRoot handles SPM resume and resource strategies.
     * Sub-activities share the realm; their on_resume fires via the
     * lifecycle callback if set from JS. */
    if (eos_activity_get_type(a) == EOS_ACTIVITY_TYPE_APP)
    {
        eos_result_t ret = spm_app_resume(ctx->app_id);
        if (ret != EOS_OK)
        {
            EOS_LOG_W("spm_app_resume failed for '%s': %d", ctx->app_id, ret);
        }

#if EOS_RECENT_APPS_ENABLE
        /* Timer/animation strategies are applied by eos_recent_apps_resume()
         * via sni_context_resume_resources() on the program's sni_ctx.
         * We also apply them here for the case where resume happens
         * without going through the recents flow. */
        script_program_t *prog = spm_get_program_by_id_any_state(ctx->app_id);
        if (prog && prog->sni_ctx && prog->state != SCRIPT_PROGRAM_STATE_TERMINATED
            && prog->state != SCRIPT_PROGRAM_STATE_STOPPING)
        {
#if defined(EOS_RECENT_APPS_TIMER_STRATEGY) && defined(EOS_RECENT_APPS_ANIM_STRATEGY)
            uint32_t timer_strat = (uint32_t)EOS_RECENT_APPS_TIMER_STRATEGY;
            uint32_t anim_strat = (uint32_t)EOS_RECENT_APPS_ANIM_STRATEGY;
            sni_context_resume_resources(prog->sni_ctx, (int)timer_strat, (int)anim_strat);
#endif
        }
#endif /* EOS_RECENT_APPS_ENABLE */
    }
}

static void _app_on_destroy(eos_activity_t *a)
{
    app_launch_ctx_t *ctx = eos_activity_get_user_data(a);
    /* Stop the app script by ID (safe when multiple app programs exist) */
    if (ctx && ctx->app_id)
    {
        spm_app_stop_by_id(ctx->app_id);
    }
    else
    {
        /* Fallback: stop any running app program */
        spm_app_stop();
    }

    if (ctx)
    {
        /* Cancel the Phase B polling / chunk-read timer if still pending */
        if (ctx->loading_timer)
        {
            lv_timer_del(ctx->loading_timer);
            ctx->loading_timer = NULL;
        }

        /* Close the chunked-I/O file handle if still open */
        if (ctx->script_file != EOS_FILE_INVALID)
        {
            eos_fs_close(ctx->script_file);
            ctx->script_file = EOS_FILE_INVALID;
        }

        /* Free the chunked-I/O buffer (may not have been assigned to pkg yet) */
        if (ctx->script_buf)
        {
            eos_free(ctx->script_buf);
            ctx->script_buf = NULL;
        }

        /* Clean up loading placeholder if it wasn't already removed */
        if (ctx->loading_widget && lv_obj_is_valid(ctx->loading_widget))
        {
            lv_obj_del(ctx->loading_widget);
            ctx->loading_widget = NULL;
            ctx->loading_spinner = NULL; /* freed via spinner root's LV_EVENT_DELETE cascade */
        }

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

    /* When resuming from recents, skip the loading UI entirely.
     * The transition animation uses the stored screenshot instead. */
    if (ctx->is_resuming)
    {
        EOS_LOG_I("App resumed from recents — skipping loading UI");
        ctx->is_resuming = false;
        return;
    }

    /* Phase A: show loading placeholder and start polling timer.
     * The zoom animation (APP_LIST → APP) plays immediately after
     * this returns, capturing the loading UI in its snapshot.
     * JS execution is deferred to Phase B after the animation completes. */
    _app_list_show_loading(a, ctx);
}

/* Phase B: deferred script loading ---------------------------*/

#define _APP_LIST_CHUNK_SIZE 4096 /* Bytes per tick during chunked I/O */

#if EOS_COMPILE_MODE == DEBUG
/* Debug: artificial delays to simulate slow Flash / slow JS parsing.
 * Set via eos_app_list_set_debug_loading_delay(). Zero = no delay. */
static uint32_t _debug_io_delay_ms = 0;
static uint32_t _debug_eval_delay_ms = 0;

void eos_app_list_set_debug_loading_delay(uint32_t io_delay_ms, uint32_t eval_delay_ms)
{
    _debug_io_delay_ms = io_delay_ms;
    _debug_eval_delay_ms = eval_delay_ms;
    EOS_LOG_I("Debug loading delay set: io=%" PRIu32 "ms eval=%" PRIu32 "ms", io_delay_ms, eval_delay_ms);
}

void eos_app_list_get_debug_loading_delay(uint32_t *io_delay_ms, uint32_t *eval_delay_ms)
{
    if (io_delay_ms)
        *io_delay_ms = _debug_io_delay_ms;
    if (eval_delay_ms)
        *eval_delay_ms = _debug_eval_delay_ms;
}
#endif /* EOS_COMPILE_MODE == DEBUG */

/**
 * @brief Update the status label text on the loading screen.
 */
static void _app_list_update_loading_status(app_launch_ctx_t *ctx, const char *msg)
{
    if (ctx->status_label && lv_obj_is_valid(ctx->status_label))
    {
        lv_label_set_text(ctx->status_label, msg);
    }
}

/**
 * @brief Build the loading UI with a determinate progress bar and status text.
 *
 * Layout (centered column):
 *   [App Name]
 *   [████████░░░░░░░░]  ← progress bar
 *   "Reading app..."    ← status label
 *
 * The zoom animation (APP_LIST → APP) snapshots this UI so the
 * user sees the progress bar during the icon-zoom transition.
 */
static void _app_list_show_loading(eos_activity_t *a, app_launch_ctx_t *ctx)
{
    /* Hide the header during loading; restored when loading completes */
    eos_activity_set_app_header_visible(a, false);

    lv_obj_t *view = eos_activity_get_view(a);
    if (!view)
    {
        return;
    }

    /* Full-screen loading container — pure black opaque overlay.
     * Placed on the activity view so the transition animation can
     * capture it as the app-snapshot. Spinner and icon are centered. */
    lv_obj_t *loading = lv_obj_create(view);
    lv_obj_remove_style_all(loading);
    lv_obj_set_size(loading, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(loading, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(loading, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(loading, 0, 0);
    lv_obj_clear_flag(loading, LV_OBJ_FLAG_SCROLLABLE);

    /* Resolve app icon path — same pattern as _app_list_refresh */
    char icon_path[EOS_FS_PATH_MAX];
    snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME, ctx->app_id);
    const void *icon_src = eos_storage_is_file(icon_path) ? (const void *)icon_path : (const void *)EOS_IMG_APP;

    /* App icon — circular, centered on screen */
    lv_obj_t *icon = eos_circle_image_create(loading, icon_src, 64);
    lv_obj_center(icon);

    /* Phased loading spinner — 24 bars, 3 phases, centered on screen.
     * Sized slightly larger than the icon so bars render around it.
     * With 80/120 radii: inner_r = 100*80/100=80, outer_r=100*120/100=120,
     * making a ~240px visual ring around the 64px icon. */
    eos_loading_spinner_t *spinner = eos_loading_spinner_create(loading, 16, 3);
    if (spinner)
    {
        lv_obj_set_size(spinner->root, _APP_LIST_LOADING_SPINNER_SIZE, _APP_LIST_LOADING_SPINNER_SIZE);
        lv_obj_center(spinner->root);
        eos_loading_spinner_set_radii(spinner, 64, 72);
        eos_loading_spinner_set_bar_width(spinner, 6);
        ctx->loading_spinner = spinner;
        eos_loading_spinner_set_infinite(spinner, true);
        /* Hidden until transition completes — snapshot shows icon only */
        lv_obj_add_flag(spinner->root, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        EOS_LOG_E("Failed to create loading spinner");
    }

    /* No text labels — the icon and spinner convey all loading state */

    ctx->loading_widget = loading;
    ctx->loading_start_ms = eos_tick_get();
    ctx->launch_running = false;
    ctx->loading_timer = lv_timer_create(_app_list_loading_tick, _APP_LIST_LOADING_TICK_MS, ctx);
}

/**
 * @brief Polling timer — waits for transition + min display time,
 *        then switches to chunked I/O reading.
 */
static void _app_list_loading_tick(lv_timer_t *t)
{
    app_launch_ctx_t *ctx = (app_launch_ctx_t *)lv_timer_get_user_data(t);
    if (!ctx || ctx->launch_running)
    {
        return;
    }

    /* Wait for the zoom animation to complete */
    if (eos_activity_is_transition_in_progress())
    {
        return;
    }

    /* Transition done — reveal the spinner now.
     * During the transition the snapshot showed icon-only on black. */
    if (ctx->loading_spinner && ctx->loading_spinner->root)
    {
        lv_obj_remove_flag(ctx->loading_spinner->root, LV_OBJ_FLAG_HIDDEN);
    }

    /* Enforce minimum display time so fast apps don't flash.
     * Drive phase 0 (wait) progress by elapsed time: 0-100 → 0-33% visual. */
    uint32_t elapsed = eos_tick_get() - ctx->loading_start_ms;
    if (elapsed < _APP_LIST_LOADING_MIN_MS)
    {
        uint16_t wait_pct = (uint16_t)((uint64_t)elapsed * 100 / _APP_LIST_LOADING_MIN_MS);
        if (ctx->loading_spinner)
        {
            eos_loading_spinner_set_phase_progress(ctx->loading_spinner, _APP_LIST_LOADING_PHASE_WAIT, wait_pct);
        }
        return;
    }

    /* Both conditions met — stop polling, start chunked I/O */
    ctx->launch_running = true;
    lv_timer_del(t);
    ctx->loading_timer = NULL;

    /* Mark phase 0 complete (visual 33%) */
    if (ctx->loading_spinner)
    {
        eos_loading_spinner_set_phase_progress(ctx->loading_spinner, _APP_LIST_LOADING_PHASE_WAIT, 100);
    }

    /* Set base_path now (fast, no I/O) so the package is valid for spm_app_run */
    char base_path[EOS_FS_PATH_MAX];
    snprintf(base_path, sizeof(base_path), EOS_APP_INSTALLED_DIR "%s/", ctx->app_id);
    ctx->pkg.base_path = eos_strdup(base_path);
    if (!ctx->pkg.base_path)
    {
        EOS_LOG_E("Failed to allocate base_path for %s", ctx->app_id);
        eos_activity_back();
        return;
    }

    _app_list_start_chunked_read(ctx);
}

/**
 * @brief Open the script file and begin chunked reading.
 *
 * Opens main.js via the VFS, gets the file size, allocates a buffer,
 * updates the progress bar range, and starts the per-tick read timer.
 */
static void _app_list_start_chunked_read(app_launch_ctx_t *ctx)
{
    char script_path[EOS_FS_PATH_MAX];
    snprintf(script_path, sizeof(script_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_SCRIPT_ENTRY_FILE_NAME, ctx->app_id);

    ctx->script_file = eos_fs_open_read(script_path);
    if (ctx->script_file == EOS_FILE_INVALID)
    {
        EOS_LOG_E("Failed to open script: %s", script_path);
        eos_activity_back();
        return;
    }

    uint32_t file_size = 0;
    if (eos_fs_size(ctx->script_file, &file_size) != EOS_OK || file_size == 0)
    {
        EOS_LOG_E("Invalid script size for %s", script_path);
        eos_fs_close(ctx->script_file);
        ctx->script_file = EOS_FILE_INVALID;
        eos_activity_back();
        return;
    }

    ctx->script_buf = eos_malloc(file_size + 1);
    if (!ctx->script_buf)
    {
        EOS_LOG_E("Failed to allocate script buffer (%" PRIu32 " bytes)", file_size);
        eos_fs_close(ctx->script_file);
        ctx->script_file = EOS_FILE_INVALID;
        eos_activity_back();
        return;
    }

    ctx->script_size = file_size;
    ctx->script_read = 0;

    /* Phase 1 (chunked read) drives 33-66%: start at 0 */
    if (ctx->loading_spinner)
    {
        eos_loading_spinner_set_phase_progress(ctx->loading_spinner, _APP_LIST_LOADING_PHASE_READ, 0);
    }

    _app_list_update_loading_status(ctx, "Reading app...");

    /* Replace the polling timer with the chunk-read timer */
    ctx->loading_timer = lv_timer_create(_app_list_read_chunk_tick, _APP_LIST_LOADING_TICK_MS, ctx);
}

/**
 * @brief Per-tick chunked read callback.
 *
 * Each invocation reads up to _APP_LIST_CHUNK_SIZE bytes from the
 * open file, advances the progress bar, and updates the status.
 * When all bytes are read the file is closed and JS evaluation begins.
 */
static void _app_list_read_chunk_tick(lv_timer_t *t)
{
    app_launch_ctx_t *ctx = (app_launch_ctx_t *)lv_timer_get_user_data(t);
    if (!ctx)
    {
        return;
    }

#if EOS_COMPILE_MODE == DEBUG
    /* Debug: inject artificial per-chunk delay to simulate slow Flash */
    if (_debug_io_delay_ms > 0)
    {
        uint32_t chunk_delay = _debug_io_delay_ms / ((ctx->script_size / _APP_LIST_CHUNK_SIZE) + 1);
        if (chunk_delay > 0)
        {
            eos_delay(chunk_delay);
        }
    }
#endif

    uint32_t remaining = ctx->script_size - ctx->script_read;
    uint32_t chunk = remaining < _APP_LIST_CHUNK_SIZE ? remaining : _APP_LIST_CHUNK_SIZE;

    int nread = eos_fs_read(ctx->script_file, ctx->script_buf + ctx->script_read, chunk);
    if (nread < 0)
    {
        EOS_LOG_E("Read error at offset %" PRIu32 " for %s", ctx->script_read, ctx->app_id);
        eos_fs_close(ctx->script_file);
        ctx->script_file = EOS_FILE_INVALID;
        eos_free(ctx->script_buf);
        ctx->script_buf = NULL;
        eos_activity_back();
        return;
    }
    if (nread == 0)
    {
        /* EOF reached — shouldn't happen if size was correct, but handle gracefully */
        nread = (int)remaining;
    }

    ctx->script_read += (uint32_t)nread;

    /* Phase 1 (chunked read) — map bytes_read to 0-100% */
    uint16_t read_pct = (ctx->script_size > 0) ? (uint16_t)(((uint64_t)ctx->script_read * 100) / ctx->script_size) : 0;
    if (ctx->loading_spinner)
    {
        eos_loading_spinner_set_phase_progress(ctx->loading_spinner, _APP_LIST_LOADING_PHASE_READ, read_pct);
    }

    if (ctx->script_read >= ctx->script_size)
    {
        /* All bytes read — finalize and proceed to JS eval */
        eos_fs_close(ctx->script_file);
        ctx->script_file = EOS_FILE_INVALID;
        ctx->script_buf[ctx->script_size] = '\0';
        ctx->pkg.script_str = ctx->script_buf;
        ctx->script_buf = NULL; /* Ownership transferred to pkg */

        lv_timer_del(t);
        ctx->loading_timer = NULL;

        /* Mark phase 1 complete (visual 66%) */
        if (ctx->loading_spinner)
        {
            eos_loading_spinner_set_phase_progress(ctx->loading_spinner, _APP_LIST_LOADING_PHASE_READ, 100);
        }

        _app_list_update_loading_status(ctx, "Starting engine...");
        _app_list_do_launch_script(ctx);
    }
}

/**
 * @brief Opacity animation exec callback — sets widget opacity.
 */
static void _loading_dim_exec_cb(void *var, int32_t v)
{
    lv_obj_t *widget = (lv_obj_t *)var;
    if (lv_obj_is_valid(widget))
    {
        lv_obj_set_style_opa(widget, (lv_opa_t)v, 0);
    }
}

/**
 * @brief Spinner dim-complete callback — removes the loading overlay
 *        and reveals the app underneath.
 */
static void _loading_dim_done_cb(void *user_data)
{
    app_launch_ctx_t *ctx = (app_launch_ctx_t *)user_data;
    if (ctx && ctx->loading_widget && lv_obj_is_valid(ctx->loading_widget))
    {
        lv_obj_del(ctx->loading_widget);
        ctx->loading_widget = NULL;
        ctx->loading_spinner = NULL; /* freed via LV_EVENT_DELETE cascade */
    }
}

/**
 * @brief Final phase: run the JS engine and reveal the app.
 *
 * After JS eval completes, the spinner is marked complete and a dim
 * animation runs on the loading overlay.  When the animation finishes
 * the callback removes the overlay, revealing the app with no transition.
 * This call blocks the main thread (JerryScript is synchronous) but the
 * user sees the spinning dots during the blocking period.
 */
static void _app_list_do_launch_script(app_launch_ctx_t *ctx)
{
    /* Verify the activity still exists (user may have navigated back) */
    eos_activity_t *a = eos_activity_get_current();
    if (!a || eos_activity_get_type(a) != EOS_ACTIVITY_TYPE_APP)
    {
        EOS_LOG_E("App activity gone during launch");
        eos_activity_back();
        return;
    }

#if EOS_COMPILE_MODE == DEBUG
    /* Debug: simulate slow JS parse/eval */
    if (_debug_eval_delay_ms > 0)
    {
        EOS_LOG_I("Debug: simulating slow JS eval delay %" PRIu32 "ms", _debug_eval_delay_ms);
        uint32_t elapsed = 0;
        while (elapsed < _debug_eval_delay_ms)
        {
            uint32_t step = 16;
            if (elapsed + step > _debug_eval_delay_ms)
                step = _debug_eval_delay_ms - elapsed;
            eos_delay(step);
            lv_timer_handler();
            elapsed += step;
        }
    }
#endif

    /* Restore the header-visible flag to its default BEFORE the JS runs so the
     * app observes the correct initial state and may still call
     * setAppHeaderVisible(false) to hide it.  The header itself stays hidden —
     * it must not appear until the app's interface has been built. */
    eos_activity_set_app_header_visible(a, true);
    eos_app_header_hide();

    /* Mark phase 2 complete. */
    if (ctx->loading_spinner)
    {
        eos_loading_spinner_set_phase_progress(ctx->loading_spinner, _APP_LIST_LOADING_PHASE_ENGINE, 100);
    }
    lv_refr_now(NULL);

    /* Run the JS — synchronous, blocks the main thread. */
    eos_result_t ret = spm_app_run(&ctx->pkg);
    if (ret != EOS_OK)
    {
        _app_handle_script_run_result(a, ctx, ret);
        return;
    }

    /* The app's interface is now built and is rendered on top of the loading
     * overlay from the very next frame.  Reveal the header in that same frame so
     * the app UI and header appear together while the overlay dims out.  An app
     * that called setAppHeaderVisible(false) during startup stays hidden. */
    if (eos_activity_is_app_header_visible(a))
    {
        eos_app_header_show(a);
    }

    /* JS eval succeeded. Dim spinner and icon smoothly while keeping
     * the pure-black background fully opaque. The spinner's dim
     * animation fires a callback that removes the overlay when done. */
    lv_obj_t *loading = ctx->loading_widget;
    if (!loading || !lv_obj_is_valid(loading))
    {
        return;
    }

    /* Animate icon opacity: 100% -> 0% over 400ms */
    lv_obj_t *icon = lv_obj_get_child(loading, 0);
    if (icon && lv_obj_is_valid(icon))
    {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, icon);
        lv_anim_set_exec_cb(&a, _loading_dim_exec_cb);
        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_duration(&a, 400);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }

    /* Spinner dim animation with callback to remove overlay */
    if (ctx->loading_spinner)
    {
        eos_loading_spinner_set_completed(ctx->loading_spinner, _loading_dim_done_cb, ctx);
    }
    else
    {
        /* No spinner — remove overlay immediately */
        lv_obj_del(loading);
        ctx->loading_widget = NULL;
        ctx->loading_spinner = NULL;
    }
}

/**
 * @brief Error handling for script launch failures — extracted from
 *        the original _app_on_enter for reuse in the deferred path.
 */
static void _app_handle_script_run_result(eos_activity_t *a, app_launch_ctx_t *ctx, eos_result_t ret)
{
    eos_script_error_type_t error_type = EOS_SCRIPT_FAULT_ERROR_EXCEPTION;
    if (ret == EOS_ERR_TIMEOUT)
    {
        error_type = EOS_SCRIPT_FAULT_UNRESPONSIVE;
    }
    else
    {
        const char *error_info = script_engine_get_error_info();
        if (error_info && strstr(error_info, "timeout"))
        {
            error_type = EOS_SCRIPT_FAULT_UNRESPONSIVE;
        }
    }

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

/**
 * @brief Read manifest only (fast, small file) — used in Phase A for app name
 */
static eos_result_t _app_list_build_manifest(const char *app_id, script_pkg_t *pkg)
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

    return EOS_OK;
}

static eos_result_t _app_list_launch_script_app(const char *app_id)
{
    /* Quick-fail: main.js must exist before we show a loading screen */
    char script_path[EOS_FS_PATH_MAX];
    snprintf(script_path, sizeof(script_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_SCRIPT_ENTRY_FILE_NAME, app_id);
    if (!eos_storage_is_file(script_path))
    {
        EOS_LOG_E("Can't find script: %s", script_path);
        return EOS_FAILED;
    }

    app_launch_ctx_t *ctx = eos_malloc_zeroed(sizeof(app_launch_ctx_t));
    if (!ctx)
    {
        EOS_LOG_E("Failed to allocate app launch context");
        return EOS_FAILED;
    }

    ctx->magic = _APP_LAUNCH_CTX_MAGIC;

    ctx->app_id = eos_strdup(app_id);
    if (!ctx->app_id)
    {
        EOS_LOG_E("Failed to copy app id");
        eos_free(ctx);
        return EOS_FAILED;
    }

    /* Phase A: read manifest only (small, fast) to get the app name */
    if (_app_list_build_manifest(app_id, &ctx->pkg) != EOS_OK)
    {
        eos_pkg_free(&ctx->pkg);
        eos_free(ctx->app_id);
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
    eos_activity_set_title(a, ctx->pkg.name);
    eos_activity_set_app_header_visible(a, true);

    EOS_LOG_D("view_size: %d, %d", lv_obj_get_width(app_view), lv_obj_get_height(app_view));

    /* Enter the activity immediately: loads the loading placeholder,
     * plays the zoom animation, then defers JS load to Phase B. */
    eos_activity_enter(a);
    return EOS_OK;
}

const char *eos_app_list_get_app_id(eos_activity_t *activity)
{
    if (!activity)
        return NULL;
    app_launch_ctx_t *ctx = (app_launch_ctx_t *)eos_activity_get_user_data(activity);
    if (!ctx)
        return NULL;
    /* Validate the sentinel to avoid type-confusion with other user_data
     * types (e.g. Flashlight uses _flash_light_card_pager_ctx_t). */
    if (ctx->magic != _APP_LAUNCH_CTX_MAGIC)
        return NULL;
    return ctx->app_id;
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

    /* Check if the app is already in the recents list — resume instead of fresh launch */
    eos_recent_app_entry_t *recent_entry = eos_recent_apps_find(app_id);
    if (recent_entry)
    {
        EOS_LOG_I("App '%s' found in recents — resuming", app_id);

        /* Capture a screenshot of the parked app's view right now, before resuming.
         * The app is detached/hidden but its LVGL widget tree is intact — snapshot
         * renders it directly into a draw buffer, independent of screen attachment.
         * This screenshot is used by the resume transition animation instead of the
         * black placeholder. */
        if (recent_entry->saved_stack_top)
        {
            lv_obj_t *parked_view = eos_activity_get_view(recent_entry->saved_stack_top);
            bool view_valid = (parked_view && lv_obj_is_valid(parked_view));
            EOS_LOG_I("[RECENT_CAPTURE] saved_stack_top=%p view=%p valid=%d",
                      (void *)recent_entry->saved_stack_top,
                      (void *)parked_view,
                      view_valid);
            if (view_valid)
            {
                int32_t vw = lv_obj_get_width(parked_view);
                int32_t vh = lv_obj_get_height(parked_view);
                bool hidden = lv_obj_has_flag(parked_view, LV_OBJ_FLAG_HIDDEN);
                uint32_t child_cnt = lv_obj_get_child_cnt(parked_view);
                lv_display_t *disp = lv_obj_get_display(parked_view);
                /* Check grandchildren: the 1st child may be the app's root container */
                uint32_t grandchild_cnt = 0;
                if (child_cnt > 0)
                {
                    lv_obj_t *first_child = lv_obj_get_child(parked_view, 0);
                    if (first_child)
                    {
                        grandchild_cnt = lv_obj_get_child_cnt(first_child);
                    }
                }
                EOS_LOG_I("[RECENT_CAPTURE] view size=%dx%d hidden=%d children=%u grandchildren=%u display=%p",
                          (int)vw,
                          (int)vh,
                          hidden,
                          (unsigned int)child_cnt,
                          (unsigned int)grandchild_cnt,
                          (void *)disp);
            }

            recent_entry->snap_buf = eos_activity_take_snapshot_standalone(recent_entry->saved_stack_top, true);
            if (recent_entry->snap_buf)
            {
                /* Sample a few pixels from the center of the draw buffer to
                 * verify it contains non-black data */
                lv_draw_buf_t *buf = recent_entry->snap_buf;
                int cx = buf->header.w / 2;
                int cy = buf->header.h / 2;
                uint16_t pixel_center = 0;
                uint16_t pixel_top_left = 0;
                uint16_t pixel_bottom_right = 0;
                if (buf->data)
                {
                    pixel_center = ((uint16_t *)buf->data)[cy * (buf->header.stride / 2) + cx];
                    pixel_top_left = ((uint16_t *)buf->data)[0];
                    pixel_bottom_right =
                        ((uint16_t *)buf->data)[(buf->header.h - 1) * (buf->header.stride / 2) + (buf->header.w - 1)];
                }
                EOS_LOG_I("[RECENT_CAPTURE] snapshot OK: %dx%d stride=%d cf=%d "
                          "pixels[TL=%04X C=%04X BR=%04X]",
                          (int)buf->header.w,
                          (int)buf->header.h,
                          (int)buf->header.stride,
                          (int)buf->header.cf,
                          (unsigned int)pixel_top_left,
                          (unsigned int)pixel_center,
                          (unsigned int)pixel_bottom_right);
            }
            else
            {
                EOS_LOG_W("[RECENT_CAPTURE] snapshot FAILED for '%s' — will use placeholder", app_id);
            }
        }
        else
        {
            EOS_LOG_W("[RECENT_CAPTURE] saved_stack_top is NULL for '%s'", app_id);
        }

        /* If another script app is active, suspend it first */
        eos_activity_t *cur = eos_activity_get_current();
        if (cur && eos_recent_apps_is_suspendable(cur))
        {
            eos_recent_apps_suspend_current();
        }
        return eos_recent_apps_resume_by_id(app_id);
    }

    /* If currently inside a script app, suspend it first instead of destroying */
    eos_activity_t *cur = eos_activity_get_current();
    if (cur && eos_recent_apps_is_suspendable(cur))
    {
        EOS_LOG_I("Suspending current app before launching new app");
        eos_recent_apps_suspend_current();
    }
    else
    {
        /* For non-script current (e.g. system app), use the old pop behavior */
        eos_activity_type_t current_type = eos_activity_get_type(cur);
        if (current_type == EOS_ACTIVITY_TYPE_APP)
        {
            EOS_LOG_I("Returning to app list before launching new app");
            _app_list_pop_to_app_list();
        }
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

/* ===================================================================
 * Unified transition animation — ONE lv_anim drives everything.
 *
 * All properties (scale, translate, opacity) for every layer are
 * computed from a single eased progress value `t` (0→255).  This
 * guarantees lockstep synchronisation and avoids the zero-delta
 * frames that occur when independent lv_anim instances with small
 * value ranges (e.g. app_snapshot 64→256, range=192) quantise to
 * zero at the tail of an ease-out curve.
 *
 * Icon opacity crossfade thresholds are keyed to the scale midpoint
 * (FOCUS_SCALE/2 = 1024), same as before:
 *   Opening  (256→2048): scale=1024 at t≈109
 *   Closing  (2048→256): scale=1024 at t≈146
 * =================================================================== */

/** Helper: eased lerp with proper rounding. */
static inline int32_t _elerp(int32_t a, int32_t b, int32_t t)
{
    return a + ((b - a) * t + 127) / 255;
}

static void _transition_anim_exec_cb(void *var, int32_t v)
{
    _transition_anim_ctx_t *ctx = (_transition_anim_ctx_t *)var;
    int32_t t = 255 - v; /* t: 0→255 eased */

    /* list_snapshot scale + translate ----------------------------*/
    if (ctx->list_snapshot && lv_obj_is_valid(ctx->list_snapshot))
    {
        lv_image_set_scale(ctx->list_snapshot, _elerp(ctx->list_scale_start, ctx->list_scale_end, t));
        lv_obj_set_style_translate_x(ctx->list_snapshot, _elerp(ctx->list_tx_start, ctx->list_tx_end, t), 0);
        lv_obj_set_style_translate_y(ctx->list_snapshot, _elerp(ctx->list_ty_start, ctx->list_ty_end, t), 0);
    }

    /* icon_clone transform scale + translate + opacity -----------*/
    if (ctx->icon_clone && lv_obj_is_valid(ctx->icon_clone))
    {
        lv_obj_set_style_transform_scale(ctx->icon_clone, _elerp(ctx->icon_scale_start, ctx->icon_scale_end, t), 0);
        lv_obj_set_style_translate_x(ctx->icon_clone, _elerp(ctx->icon_tx_start, ctx->icon_tx_end, t), 0);
        lv_obj_set_style_translate_y(ctx->icon_clone, _elerp(ctx->icon_ty_start, ctx->icon_ty_end, t), 0);
    }

    /* app_snapshot image scale + translate + opacity -------------*/
    if (ctx->app_snapshot && lv_obj_is_valid(ctx->app_snapshot))
    {
        lv_image_set_scale(ctx->app_snapshot, _elerp(ctx->app_scale_start, ctx->app_scale_end, t));
        lv_obj_set_style_translate_x(ctx->app_snapshot, _elerp(ctx->app_tx_start, ctx->app_tx_end, t), 0);
        lv_obj_set_style_translate_y(ctx->app_snapshot, _elerp(ctx->app_ty_start, ctx->app_ty_end, t), 0);
    }

    /* Opacity crossfade (keyed to scale midpoint) ----------------*/
    int32_t icon_opa;
    if (ctx->opening)
    {
        if (t < 109)
            icon_opa = 255 - (t * 255 / 109);
        else
            icon_opa = 0;
    }
    else
    {
        if (t < 146)
            icon_opa = 0;
        else
            icon_opa = 255 * (t - 146) / 109;
    }
    int32_t app_opa = 255 - icon_opa;

    if (ctx->obj_a && lv_obj_is_valid(ctx->obj_a))
        lv_obj_set_style_opa(ctx->obj_a, (lv_opa_t)(ctx->opening ? icon_opa : app_opa), 0);
    if (ctx->obj_b && lv_obj_is_valid(ctx->obj_b))
        lv_obj_set_style_opa(ctx->obj_b, (lv_opa_t)(ctx->opening ? app_opa : icon_opa), 0);
}

static void _free_transition_anim_ctx(lv_timer_t *t)
{
    _transition_anim_ctx_t *ctx = lv_timer_get_user_data(t);
    eos_free(ctx);
}

static void _transition_anim_ready_cb(lv_anim_t *a)
{
    _transition_anim_ctx_t *ctx = (_transition_anim_ctx_t *)lv_anim_get_user_data(a);

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
        if (g->completed >= g->expected && g->callback)
        {
            g->callback(g->user_data);
        }
    }

    /* Defer free to avoid use-after-free in lv_anim internals */
    lv_timer_t *t = lv_timer_create(_free_transition_anim_ctx, 10, ctx);
    lv_timer_set_repeat_count(t, 1);
}

/** Set up the unified transition animation that drives everything. */
static void _transition_anim_start(_transition_anim_ctx_t *ctx, uint32_t duration, uint32_t delay)
{
    /* Apply initial values for all properties */
    if (ctx->list_snapshot && lv_obj_is_valid(ctx->list_snapshot))
    {
        lv_image_set_scale(ctx->list_snapshot, ctx->list_scale_start);
        lv_obj_set_style_translate_x(ctx->list_snapshot, ctx->list_tx_start, 0);
        lv_obj_set_style_translate_y(ctx->list_snapshot, ctx->list_ty_start, 0);
    }
    if (ctx->icon_clone && lv_obj_is_valid(ctx->icon_clone))
    {
        lv_obj_set_style_transform_scale(ctx->icon_clone, ctx->icon_scale_start, 0);
        lv_obj_set_style_translate_x(ctx->icon_clone, ctx->icon_tx_start, 0);
        lv_obj_set_style_translate_y(ctx->icon_clone, ctx->icon_ty_start, 0);
    }
    if (ctx->app_snapshot && lv_obj_is_valid(ctx->app_snapshot))
    {
        lv_image_set_scale(ctx->app_snapshot, ctx->app_scale_start);
        lv_obj_set_style_translate_x(ctx->app_snapshot, ctx->app_tx_start, 0);
        lv_obj_set_style_translate_y(ctx->app_snapshot, ctx->app_ty_start, 0);
    }
    if (ctx->obj_a && lv_obj_is_valid(ctx->obj_a))
        lv_obj_set_style_opa(ctx->obj_a, LV_OPA_COVER, 0);
    if (ctx->obj_b && lv_obj_is_valid(ctx->obj_b))
        lv_obj_set_style_opa(ctx->obj_b, LV_OPA_TRANSP, 0);

    lv_anim_init(&ctx->lv_anim);
    lv_anim_set_var(&ctx->lv_anim, ctx);
    lv_anim_set_values(&ctx->lv_anim, 255, 0);
    lv_anim_set_exec_cb(&ctx->lv_anim, _transition_anim_exec_cb);
    lv_anim_set_path_cb(&ctx->lv_anim, lv_anim_path_custom_bezier3);
    lv_anim_set_bezier3_param(&ctx->lv_anim,
                              _EASE_OUT_SNAP_BX1,
                              _EASE_OUT_SNAP_BY1,
                              _EASE_OUT_SNAP_BX2,
                              _EASE_OUT_SNAP_BY2);
    lv_anim_set_duration(&ctx->lv_anim, duration);
    lv_anim_set_delay(&ctx->lv_anim, delay);
    lv_anim_set_completed_cb(&ctx->lv_anim, _transition_anim_ready_cb);
    lv_anim_set_user_data(&ctx->lv_anim, ctx);

    if (ctx->group)
    {
        ctx->group->expected++;
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
        /* Check for a pre-captured screenshot (from recents resume).
         * If present, create the app_snapshot image directly from the
         * draw buffer on the snapshot layer — bypassing the view-capture
         * step.  Register it for cleanup so the animation system deletes
         * it when the transition completes. */
        lv_draw_buf_t *stored = eos_activity_get_snap_buf(to);
        if (stored)
        {
            app_snapshot = lv_image_create(eos_overlay_get_snapshot_layer());
            lv_image_set_src(app_snapshot, stored);
            lv_obj_set_size(app_snapshot, stored->header.w, stored->header.h);
            /* Match the real view's position for correct pivot/translate math */
            {
                lv_obj_t *to_view = eos_activity_get_view(to);
                if (to_view)
                    lv_obj_set_pos(app_snapshot, lv_obj_get_x(to_view), lv_obj_get_y(to_view));
            }
            /* Register for cleanup: image auto-deleted, draw_buf freed on transition end */
            eos_activity_register_snapshot_for_cleanup(app_snapshot, stored, to);
            eos_activity_set_snap_buf(to, NULL); /* consumed */
            EOS_LOG_I("OPEN ANIM: app_snapshot from stored snap_buf=%p (%dx%d)",
                      (void *)stored,
                      (int)stored->header.w,
                      (int)stored->header.h);
        }
        else
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

    /* ================================================================
     * Unified transition animation — ONE lv_anim for everything.
     * All scale/translate/opacity values for every layer are computed
     * from a single eased progress `t` in _transition_anim_exec_cb,
     * guaranteeing lockstep sync regardless of value-range size.
     * ================================================================ */

    _transition_anim_ctx_t *ctx = eos_malloc_zeroed(sizeof(_transition_anim_ctx_t));
    if (!ctx)
    {
        if (icon_clone)
            lv_obj_delete(icon_clone);
        return;
    }

    ctx->list_snapshot = list_snapshot;
    ctx->icon_clone = icon_clone;
    ctx->app_snapshot = app_snapshot;
    ctx->group = group;
    ctx->opening = opening;

    /* list_snapshot params ---------------------------------------*/
    if (list_snapshot)
    {
        ctx->list_scale_start = opening ? 256 : _APP_LIST_ANIM_FOCUS_SCALE;
        ctx->list_scale_end = opening ? _APP_LIST_ANIM_FOCUS_SCALE : 256;
        ctx->list_tx_start = opening ? 0 : focus_translate_x;
        ctx->list_tx_end = opening ? focus_translate_x : 0;
        ctx->list_ty_start = opening ? 0 : focus_translate_y;
        ctx->list_ty_end = opening ? focus_translate_y : 0;
    }

    /* icon_clone params ------------------------------------------*/
    if (icon_clone)
    {
        ctx->icon_scale_start = opening ? 256 : _APP_LIST_ANIM_FOCUS_SCALE;
        ctx->icon_scale_end = opening ? _APP_LIST_ANIM_FOCUS_SCALE : 256;
        ctx->icon_tx_start = opening ? 0 : focus_translate_x;
        ctx->icon_tx_end = opening ? focus_translate_x : 0;
        ctx->icon_ty_start = opening ? 0 : focus_translate_y;
        ctx->icon_ty_end = opening ? focus_translate_y : 0;
    }

    /* app_snapshot params ----------------------------------------*/
    ctx->app_scale_start = opening ? _APP_LIST_ANIM_MIN_SACLE : 256;
    ctx->app_scale_end = opening ? 256 : _APP_LIST_ANIM_MIN_SACLE;
    /* app_snapshot translate keeps its visual center aligned with
     * icon_clone: snap_translate = icon_translate - focus_translate */
    ctx->app_tx_start = opening ? -focus_translate_x : 0;
    ctx->app_tx_end = opening ? 0 : -focus_translate_x;
    ctx->app_ty_start = opening ? -focus_translate_y : 0;
    ctx->app_ty_end = opening ? 0 : -focus_translate_y;

    /* opacity crossfade pair -------------------------------------*/
    ctx->obj_a = opening ? icon_clone : app_snapshot; /* fades out */
    ctx->obj_b = opening ? app_snapshot : icon_clone; /* fades in  */
    ctx->focus_icon = opening ? NULL : focus_icon; /* restored on close */

    _transition_anim_start(ctx, total_duration, 0);

    /* Cleanup: when app_snapshot is deleted, also delete icon_clone */
    lv_obj_add_event_cb(app_snapshot, _app_list_cleanup_extra_cb, LV_EVENT_DELETE, icon_clone);

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
