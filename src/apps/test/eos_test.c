/**
 * @file eos_test.c
 * @brief System function test
 */

#include "eos_test.h"
#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

/* Includes ---------------------------------------------------*/
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_swipe_panel.h"
#include "lvgl.h"
#include "eos_image.h"
#include "eos_msg_list.h"
#include "eos_lang.h"
#include "eos_log.h"
#include "eos_basic_widgets.h"
#include "eos_std_widgets.h"
#include "eos_event.h"
#include "eos_port.h"
#include "eos_app.h"
#include "eos_app_list.h"
#include "eos_core.h"
#include "spm.h"
#include "eos_pkg_mgr.h"
#include "eos_watchface_list.h"
#include "eos_icon.h"
#include "eos_toast.h"
#include "eos_slide_widget.h"
#include "eos_font.h"
#include "eos_service_sensor.h"
#include "eos_dev_sensor.h"
#include "sensor/eos_test_sensor_chart.h"
#include "sensor/eos_test_sensor_multi_chart.h"
#include "battery/eos_test_battery_history.h"
#include "package/eos_test_package.h"
#include "input/eos_test_input_page.h"
#include "eos_test_runner.h"
#include "eos_service_audio.h"
#include "eos_audio_player.h"
#include "eos_dev_speaker.h"
#include "eos_crown.h"
#include "eos_app_header.h"
#include "eos_service_storage.h"
#include "eos_mem.h"
#include "eos_theme.h"
#include "eos_activity.h"
#include "eos_panel.h"
#include "eos_fault_panel.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "Test"
// #define TEST_USE_ZH_FONT
#ifdef TEST_USE_ZH_FONT
LV_FONT_DECLARE(eos_font_resource_han_rounded_30);
#endif
#define LV_KB_BTN(width) LV_BUTTONMATRIX_CTRL_POPOVER | width

/* Variables --------------------------------------------------*/
static lv_obj_t *img = NULL; // Global image object
static lv_obj_t *ta = NULL; // Global text input object
/* Function Implementations -----------------------------------*/

typedef struct
{
    const char *symbol;
    uint32_t codepoint;
} symbol_t;

typedef struct
{
    lv_obj_t *launcher_screen;
    lv_obj_t *list_screen;
    lv_obj_t *debug_bar;
    char *current_app_id;
    bool debug_active;
    bool global_cb_registered;
} test_app_debug_ctx_t;

typedef struct
{
    lv_obj_t *screen;
    lv_obj_t *status_label;
    lv_obj_t *play_btn;
    lv_obj_t *play_btn_label;
    lv_obj_t *pause_btn;
    lv_obj_t *stop_btn;
    lv_obj_t *progress_slider;
    lv_obj_t *time_label;
    lv_obj_t *volume_slider;
    lv_obj_t *volume_label;
    lv_timer_t *state_timer;
} test_audio_page_ctx_t;

typedef struct
{
    lv_obj_t *screen;
    lv_obj_t *status_label;
    lv_obj_t *record_btn;
    lv_obj_t *record_btn_label;
    lv_obj_t *playback_btn;
    lv_timer_t *state_timer;
    bool is_recording;
} test_recording_page_ctx_t;

// All LVGL built-in symbols
static const symbol_t lv_symbols[] = {
    {LV_SYMBOL_AUDIO, 0xF001},        {LV_SYMBOL_VIDEO, 0xF008},         {LV_SYMBOL_LIST, 0xF00B},
    {LV_SYMBOL_OK, 0xF00C},           {LV_SYMBOL_CLOSE, 0xF00D},         {LV_SYMBOL_POWER, 0xF011},
    {LV_SYMBOL_SETTINGS, 0xF013},     {LV_SYMBOL_HOME, 0xF015},          {LV_SYMBOL_DOWNLOAD, 0xF019},
    {LV_SYMBOL_DRIVE, 0xF01C},        {LV_SYMBOL_REFRESH, 0xF021},       {LV_SYMBOL_MUTE, 0xF026},
    {LV_SYMBOL_VOLUME_MID, 0xF027},   {LV_SYMBOL_VOLUME_MAX, 0xF028},    {LV_SYMBOL_IMAGE, 0xF03E},
    {LV_SYMBOL_TINT, 0xF043},         {LV_SYMBOL_PREV, 0xF048},          {LV_SYMBOL_PLAY, 0xF04B},
    {LV_SYMBOL_PAUSE, 0xF04C},        {LV_SYMBOL_STOP, 0xF04D},          {LV_SYMBOL_NEXT, 0xF051},
    {LV_SYMBOL_EJECT, 0xF052},        {LV_SYMBOL_LEFT, 0xF053},          {LV_SYMBOL_RIGHT, 0xF054},
    {LV_SYMBOL_PLUS, 0xF067},         {LV_SYMBOL_MINUS, 0xF068},         {LV_SYMBOL_EYE_OPEN, 0xF06E},
    {LV_SYMBOL_EYE_CLOSE, 0xF070},    {LV_SYMBOL_WARNING, 0xF071},       {LV_SYMBOL_SHUFFLE, 0xF074},
    {LV_SYMBOL_UP, 0xF077},           {LV_SYMBOL_DOWN, 0xF078},          {LV_SYMBOL_LOOP, 0xF079},
    {LV_SYMBOL_DIRECTORY, 0xF07B},    {LV_SYMBOL_UPLOAD, 0xF093},        {LV_SYMBOL_CALL, 0xF095},
    {LV_SYMBOL_CUT, 0xF0C4},          {LV_SYMBOL_COPY, 0xF0C5},          {LV_SYMBOL_SAVE, 0xF0C7},
    {LV_SYMBOL_BARS, 0xF0C9},         {LV_SYMBOL_ENVELOPE, 0xF0E0},      {LV_SYMBOL_CHARGE, 0xF0E7},
    {LV_SYMBOL_PASTE, 0xF0EA},        {LV_SYMBOL_BELL, 0xF0F3},          {LV_SYMBOL_KEYBOARD, 0xF11C},
    {LV_SYMBOL_GPS, 0xF124},          {LV_SYMBOL_FILE, 0xF158},          {LV_SYMBOL_WIFI, 0xF1EB},
    {LV_SYMBOL_BATTERY_FULL, 0xF240}, {LV_SYMBOL_BATTERY_3, 0xF241},     {LV_SYMBOL_BATTERY_2, 0xF242},
    {LV_SYMBOL_BATTERY_1, 0xF243},    {LV_SYMBOL_BATTERY_EMPTY, 0xF244}, {LV_SYMBOL_USB, 0xF287},
    {LV_SYMBOL_BLUETOOTH, 0xF293},    {LV_SYMBOL_TRASH, 0xF2ED},         {LV_SYMBOL_EDIT, 0xF304},
    {LV_SYMBOL_BACKSPACE, 0xF55A},    {LV_SYMBOL_SD_CARD, 0xF7C2},       {LV_SYMBOL_NEW_LINE, 0xF8A2},
    {LV_SYMBOL_DUMMY, 0xFFFF},
};

static test_app_debug_ctx_t s_test_app_debug = {0};
static test_audio_page_ctx_t s_test_audio_page = {0};
static test_recording_page_ctx_t s_test_recording_page = {0};
static lv_coord_t s_debug_bar_global_x = 0;
static lv_coord_t s_debug_bar_global_y = 0;
static bool s_debug_bar_global_pos_valid = false;
static const char *s_test_audio_primary_path = "fs/music.mp3";
static const char *s_test_audio_fallback_path = "/music.mp3";

#define TEST_APP_DEBUG_BAR_W 220
#define TEST_APP_DEBUG_BAR_H 64

/* Activity Lifecycle -----------------------------------------*/

static void _test_activity_on_enter(eos_activity_t *activity)
{
    LV_UNUSED(activity);
}

static void _test_activity_on_destroy(eos_activity_t *activity)
{
    LV_UNUSED(activity);
}

static void _test_activity_on_pause(eos_activity_t *activity)
{
    LV_UNUSED(activity);
}

static void _test_activity_on_resume(eos_activity_t *activity)
{
    LV_UNUSED(activity);
}

static const eos_activity_lifecycle_t s_test_activity_lifecycle = {.on_enter = _test_activity_on_enter,
                                                                   .on_destroy = _test_activity_on_destroy,
                                                                   .on_pause = _test_activity_on_pause,
                                                                   .on_resume = _test_activity_on_resume};

static void _test_app_debug_clamp_bar_pos(int32_t *x, int32_t *y, int32_t w, int32_t h)
{
    if (!(x && y))
        return;

    if (*x < 0)
        *x = 0;
    if (*y < 0)
        *y = 0;
    if (*x + w > EOS_DISPLAY_WIDTH)
        *x = EOS_DISPLAY_WIDTH - w;
    if (*y + h > EOS_DISPLAY_HEIGHT)
        *y = EOS_DISPLAY_HEIGHT - h;
    if (*x < 0)
        *x = 0;
    if (*y < 0)
        *y = 0;
}

static void _test_app_debug_set_global_bar_pos(int32_t x, int32_t y, int32_t w, int32_t h)
{
    _test_app_debug_clamp_bar_pos(&x, &y, w, h);
    s_debug_bar_global_x = x;
    s_debug_bar_global_y = y;
    s_debug_bar_global_pos_valid = true;
}

static void _test_app_debug_save_bar_pos(lv_obj_t *bar)
{
    if (!(bar && lv_obj_is_valid(bar)))
        return;

    int32_t x = lv_obj_get_x(bar);
    int32_t y = lv_obj_get_y(bar);
    int32_t w = lv_obj_get_width(bar);
    int32_t h = lv_obj_get_height(bar);
    if (w <= 0)
        w = TEST_APP_DEBUG_BAR_W;
    if (h <= 0)
        h = TEST_APP_DEBUG_BAR_H;

    _test_app_debug_set_global_bar_pos(x, y, w, h);
}

static void _test_app_debug_sync_bar_pos(void)
{
    lv_obj_t *bar = s_test_app_debug.debug_bar;
    if (!(bar && lv_obj_is_valid(bar) && s_debug_bar_global_pos_valid))
        return;

    int32_t x = s_debug_bar_global_x;
    int32_t y = s_debug_bar_global_y;
    _test_app_debug_clamp_bar_pos(&x, &y, lv_obj_get_width(bar), lv_obj_get_height(bar));

    lv_obj_set_pos(bar, x, y);
    s_debug_bar_global_x = x;
    s_debug_bar_global_y = y;
}

static void _test_app_debug_clear_current_app_id(void)
{
    if (s_test_app_debug.current_app_id)
    {
        eos_free(s_test_app_debug.current_app_id);
        s_test_app_debug.current_app_id = NULL;
    }
}

static void _test_app_debug_destroy_bar_internal(bool save_pos)
{
    if (s_test_app_debug.debug_bar && lv_obj_is_valid(s_test_app_debug.debug_bar))
    {
        if (save_pos)
        {
            _test_app_debug_save_bar_pos(s_test_app_debug.debug_bar);
        }
        lv_obj_delete_async(s_test_app_debug.debug_bar);
    }
    s_test_app_debug.debug_bar = NULL;
}

static void _test_app_debug_destroy_bar(void)
{
    _test_app_debug_destroy_bar_internal(true);
}

static void _test_app_debug_create_bar(void);
static void _test_app_debug_app_installed_cb(eos_event_t *e);

static void _test_app_debug_script_exited_cb(eos_event_t *e)
{
    LV_UNUSED(e);

    if (!s_test_app_debug.debug_active)
        return;

    /* Do not overwrite persisted position in script-exit timing path. */
    _test_app_debug_destroy_bar_internal(false);
    s_test_app_debug.debug_active = false;
    _test_app_debug_clear_current_app_id();
}

static void _test_app_debug_global_screen_loaded_cb(lv_event_t *e)
{
    lv_obj_t *scr = lv_event_get_param(e);
    if (!s_test_app_debug.debug_active)
        return;
    if (!(scr && lv_obj_is_valid(scr) && lv_obj_has_class(scr, &lv_obj_class)))
        return;

    eos_app_header_hide();
    if (s_test_app_debug.debug_bar && lv_obj_is_valid(s_test_app_debug.debug_bar))
    {
        _test_app_debug_sync_bar_pos();
        lv_obj_move_foreground(s_test_app_debug.debug_bar);
    }
}

static void _test_app_debug_register_global_cb(void)
{
    if (s_test_app_debug.global_cb_registered)
        return;

    // eos_event_add_global_cb(_test_app_debug_global_screen_loaded_cb,
    //                         EOS_EVENT_GLOBAL_SCREEN_LOADED,
    //                         NULL);
    eos_event_subscribe(EOS_EVENT_SCRIPT_EXITED, _test_app_debug_script_exited_cb, NULL);
    s_test_app_debug.global_cb_registered = true;
}

static void _test_app_debug_unregister_global_cb(void)
{
    if (!s_test_app_debug.global_cb_registered)
        return;

    eos_event_unsubscribe_all(_test_app_debug_script_exited_cb);
    eos_event_unsubscribe(EOS_EVENT_APP_INSTALLED, _test_app_debug_app_installed_cb);
    eos_event_cleanup_now();
    s_test_app_debug.global_cb_registered = false;
}

static eos_result_t _test_app_debug_create_pkg(const char *app_id, script_pkg_t **out_pkg)
{
    if (!(app_id && out_pkg))
        return EOS_ERR_SCRIPT_NULL_PACKAGE;

    char manifest_path[EOS_FS_PATH_MAX];
    snprintf(manifest_path, sizeof(manifest_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_MANIFEST_FILE_NAME, app_id);

    script_pkg_t *pkg = eos_malloc_zeroed(sizeof(script_pkg_t));
    if (!pkg)
        return EOS_ERR_MEM;

    pkg->type = SCRIPT_TYPE_APPLICATION;
    if (script_engine_get_manifest(manifest_path, pkg) != EOS_OK)
    {
        EOS_LOG_E("Read manifest failed: %s", manifest_path);
        eos_free(pkg);
        return EOS_FAILED;
    }

    char script_path[EOS_FS_PATH_MAX];
    snprintf(script_path, sizeof(script_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_SCRIPT_ENTRY_FILE_NAME, app_id);

    char base_path[EOS_FS_PATH_MAX];
    snprintf(base_path, sizeof(base_path), EOS_APP_INSTALLED_DIR "%s/", app_id);
    pkg->base_path = eos_strdup(base_path);

    if (!eos_storage_is_file(script_path))
    {
        EOS_LOG_E("Can't find script: %s", script_path);
        eos_pkg_free(pkg);
        eos_free(pkg);
        return EOS_FAILED;
    }

    pkg->script_str = eos_storage_read_file(script_path);
    if (!pkg->script_str)
    {
        eos_pkg_free(pkg);
        eos_free(pkg);
        return EOS_FAILED;
    }

    *out_pkg = pkg;
    return EOS_OK;
}

static void _test_app_debug_show_error(lv_obj_t *scr, const char *app_id, eos_result_t ret)
{
    lv_obj_clean(scr);
    lv_obj_remove_style_all(scr);
    // lv_obj_add_style(scr, eos_theme_get_view_style(), 0);

    lv_obj_t *list = eos_std_info_create(scr,
                                         EOS_THEME_DANGEROS_COLOR,
                                         RI_BUG_LINE,
                                         eos_lang_get_text(STR_ID_APP_RUN_ERR_TITLE),
                                         eos_lang_get_text(STR_ID_APP_RUN_ERR));

    char info_str[1024];
    snprintf(info_str, sizeof(info_str), "Code: %d\nAppID: %s\nError: %s", ret, app_id, script_engine_get_error_info());
    eos_list_add_comment(list, info_str);
}

static void _test_app_debug_restore_after_error(const char *app_id)
{
    if (!app_id)
        return;

    s_test_app_debug.debug_active = true;
    _test_app_debug_clear_current_app_id();
    s_test_app_debug.current_app_id = (char *)eos_strdup(app_id);
    _test_app_debug_create_bar();
    _test_app_debug_sync_bar_pos();
}

static eos_result_t _test_app_debug_start_internal(const char *app_id)
{
    if (!(app_id && s_test_app_debug.list_screen && lv_obj_is_valid(s_test_app_debug.list_screen)))
        return EOS_ERR_SCRIPT_NULL_PACKAGE;

    if (script_engine_get_state() != SCRIPT_ENGINE_STATE_UNINITIALIZED
        && script_engine_get_state() != SCRIPT_ENGINE_STATE_EXCEPTION)
    {
        script_engine_request_stop();
    }

    _test_app_debug_register_global_cb();
    _test_app_debug_clear_current_app_id();
    s_test_app_debug.current_app_id = (char *)eos_strdup(app_id);
    if (!s_test_app_debug.current_app_id)
    {
        s_test_app_debug.debug_active = false;
        return EOS_ERR_MEM;
    }
    s_test_app_debug.debug_active = true;

    // Create new activity for the app
    eos_activity_t *activity = eos_activity_create(&s_test_activity_lifecycle);
    if (!activity)
    {
        s_test_app_debug.debug_active = false;
        _test_app_debug_clear_current_app_id();
        return EOS_FAILED;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        s_test_app_debug.debug_active = false;
        _test_app_debug_clear_current_app_id();
        return EOS_FAILED;
    }

    eos_activity_set_view(activity, view);
    eos_activity_set_title(activity, app_id);
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    script_pkg_t *pkg = NULL;
    eos_result_t ret = _test_app_debug_create_pkg(app_id, &pkg);
    if (ret != EOS_OK)
    {
        s_test_app_debug.debug_active = false;
        _test_app_debug_clear_current_app_id();
        return ret;
    }

    eos_activity_enter(activity);
    ret = spm_app_run(pkg);
    if (ret != EOS_OK)
    {
        _test_app_debug_show_error(view, app_id, ret);
        _test_app_debug_restore_after_error(app_id);
    }

    return ret;
}

static void _test_app_debug_safe_nav_cleanup(void)
{
    // Activity-based cleanup - just return to previous activity
    eos_activity_back();
}

static void _test_app_debug_exit_current_app(void)
{
    if (script_engine_get_state() == SCRIPT_ENGINE_STATE_RUNNING
        || script_engine_get_state() == SCRIPT_ENGINE_STATE_IDLE
        || script_engine_get_state() == SCRIPT_ENGINE_STATE_EXCEPTION)
    {
        script_engine_request_stop();
    }

    s_test_app_debug.debug_active = false;
    _test_app_debug_destroy_bar();
    _test_app_debug_clear_current_app_id();
    _test_app_debug_unregister_global_cb();

    _test_app_debug_safe_nav_cleanup();
}

static void _test_app_debug_restart_current_app(void)
{
    if (!s_test_app_debug.current_app_id)
        return;

    char *app_id = (char *)eos_strdup(s_test_app_debug.current_app_id);
    if (!app_id)
        return;

    if (script_engine_get_state() == SCRIPT_ENGINE_STATE_RUNNING
        || script_engine_get_state() == SCRIPT_ENGINE_STATE_IDLE
        || script_engine_get_state() == SCRIPT_ENGINE_STATE_EXCEPTION)
    {
        script_engine_request_stop();
    }

    _test_app_debug_safe_nav_cleanup();

    _test_app_debug_create_bar();
    _test_app_debug_start_internal(app_id);
    eos_free(app_id);
}

static void _test_app_debug_restart_btn_cb(lv_event_t *e)
{
    _test_app_debug_restart_current_app();
}

static void _test_app_debug_exit_btn_cb(lv_event_t *e)
{
    _test_app_debug_exit_current_app();
}

/* Drag handle callback: PRESSED=highlight, PRESSING=move bar, RELEASED=restore */
static void _test_app_debug_drag_handle_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *handle = lv_event_get_target(e);
    lv_obj_t *bar = lv_obj_get_parent(handle);

    if (code == LV_EVENT_PRESSING)
    {
        lv_indev_t *indev = lv_indev_active();
        if (!indev)
            return;
        lv_point_t vect;
        lv_indev_get_vect(indev, &vect);
        if (vect.x == 0 && vect.y == 0)
            return;

        int32_t new_x = lv_obj_get_x(bar) + vect.x;
        int32_t new_y = lv_obj_get_y(bar) + vect.y;
        int32_t bar_w = lv_obj_get_width(bar);
        int32_t bar_h = lv_obj_get_height(bar);

        if (new_x < 0)
            new_x = 0;
        if (new_y < 0)
            new_y = 0;
        if (new_x + bar_w > EOS_DISPLAY_WIDTH)
            new_x = EOS_DISPLAY_WIDTH - bar_w;
        if (new_y + bar_h > EOS_DISPLAY_HEIGHT)
            new_y = EOS_DISPLAY_HEIGHT - bar_h;

        lv_obj_set_pos(bar, new_x, new_y);
        _test_app_debug_set_global_bar_pos(new_x, new_y, bar_w, bar_h);
    }
    else if (code == LV_EVENT_PRESSED)
    {
        lv_obj_set_style_bg_color(handle, lv_color_hex(0x3A4550), 0);
        lv_obj_set_style_bg_opa(handle, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(handle, lv_color_hex(0xFFFFFF), 0);
    }
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
    {
        _test_app_debug_save_bar_pos(bar);
        lv_obj_set_style_bg_opa(handle, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(handle, lv_color_hex(0x8090A0), 0);
    }
}

static void _test_app_debug_create_bar(void)
{
    _test_app_debug_destroy_bar();

    lv_obj_t *bar = lv_obj_create(lv_layer_top());
    s_test_app_debug.debug_bar = bar;
    lv_obj_remove_style_all(bar);

    const int32_t bar_w = TEST_APP_DEBUG_BAR_W;
    const int32_t bar_h = TEST_APP_DEBUG_BAR_H;
    int32_t bar_x = (EOS_DISPLAY_WIDTH - bar_w) / 2;
    int32_t bar_y = 18;

    if (s_debug_bar_global_pos_valid)
    {
        bar_x = s_debug_bar_global_x;
        bar_y = s_debug_bar_global_y;
    }

    _test_app_debug_clamp_bar_pos(&bar_x, &bar_y, bar_w, bar_h);

    lv_obj_set_size(bar, bar_w, bar_h);
    lv_obj_set_pos(bar, bar_x, bar_y);
    lv_obj_set_style_radius(bar, 20, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_90, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 8, 0);
    lv_obj_set_style_pad_column(bar, 8, 0);
    lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    _test_app_debug_set_global_bar_pos(bar_x, bar_y, bar_w, bar_h);

    /* Drag handle (left side) */
    lv_obj_t *drag_handle = lv_obj_create(bar);
    lv_obj_remove_style_all(drag_handle);
    lv_obj_set_size(drag_handle, 44, lv_pct(100));
    lv_obj_set_style_radius(drag_handle, 12, 0);
    lv_obj_set_style_bg_opa(drag_handle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(drag_handle, lv_color_hex(0x8090A0), 0);
    lv_obj_add_flag(drag_handle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(drag_handle, _test_app_debug_drag_handle_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *drag_icon = lv_label_create(drag_handle);
    lv_label_set_text(drag_icon, RI_DRAG_MOVE_LINE);
    lv_obj_center(drag_icon);

    /* Restart button (icon only) */
    lv_obj_t *restart_btn = lv_button_create(bar);
    lv_obj_set_size(restart_btn, 72, lv_pct(100));
    lv_obj_add_event_cb(restart_btn, _test_app_debug_restart_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *restart_icon = lv_label_create(restart_btn);
    lv_label_set_text(restart_icon, RI_RESTART_LINE);
    lv_obj_center(restart_icon);

    /* Exit button (icon only) */
    lv_obj_t *exit_btn = lv_button_create(bar);
    lv_obj_set_size(exit_btn, 72, lv_pct(100));
    lv_obj_set_style_bg_color(exit_btn, EOS_THEME_DANGEROS_COLOR, 0);
    lv_obj_add_event_cb(exit_btn, _test_app_debug_exit_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *exit_icon = lv_label_create(exit_btn);
    lv_label_set_text(exit_icon, RI_CLOSE_FILL);
    lv_obj_center(exit_icon);
}

static void _test_app_debug_app_btn_cb(lv_event_t *e)
{
    const char *app_id = (const char *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(app_id);

    _test_app_debug_create_bar();
    _test_app_debug_start_internal(app_id);
}

static void _test_app_debug_app_btn_create(lv_obj_t *parent, const char *app_id)
{
    char icon_path[EOS_FS_PATH_MAX];
    snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME, app_id);
    if (!eos_storage_is_file(icon_path))
    {
        memcpy(icon_path, EOS_IMG_APP, sizeof(EOS_IMG_APP));
    }

    char manifest_path[EOS_FS_PATH_MAX];
    snprintf(manifest_path, sizeof(manifest_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_MANIFEST_FILE_NAME, app_id);

    script_pkg_t pkg = {0};
    if (script_engine_get_manifest(manifest_path, &pkg) != EOS_OK)
    {
        EOS_LOG_E("Read manifest failed: %s", manifest_path);
        return;
    }

    lv_obj_t *btn = eos_list_add_button(parent, icon_path, pkg.name);
    lv_obj_add_event_cb(btn, _test_app_debug_app_btn_cb, LV_EVENT_CLICKED, (void *)app_id);
    eos_app_obj_auto_delete(btn, app_id);
    eos_pkg_free(&pkg);
}

static void _test_app_debug_app_installed_cb(eos_event_t *e)
{
    lv_obj_t *parent = eos_event_get_user_data(e);
    const char *installed_app_id = eos_event_get_param(e);
    EOS_CHECK_PTR_RETURN(parent && installed_app_id);

    _test_app_debug_app_btn_create(parent, installed_app_id);
}

static void _test_app_debug_list_delete_cb(lv_event_t *e)
{
    lv_obj_t *scr = lv_event_get_target(e);
    if (scr == s_test_app_debug.list_screen)
    {
        s_test_app_debug.list_screen = NULL;
        s_test_app_debug.launcher_screen = NULL;
    }
    _test_app_debug_destroy_bar();
    _test_app_debug_clear_current_app_id();
    _test_app_debug_unregister_global_cb();
    s_test_app_debug.debug_active = false;
}

static void _test_app_debug_back_to_test_cb(lv_event_t *e)
{
    lv_obj_t *list_screen = s_test_app_debug.list_screen;

    _test_app_debug_destroy_bar();
    _test_app_debug_clear_current_app_id();
    _test_app_debug_unregister_global_cb();
    s_test_app_debug.debug_active = false;

    eos_activity_back();

    if (list_screen && lv_obj_is_valid(list_screen))
    {
        lv_obj_delete_async(list_screen);
    }
}

static void _test_app_debugger(void)
{
    if (s_test_app_debug.list_screen && lv_obj_is_valid(s_test_app_debug.list_screen))
    {
        lv_obj_delete_async(s_test_app_debug.list_screen);
        s_test_app_debug.list_screen = NULL;
    }

    s_test_app_debug.launcher_screen = eos_view_active();

    eos_activity_t *activity = eos_activity_create(&s_test_activity_lifecycle);
    if (!activity)
    {
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return;
    }

    s_test_app_debug.list_screen = view;

    eos_activity_set_view(activity, view);
    eos_activity_set_title(activity, "App Debugger");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    lv_obj_t *scr = view;
    lv_obj_add_event_cb(scr, _test_app_debug_list_delete_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_layout(scr, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(scr, 18, 0);
    lv_obj_set_style_pad_row(scr, 10, 0);

    lv_obj_t *toolbar = lv_obj_create(scr);
    lv_obj_set_size(toolbar, lv_pct(100), 96);
    lv_obj_set_style_border_width(toolbar, 0, 0);
    lv_obj_set_style_bg_opa(toolbar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(toolbar, 0, 0);

    lv_obj_t *back_btn = lv_button_create(toolbar);
    lv_obj_set_size(back_btn, 68, 68);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back_btn, _test_app_debug_back_to_test_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(toolbar);
    lv_label_set_text(title, "App Debugger");
    eos_label_set_font_size(title, EOS_FONT_SIZE_LARGE);
    lv_obj_align(title, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *app_list = lv_list_create(scr);
    lv_obj_set_width(app_list, lv_pct(100));
    lv_obj_set_flex_grow(app_list, 1);
    eos_event_subscribe_ex(EOS_EVENT_APP_INSTALLED, _test_app_debug_app_installed_cb, app_list, NULL);

    size_t app_list_size = eos_app_get_installed();
    for (size_t i = 0; i < app_list_size; i++)
    {
        _test_app_debug_app_btn_create(app_list, eos_app_list_get_id(i));
    }

    eos_crown_encoder_set_target_obj(app_list);
    eos_activity_enter(activity);
}

lv_obj_t *_create_new_scr()
{
    eos_activity_t *activity = eos_activity_create(&s_test_activity_lifecycle);
    if (!activity)
    {
        return NULL;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return NULL;
    }

    eos_activity_set_view(activity, view);
    eos_activity_set_title(activity, "ElenixOS Test");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    eos_activity_enter(activity);
    return view;
}

static void _test_msg_list_cb(lv_event_t *e)
{
    eos_msg_list_t *msg_list = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(msg_list);
    char *message = "Sab1e: No one's born being good at all things."
                    "You become good at things through hard work. "
                    "You're not a varsity athlete the first time "
                    "you play a new sport.";

    // Add a new message item
    eos_msg_list_item_t *item = eos_msg_list_item_create(msg_list);
    // Set the content
    eos_msg_list_item_set_title(item, "Settings");
    eos_msg_list_item_set_msg(item, message);
    eos_msg_list_item_set_time(item, "12:30");

    eos_msg_list_item_icon_set_src(item, EOS_IMG_SETTINGS);

    eos_msg_list_item_t *item1 = eos_msg_list_item_create(msg_list);
    eos_msg_list_item_set_title(item1, "QQ");
    eos_msg_list_item_set_msg(item1, message);
    eos_msg_list_item_set_time(item1, "21:00");
}

static void _test_msg_list(lv_event_t *e)
{
    _create_new_scr();
    eos_msg_list_t *msg_list = eos_msg_list_get_instance();
    EOS_CHECK_PTR_RETURN(msg_list);
    lv_obj_t *btn = lv_button_create(eos_view_active());
    lv_obj_center(btn);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, RI_CHAT_FOLLOW_UP_FILL " Add new message");
    lv_obj_add_event_cb(btn, _test_msg_list_cb, LV_EVENT_CLICKED, msg_list);
}

static void _nav_init_global_cb(lv_event_t *e)
{
    (*(int32_t *)lv_event_get_user_data(e)) = 0;
}

static void _back_prev_global_cb(lv_event_t *e)
{
    (*(int32_t *)lv_event_get_user_data(e))--;
}

static void _test_nav_cb_1(lv_event_t *e)
{
    static int32_t nav_counter = 0;

    eos_activity_t *activity = eos_activity_create(&s_test_activity_lifecycle);
    if (!activity)
    {
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return;
    }

    char title_str[64];
    snprintf(title_str, sizeof(title_str), "Screen %d", nav_counter);
    EOS_LOG_D("%s", title_str);

    eos_activity_set_view(activity, view);
    eos_activity_set_title(activity, title_str);
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    lv_obj_t *new_scr_btn = lv_button_create(view);
    lv_obj_t *label = lv_label_create(new_scr_btn);
    lv_label_set_text(label, "Create new scr");
    lv_obj_center(label);
    lv_obj_add_event_cb(new_scr_btn, _test_nav_cb_1, LV_EVENT_CLICKED, NULL);

#if 0
    uint32_t w = 250, h = 80;
    lv_obj_set_size(new_scr_btn, w, h);
    int32_t x = rand() % (EOS_DISPLAY_WIDTH - w);
    int32_t y = rand() % (EOS_DISPLAY_HEIGHT - h);
    if (y < 130)
        y += 130;
    lv_obj_set_pos(new_scr_btn, x, y);
#else
    lv_obj_center(new_scr_btn);
#endif

    // Add back button
    lv_obj_t *back_btn = lv_button_create(view);
    lv_obj_set_size(back_btn, 68, 68);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, eos_activity_back_cb, LV_EVENT_CLICKED, NULL);

    nav_counter++;
    eos_activity_enter(activity);
}

static void _test_font(lv_event_t *e)
{
    _create_new_scr();

    const char *test_str = /* Chinese symbols test */
        "，。、：；？！“”‘’（）【】《》〈〉——……·＋－×÷＝≠＞＜≥≤≈±￥％‰℃°＠＃＆☆★●○■□▲△▼▽"
        /* English symbols test */ "~!@#$%^&*()-_=+[]{}\\|;:'\",./<>?`©®™"
        /* Greek alphabet test */ "ΑαΒβΓγΔδΕεΖζΗηΘθΙιΚκΛλΜμΝνΞξΟοΠπΡρΣσΤτΥυΦφΧχΨψΩω"
        /* English digits test */ "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"
        /* Common Chinese chars test */ "在夏末的午后，风把阳台上的风铃吹得叮当作响，像是某种不经意的暗号。"
        /* Rare Chinese chars test */ "霡霂淅沥，薜荔葳蕤。彳亍踟蹰，睥睨娉婷。觊觎饕餮，倥偬倜傥。菡萏猗傩，蘼芜菁菁"
        "。";

    lv_obj_t *container = eos_list_create(eos_view_active());
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_t *font_label = lv_label_create(container);
    lv_label_set_text(font_label, test_str);
    lv_obj_set_width(font_label, lv_pct(100));
    lv_label_set_long_mode(font_label, LV_LABEL_LONG_WRAP);
#ifdef TEST_USE_ZH_FONT
    lv_obj_set_style_text_font(font_label, &eos_font_resource_han_rounded_30, LV_PART_MAIN);
#endif
}

static void _test_lang_cb(lv_event_t *e)
{
    if (eos_lang_get_current_id() == LANG_ZH)
    {
        eos_lang_set_current_id(LANG_EN);
    }
    else
    {
        eos_lang_set_current_id(LANG_ZH);
    }
}

static void _test_lang(lv_event_t *e)
{
    _create_new_scr();

    lv_obj_t *label = lv_label_create(eos_view_active());
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_center(label);
#ifdef TEST_USE_ZH_FONT
    lv_obj_set_style_text_font(label, &eos_font_resource_han_rounded_30, LV_PART_MAIN);
#endif
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_t *btn = lv_button_create(eos_view_active());
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, RI_REPEAT_2_FILL " Switch Language");
    lv_obj_add_event_cb(btn, _test_lang_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void _test_image_input_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *kb = lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED)
    {
        // Click the text box to display the keyboard
        lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
    else if (code == LV_EVENT_READY || code == LV_EVENT_DEFOCUSED)
    {
        // Process when the confirm key is pressed or the focus is lost
        const char *path = lv_textarea_get_text(ta);

        if (strlen(path) > 0)
        {
            // Set image source
            lv_image_set_src(img, path);

            // Clear text box content
            lv_textarea_set_text(ta, "");
        }

        // Hide keyboard
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void _test_image(lv_event_t *e)
{
    lv_obj_t *scr = _create_new_scr();
    // Create image object without setting source
    img = lv_image_create(scr);
    lv_obj_center(img);
    lv_obj_move_background(img);

    // Create text area
    ta = lv_textarea_create(scr);
    lv_obj_set_size(ta, LV_HOR_RES - 40, 80);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 20);
    lv_textarea_set_placeholder_text(ta, "Input image path here.(e.g. /flower.bin)");
    lv_textarea_set_one_line(ta, true);

    // Add keyboard
    lv_obj_t *kb = lv_keyboard_create(scr);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN); // Hide keyboard by default

    // Add event callback
    lv_obj_add_event_cb(ta, _test_image_input_cb, LV_EVENT_ALL, kb);
}

static void _test_app_list(lv_event_t *e)
{
    eos_app_list_enter();
}

static void _test_app_debug_page(lv_event_t *e)
{
    _test_app_debugger();
}

static void _test_watchface_list(lv_event_t *e)
{
    eos_watchface_list_enter();
}

static void _toast_clicked_cb(lv_event_t *e)
{
    eos_app_list_enter();
}

static void _test_toast(lv_event_t *e)
{
    lv_obj_t *toast = eos_toast_show(NULL, "Click me to open App List!");
    lv_obj_add_event_cb(toast, _toast_clicked_cb, LV_EVENT_CLICKED, NULL);
}

static void _test_panel_basic(lv_event_t *e)
{
    lv_obj_t *view = _create_new_scr();
    if (!view)
    {
        EOS_LOG_E("Failed to create view");
        return;
    }

    lv_obj_remove_style_all(view);
    lv_obj_add_style(view, eos_theme_get_view_style(), 0);

    eos_panel_cfg_t cfg = {
        .icon_bg_color = EOS_COLOR_BLUE,
        .icon_type = EOS_PANEL_ICON_TYPE_SYMBOL,
        .icon_src = RI_INFORMATION_LINE,
        .title_text = "Panel Title",
        .message_text = "This is a basic panel test message",
        .confirm_btn_id = STR_ID_OK,
        .confirm_btn_text = NULL,
        .confirm_cb = NULL,
        .cancel_btn_id = STR_ID_BACK,
        .cancel_btn_text = NULL,
        .cancel_cb = NULL,
    };

    eos_panel_t *panel = eos_panel_create(view, &cfg);
    if (panel)
    {
        EOS_LOG_I("Basic panel created successfully");
    }
}

static void _test_panel_with_extra_slot(lv_event_t *e)
{
    lv_obj_t *view = _create_new_scr();
    if (!view)
    {
        EOS_LOG_E("Failed to create view");
        return;
    }

    lv_obj_remove_style_all(view);
    lv_obj_add_style(view, eos_theme_get_view_style(), 0);

    eos_panel_cfg_t cfg = {
        .icon_bg_color = EOS_COLOR_GREEN,
        .icon_type = EOS_PANEL_ICON_TYPE_SYMBOL,
        .icon_src = RI_CHECK_FILL,
        .title_text = "Extra Slot Test",
        .message_text = "Adding custom content to extra slot",
        .confirm_btn_id = 0,
        .confirm_btn_text = NULL,
        .confirm_cb = NULL,
        .cancel_btn_id = STR_ID_BACK,
        .cancel_btn_text = NULL,
        .cancel_cb = NULL,
    };

    eos_panel_t *panel = eos_panel_create(view, &cfg);
    if (panel)
    {
        lv_obj_t *extra_slot = eos_panel_get_extra_slot(panel);
        if (extra_slot)
        {
            lv_obj_t *switch_label = lv_label_create(extra_slot);
            lv_label_set_text(switch_label, "Option 1: Sample Switch");
            lv_obj_set_width(switch_label, EOS_PANEL_CONTENT_WIDTH);

            lv_obj_t *switch_btn = lv_switch_create(extra_slot);
            lv_obj_set_width(switch_btn, 50);
        }
        EOS_LOG_I("Panel with extra slot created successfully");
    }
}

static void _test_fault_panel_bug(lv_event_t *e)
{
    lv_obj_t *view = _create_new_scr();
    if (!view)
    {
        EOS_LOG_E("Failed to create view");
        return;
    }

    eos_fault_cfg_t cfg = {
        .icon_type = EOS_FAULT_ICON_BUG,
        .custom_icon = NULL,
        .title_id = STR_ID_ERROR,
        .title_text = NULL,
        .message_id = 0,
        .message_text = "A runtime exception occurred",
        .confirm_btn_id = 0,
        .confirm_btn_text = NULL,
        .confirm_cb = NULL,
        .cancel_btn_id = STR_ID_BACK,
        .cancel_btn_text = NULL,
        .cancel_cb = NULL,
        .icon_color = EOS_COLOR_RED,
    };

    eos_fault_panel_t *fault_panel = eos_fault_panel_create_on_activity(NULL, &cfg);
    if (fault_panel)
    {
        EOS_LOG_I("Bug fault panel created successfully");
    }
}

static void _test_fault_panel_warning(lv_event_t *e)
{
    lv_obj_t *view = _create_new_scr();
    if (!view)
    {
        EOS_LOG_E("Failed to create view");
        return;
    }

    eos_fault_cfg_t cfg = {
        .icon_type = EOS_FAULT_ICON_WARNING,
        .custom_icon = NULL,
        .title_id = 0,
        .title_text = "Warning",
        .message_id = 0,
        .message_text = "This is a warning message",
        .confirm_btn_id = STR_ID_OK,
        .confirm_btn_text = NULL,
        .confirm_cb = NULL,
        .cancel_btn_id = STR_ID_CANCEL,
        .cancel_btn_text = NULL,
        .cancel_cb = NULL,
        .icon_color = EOS_COLOR_ORANGE,
    };

    eos_fault_panel_t *fault_panel = eos_fault_panel_create_on_activity(NULL, &cfg);
    if (fault_panel)
    {
        EOS_LOG_I("Warning fault panel created successfully");
    }
}

static void _test_fault_panel_info(lv_event_t *e)
{
    lv_obj_t *view = _create_new_scr();
    if (!view)
    {
        EOS_LOG_E("Failed to create view");
        return;
    }

    eos_fault_cfg_t cfg = {
        .icon_type = EOS_FAULT_ICON_INFO,
        .custom_icon = NULL,
        .title_id = 0,
        .title_text = "Information",
        .message_id = 0,
        .message_text = "This is an information message",
        .confirm_btn_id = 0,
        .confirm_btn_text = NULL,
        .confirm_cb = NULL,
        .cancel_btn_id = STR_ID_OK,
        .cancel_btn_text = NULL,
        .cancel_cb = NULL,
        .icon_color = EOS_COLOR_BLUE,
    };

    eos_fault_panel_t *fault_panel = eos_fault_panel_create_on_activity(NULL, &cfg);
    if (fault_panel)
    {
        EOS_LOG_I("Info fault panel created successfully");
    }
}

static void _test_panel_no_icon(lv_event_t *e)
{
    lv_obj_t *view = _create_new_scr();
    if (!view)
    {
        EOS_LOG_E("Failed to create view");
        return;
    }

    lv_obj_remove_style_all(view);
    lv_obj_add_style(view, eos_theme_get_view_style(), 0);

    eos_panel_cfg_t cfg = {
        .icon_bg_color = EOS_COLOR_RED,
        .icon_type = EOS_PANEL_ICON_TYPE_NONE,
        .icon_src = NULL,
        .title_text = "No Icon Panel",
        .message_text = "This panel has no icon",
        .confirm_btn_id = STR_ID_OK,
        .confirm_btn_text = NULL,
        .confirm_cb = NULL,
        .cancel_btn_id = STR_ID_BACK,
        .cancel_btn_text = NULL,
        .cancel_cb = NULL,
    };

    eos_panel_t *panel = eos_panel_create(view, &cfg);
    if (panel)
    {
        EOS_LOG_I("No icon panel created successfully");
    }
}

static void _test_panel_list(lv_event_t *e)
{
    eos_activity_t *activity = eos_activity_get_current();
    if (!activity)
    {
        EOS_LOG_E("No current activity");
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return;
    }

    lv_obj_t *list = lv_list_create(view);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    eos_crown_encoder_set_target_obj(list);

    lv_obj_t *btn;
    lv_obj_t *label = lv_list_add_text(list, RI_WINDOW_LINE " Panel Test List");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    btn = lv_list_add_button(list, RI_WINDOW_LINE, "Basic Panel");
    lv_obj_add_event_cb(btn, _test_panel_basic, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(list, RI_WINDOW_LINE, "Extra Slot");
    lv_obj_add_event_cb(btn, _test_panel_with_extra_slot, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(list, RI_WINDOW_LINE, "Fault Bug");
    lv_obj_add_event_cb(btn, _test_fault_panel_bug, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(list, RI_WINDOW_LINE, "Fault Warning");
    lv_obj_add_event_cb(btn, _test_fault_panel_warning, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(list, RI_WINDOW_LINE, "Fault Info");
    lv_obj_add_event_cb(btn, _test_fault_panel_info, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(list, RI_WINDOW_LINE, "No Icon");
    lv_obj_add_event_cb(btn, _test_panel_no_icon, LV_EVENT_CLICKED, NULL);

    eos_activity_enter(activity);
}

static const char *_test_audio_resolve_path(void)
{
    if (eos_storage_is_file(s_test_audio_primary_path))
    {
        return s_test_audio_primary_path;
    }

    if (eos_storage_is_file(s_test_audio_fallback_path))
    {
        return s_test_audio_fallback_path;
    }

    return s_test_audio_primary_path;
}

static void _test_audio_sync_ui(void)
{
    test_audio_page_ctx_t *ctx = &s_test_audio_page;
    if (!(ctx->status_label && lv_obj_is_valid(ctx->status_label)))
        return;

    bool spk_avail = eos_service_audio_speaker_available();
    bool mic_avail = eos_service_audio_microphone_available();
    eos_dev_state_t state = eos_dev_speaker_get_state();
    const char *path = _test_audio_resolve_path();

    const char *status_text;
    switch (state)
    {
        case DEV_STATE_BUSY:
            status_text = "Audio status: playing";
            break;
        case DEV_STATE_READY:
            status_text = "Audio status: ready";
            break;
        default:
            status_text = "Audio status: idle";
            break;
    }

    lv_label_set_text_fmt(ctx->status_label,
                          "%s\nSpeaker: %s  Mic: %s\nFile: %s",
                          status_text,
                          spk_avail ? "yes" : "no",
                          mic_avail ? "yes" : "no",
                          path);

    eos_audio_player_t *player = eos_service_audio_get_player();

    if (ctx->play_btn_label && lv_obj_is_valid(ctx->play_btn_label))
    {
        eos_audio_player_state_t p_state = eos_audio_player_get_state(player);
        uint32_t check_dur = eos_audio_player_get_duration(player);

        /* Defensive: if state claims PLAYING but no decoder is open,
         * treat as IDLE (stale state from incomplete teardown). */
        if (p_state == EOS_AUDIO_PLAYING && check_dur == 0)
        {
            p_state = EOS_AUDIO_IDLE;
        }

        lv_label_set_text(ctx->play_btn_label,
                          (p_state == EOS_AUDIO_PLAYING) ? (LV_SYMBOL_PAUSE " Pause") : (LV_SYMBOL_PLAY " Play"));
    }

    uint32_t pos = eos_audio_player_get_position(player);
    uint32_t dur = eos_audio_player_get_duration(player);
    uint32_t rate = eos_audio_player_get_sample_rate(player);

    if (ctx->progress_slider && lv_obj_is_valid(ctx->progress_slider))
    {
        if (dur > 0 && rate > 0)
        {
            lv_slider_set_range(ctx->progress_slider, 0, (int32_t)dur);
            lv_slider_set_value(ctx->progress_slider, (int32_t)pos, LV_ANIM_OFF);
            lv_obj_remove_flag(ctx->progress_slider, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(ctx->progress_slider, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ctx->time_label && lv_obj_is_valid(ctx->time_label))
    {
        if (dur > 0 && rate > 0)
        {
            uint32_t pos_sec = pos / rate;
            uint32_t dur_sec = dur / rate;
            lv_label_set_text_fmt(ctx->time_label,
                                  "%u:%02u / %u:%02u",
                                  pos_sec / 60,
                                  pos_sec % 60,
                                  dur_sec / 60,
                                  dur_sec % 60);
            lv_obj_remove_flag(ctx->time_label, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(ctx->time_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void _test_audio_state_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    _test_audio_sync_ui();
}

static void _test_audio_seek_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t sample = lv_slider_get_value(slider);
    eos_audio_player_t *player = eos_service_audio_get_player();
    eos_audio_player_seek(player, (uint32_t)sample);
}

static void _test_audio_volume_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    uint8_t vol = (uint8_t)lv_slider_get_value(slider);
    eos_service_audio_set_volume(vol);
    if (s_test_audio_page.volume_label && lv_obj_is_valid(s_test_audio_page.volume_label))
    {
        lv_label_set_text_fmt(s_test_audio_page.volume_label, "Volume: %u%%", vol);
    }
}

static void _test_audio_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    eos_audio_player_t *player = eos_service_audio_get_player();
    eos_audio_player_state_t p_state = eos_audio_player_get_state(player);

    if (p_state == EOS_AUDIO_PLAYING)
    {
        eos_service_audio_pause();
        eos_toast_show(NULL, "Paused");
    }
    else if (p_state == EOS_AUDIO_PAUSED)
    {
        eos_service_audio_resume();
        eos_toast_show(NULL, "Resumed");
    }
    else
    {
        if (!eos_service_audio_speaker_available())
        {
            eos_toast_show(NULL, "Speaker unavailable");
            return;
        }
        const char *path = _test_audio_resolve_path();
        if (!eos_storage_is_file(path))
        {
            eos_toast_show(NULL, "File not found");
            return;
        }
        eos_result_t ret = eos_service_audio_play(path);
        if (ret != EOS_OK)
            eos_toast_show(NULL, "Play failed");
        else
            eos_toast_show(NULL, "Playing...");
    }
    _test_audio_sync_ui();
}

static void _test_audio_stop_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    eos_service_audio_stop();
    _test_audio_sync_ui();
}

static void _test_audio_page_delete_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    eos_service_audio_stop();

    if (s_test_audio_page.state_timer)
    {
        lv_timer_delete(s_test_audio_page.state_timer);
    }
    memset(&s_test_audio_page, 0, sizeof(s_test_audio_page));
}

static void _test_audio_page(lv_event_t *e)
{
    lv_obj_t *scr = _create_new_scr();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    memset(&s_test_audio_page, 0, sizeof(s_test_audio_page));
    s_test_audio_page.screen = scr;

    lv_obj_t *list = eos_list_create(scr);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_style_pad_all(list, 16, 0);
    lv_obj_set_style_pad_row(list, 14, 0);

    lv_obj_t *title = lv_label_create(list);
    lv_label_set_text(title, LV_SYMBOL_AUDIO " Audio Playback");
    eos_label_set_font_size(title, EOS_FONT_SIZE_LARGE);

    lv_obj_t *hint = lv_label_create(list);
    lv_label_set_text(hint, "Play/Pause/Stop fs/music.mp3");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xA0A0A0), LV_PART_MAIN);

    s_test_audio_page.play_btn = lv_button_create(list);
    lv_obj_set_size(s_test_audio_page.play_btn, lv_pct(100), 56);
    lv_obj_add_event_cb(s_test_audio_page.play_btn, _test_audio_button_cb, LV_EVENT_CLICKED, NULL);

    s_test_audio_page.play_btn_label = lv_label_create(s_test_audio_page.play_btn);
    lv_label_set_text(s_test_audio_page.play_btn_label, LV_SYMBOL_PLAY " Play");
    lv_obj_center(s_test_audio_page.play_btn_label);

    s_test_audio_page.progress_slider = lv_slider_create(list);
    lv_obj_set_size(s_test_audio_page.progress_slider, lv_pct(100), 20);
    lv_slider_set_range(s_test_audio_page.progress_slider, 0, 100);
    lv_obj_set_style_pad_all(s_test_audio_page.progress_slider, 8, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s_test_audio_page.progress_slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_add_flag(s_test_audio_page.progress_slider, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_test_audio_page.progress_slider, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(s_test_audio_page.progress_slider, _test_audio_seek_cb, LV_EVENT_RELEASED, NULL);

    s_test_audio_page.time_label = lv_label_create(list);
    lv_obj_add_flag(s_test_audio_page.time_label, LV_OBJ_FLAG_HIDDEN);

    s_test_audio_page.volume_slider = lv_slider_create(list);
    lv_obj_set_size(s_test_audio_page.volume_slider, lv_pct(70), 20);
    lv_slider_set_range(s_test_audio_page.volume_slider, 0, 100);
    lv_slider_set_value(s_test_audio_page.volume_slider, eos_service_audio_get_volume(), LV_ANIM_OFF);
    lv_obj_add_event_cb(s_test_audio_page.volume_slider, _test_audio_volume_cb, LV_EVENT_RELEASED, NULL);

    s_test_audio_page.volume_label = lv_label_create(list);
    lv_label_set_text_fmt(s_test_audio_page.volume_label, "Volume: %u%%", eos_service_audio_get_volume());

    s_test_audio_page.stop_btn = lv_button_create(list);
    lv_obj_set_size(s_test_audio_page.stop_btn, lv_pct(100), 40);
    lv_obj_set_style_bg_color(s_test_audio_page.stop_btn, lv_color_hex(0xD84F4F), LV_PART_MAIN);
    lv_obj_add_event_cb(s_test_audio_page.stop_btn, _test_audio_stop_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *stop_label = lv_label_create(s_test_audio_page.stop_btn);
    lv_label_set_text(stop_label, LV_SYMBOL_STOP " Stop");
    lv_obj_set_style_text_color(stop_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(stop_label);

    s_test_audio_page.status_label = lv_label_create(list);
    lv_obj_set_width(s_test_audio_page.status_label, lv_pct(100));
    lv_label_set_long_mode(s_test_audio_page.status_label, LV_LABEL_LONG_WRAP);

    lv_obj_add_event_cb(scr, _test_audio_page_delete_cb, LV_EVENT_DELETE, NULL);
    s_test_audio_page.state_timer = lv_timer_create(_test_audio_state_timer_cb, 200, NULL);
    _test_audio_sync_ui();
}

#define RECORDING_PATH "/.sys/recording.wav"

static void _test_recording_sync_ui(void)
{
    test_recording_page_ctx_t *ctx = &s_test_recording_page;
    if (!(ctx->status_label && lv_obj_is_valid(ctx->status_label)))
        return;

    bool mic_avail = eos_service_audio_microphone_available();
    const char *status = ctx->is_recording ? "Recording..." : "Idle";

    lv_label_set_text_fmt(ctx->status_label, "%s\nMic: %s\nFile: %s", status, mic_avail ? "yes" : "no", RECORDING_PATH);

    if (ctx->record_btn_label && lv_obj_is_valid(ctx->record_btn_label))
    {
        lv_label_set_text(ctx->record_btn_label,
                          ctx->is_recording ? (LV_SYMBOL_STOP " Stop") : (RI_RECORD_CIRCLE_FILL " Record"));
    }
}

static void _test_recording_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    _test_recording_sync_ui();
}

static void _test_recording_record_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    test_recording_page_ctx_t *ctx = &s_test_recording_page;

    if (!eos_service_audio_microphone_available())
    {
        eos_toast_show(NULL, "Microphone unavailable");
        return;
    }

    if (ctx->is_recording)
    {
        eos_service_audio_stop_recording();
        ctx->is_recording = false;
        eos_toast_show(NULL, "Recording stopped");
    }
    else
    {
        eos_result_t ret = eos_service_audio_start_recording(RECORDING_PATH);
        if (ret != EOS_OK)
        {
            eos_toast_show(NULL, "Start recording failed");
        }
        else
        {
            ctx->is_recording = true;
            eos_toast_show(NULL, "Recording...");
        }
    }
    _test_recording_sync_ui();
}

static void _test_recording_playback_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    if (s_test_recording_page.is_recording)
    {
        eos_toast_show(NULL, "Stop recording first");
        return;
    }

    if (!eos_storage_is_file(RECORDING_PATH))
    {
        eos_toast_show(NULL, "No recording yet");
        return;
    }

    eos_result_t ret = eos_service_audio_play(RECORDING_PATH);
    if (ret != EOS_OK)
    {
        eos_toast_show(NULL, "Playback failed");
    }
}

static void _test_recording_page_delete_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    test_recording_page_ctx_t *ctx = &s_test_recording_page;

    if (ctx->is_recording)
    {
        eos_service_audio_stop_recording();
        ctx->is_recording = false;
    }
    if (ctx->state_timer)
    {
        lv_timer_delete(ctx->state_timer);
    }
    memset(ctx, 0, sizeof(*ctx));
}

static void _test_recording_page(lv_event_t *e)
{
    lv_obj_t *scr = _create_new_scr();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    memset(&s_test_recording_page, 0, sizeof(s_test_recording_page));
    s_test_recording_page.screen = scr;

    lv_obj_t *list = eos_list_create(scr);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_style_pad_all(list, 16, 0);
    lv_obj_set_style_pad_row(list, 14, 0);

    lv_obj_t *title = lv_label_create(list);
    lv_label_set_text(title, LV_SYMBOL_AUDIO " Recording");
    eos_label_set_font_size(title, EOS_FONT_SIZE_LARGE);

    lv_obj_t *hint = lv_label_create(list);
    lv_label_set_text(hint, "Tap Record to start/stop, Play to listen");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xA0A0A0), LV_PART_MAIN);

    /* Record button */
    s_test_recording_page.record_btn = lv_button_create(list);
    lv_obj_set_size(s_test_recording_page.record_btn, lv_pct(100), 56);
    lv_obj_add_event_cb(s_test_recording_page.record_btn, _test_recording_record_btn_cb, LV_EVENT_CLICKED, NULL);

    s_test_recording_page.record_btn_label = lv_label_create(s_test_recording_page.record_btn);
    lv_label_set_text(s_test_recording_page.record_btn_label, RI_RECORD_CIRCLE_FILL " Record");
    lv_obj_center(s_test_recording_page.record_btn_label);

    /* Playback button */
    s_test_recording_page.playback_btn = lv_button_create(list);
    lv_obj_set_size(s_test_recording_page.playback_btn, lv_pct(100), 56);
    lv_obj_add_event_cb(s_test_recording_page.playback_btn, _test_recording_playback_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(s_test_recording_page.playback_btn, lv_color_hex(0x2F80ED), LV_PART_MAIN);

    lv_obj_t *play_label = lv_label_create(s_test_recording_page.playback_btn);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY " Play Recording");
    lv_obj_center(play_label);

    /* Status label */
    s_test_recording_page.status_label = lv_label_create(list);
    lv_obj_set_width(s_test_recording_page.status_label, lv_pct(100));
    lv_label_set_long_mode(s_test_recording_page.status_label, LV_LABEL_LONG_WRAP);

    lv_obj_add_event_cb(scr, _test_recording_page_delete_cb, LV_EVENT_DELETE, NULL);
    s_test_recording_page.state_timer = lv_timer_create(_test_recording_timer_cb, 300, NULL);
    _test_recording_sync_ui();
}

static void _test_show_all_lv_symbols_list(lv_event_t *e)
{
    lv_obj_t *scr = _create_new_scr();

    // Create list
    lv_obj_t *list = lv_list_create(scr);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(list, 5, 0);

    for (size_t i = 0; i < sizeof(lv_symbols) / sizeof(lv_symbols[0]); i++)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s  U+%04X", lv_symbols[i].symbol, lv_symbols[i].codepoint);

        // Add item to the list
        lv_obj_t *label = lv_list_add_text(list, buf);
        // lv_obj_set_style_text_font(label, &lv_font_montserrat_30, 0); // Set font
    }
}

static void _slide_widget_reached_threshold_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    eos_slide_widget_t *sw = (eos_slide_widget_t *)lv_event_get_user_data(e);
    eos_slide_widget_delete(sw);
}

static void _slide_widget_moving_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
    lv_label_set_text_fmt(label, "(%d,%d)", lv_obj_get_x(obj), lv_obj_get_y(obj));
}

static void _slide_widget_reset_btn_clicked_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *target = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_set_pos(target, 0, 160);
}

static lv_obj_t *_add_slide_wdiget(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_button_create(parent);

    lv_obj_set_size(obj, EOS_DISPLAY_WIDTH - 100, 100);
    lv_obj_set_style_margin_ver(obj, 10, 0);
    lv_obj_update_layout(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    eos_slide_widget_t *sw =
        eos_slide_widget_create_with_touch(obj, obj, EOS_SLIDE_DIR_HOR, EOS_DISPLAY_WIDTH, EOS_THRESHOLD_30);
    eos_slide_widget_set_bidirectional(sw, true);
    eos_slide_widget_add_event_cb_reached_threshold(sw, _slide_widget_reached_threshold_cb, sw);

    return obj;
}

static void _test_slide_widget(lv_event_t *e)
{
    lv_obj_t *scr = _create_new_scr();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *list = lv_list_create(scr);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(88));
    lv_obj_center(list);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 30, 0);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_user_data(list, list);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *obj = _add_slide_wdiget(list);
    obj = _add_slide_wdiget(list);
    obj = _add_slide_wdiget(list);
    obj = _add_slide_wdiget(list);
    // lv_obj_t *reset_btn = lv_button_create(scr);
    // lv_obj_t *label = lv_label_create(reset_btn);
    // lv_obj_align(reset_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    // lv_obj_add_event_cb(reset_btn, _slide_widget_reset_btn_clicked_cb, LV_EVENT_CLICKED, obj);
    // eos_slide_widget_add_event_cb_moving(sw, _slide_widget_moving_cb, label);
}

static void _test_font_size(lv_event_t *e)
{
    lv_obj_t *scr = _create_new_scr();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *list = eos_list_create(scr);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));

    static const char *test_text = "AaBbCc 你好中国 1234567890";

    lv_obj_t *label = lv_label_create(list);
    lv_label_set_text(label, test_text);
    eos_label_set_font_size(label, EOS_FONT_SIZE_SMALL);

    label = lv_label_create(list);
    lv_label_set_text(label, test_text);
    eos_label_set_font_size(label, EOS_FONT_SIZE_MEDIUM);

    label = lv_label_create(list);
    lv_label_set_text(label, test_text);
    eos_label_set_font_size(label, EOS_FONT_SIZE_LARGE);
}

typedef struct
{
    lv_obj_t *table;
    eos_dev_sensor_t *sensors[EOS_SENSOR_TYPE_MAX];
    uint8_t sensor_count;
    lv_timer_t *timer;
} _sensor_test_data_t;

static _sensor_test_data_t sensor_data;

static void _sensor_update_table(_sensor_test_data_t *data)
{
    if (!data || !data->table)
    {
        return;
    }

    const uint8_t _SENSOR_VAL_COL = 1;

    for (uint8_t i = 0; i < data->sensor_count; i++)
    {
        eos_dev_sensor_t *dev = data->sensors[i];
        if (!dev)
        {
            lv_table_set_cell_value_fmt(data->table, i + 1, _SENSOR_VAL_COL, "N/A");
            continue;
        }

        eos_sensor_raw_data_t raw_data;
        eos_result_t result = eos_sensor_read_latest(dev->type, &raw_data);

        if (result != EOS_OK)
        {
            lv_table_set_cell_value_fmt(data->table, i + 1, _SENSOR_VAL_COL, "N/A");
            continue;
        }

        switch (dev->type)
        {
            case EOS_SENSOR_TYPE_ACCE:
                lv_table_set_cell_value_fmt(data->table,
                                            i + 1,
                                            _SENSOR_VAL_COL,
                                            "X=%d Y=%d Z=%d",
                                            raw_data.data.acce.x,
                                            raw_data.data.acce.y,
                                            raw_data.data.acce.z);
                break;

            case EOS_SENSOR_TYPE_GYRO:
                lv_table_set_cell_value_fmt(data->table,
                                            i + 1,
                                            _SENSOR_VAL_COL,
                                            "X=%d Y=%d Z=%d",
                                            raw_data.data.gyro.x,
                                            raw_data.data.gyro.y,
                                            raw_data.data.gyro.z);
                break;

            case EOS_SENSOR_TYPE_MAG:
                lv_table_set_cell_value_fmt(data->table,
                                            i + 1,
                                            _SENSOR_VAL_COL,
                                            "X=%d Y=%d Z=%d",
                                            raw_data.data.mag.x,
                                            raw_data.data.mag.y,
                                            raw_data.data.mag.z);
                break;

            case EOS_SENSOR_TYPE_TEMP:
                lv_table_set_cell_value_fmt(data->table,
                                            i + 1,
                                            _SENSOR_VAL_COL,
                                            "%.2f C",
                                            raw_data.data.temp.temp / 100.0f);
                break;

            case EOS_SENSOR_TYPE_BARO:
                lv_table_set_cell_value_fmt(data->table,
                                            i + 1,
                                            _SENSOR_VAL_COL,
                                            "%.2f hPa",
                                            raw_data.data.baro.pressure / 100.0f);
                break;

            case EOS_SENSOR_TYPE_LIGHT:
                lv_table_set_cell_value_fmt(data->table,
                                            i + 1,
                                            _SENSOR_VAL_COL,
                                            "%u lux",
                                            (unsigned int)raw_data.data.light.lux);
                break;

            case EOS_SENSOR_TYPE_PROXIMITY:
                lv_table_set_cell_value_fmt(data->table,
                                            i + 1,
                                            _SENSOR_VAL_COL,
                                            "%u mm",
                                            raw_data.data.proximity.distance_mm);
                break;

            case EOS_SENSOR_TYPE_HR:
                lv_table_set_cell_value_fmt(data->table,
                                            i + 1,
                                            _SENSOR_VAL_COL,
                                            "HR=%u bpm",
                                            raw_data.data.hr.heart_rate);
                break;

            case EOS_SENSOR_TYPE_SPO2:
                lv_table_set_cell_value_fmt(data->table, i + 1, _SENSOR_VAL_COL, "SpO2=%u%%", raw_data.data.spo2.spo2);
                break;

            case EOS_SENSOR_TYPE_STEP:
                lv_table_set_cell_value_fmt(data->table,
                                            i + 1,
                                            _SENSOR_VAL_COL,
                                            "%lu steps",
                                            (unsigned long)raw_data.data.step.steps);
                break;

            default:
                lv_table_set_cell_value_fmt(data->table, i + 1, _SENSOR_VAL_COL, "-");
                break;
        }
    }
}

static void _sensor_timer_cb(lv_timer_t *t)
{
    _sensor_test_data_t *data = (_sensor_test_data_t *)lv_timer_get_user_data(t);
    if (data && data->table && data->timer == t)
    {
        _sensor_update_table(data);
    }
}

static void _sensor_cleanup(_sensor_test_data_t *data)
{
    if (data)
    {
        if (data->timer)
        {
            lv_timer_delete(data->timer);
            data->timer = NULL;
        }
        /* Disable all sensors that were enabled by the tester */
        for (uint8_t i = 0; i < data->sensor_count; i++)
        {
            eos_dev_sensor_t *dev = data->sensors[i];
            if (dev)
                eos_sensor_set_sample_period(dev->type, 0);
        }
        data->table = NULL;
        data->sensor_count = 0;
    }
}

static void _sensor_test_on_destroy(eos_activity_t *activity)
{
    LV_UNUSED(activity);
    _sensor_cleanup(&sensor_data);
}

static const eos_activity_lifecycle_t _s_sensor_test_lifecycle = {.on_enter = NULL,
                                                                  .on_destroy = _sensor_test_on_destroy,
                                                                  .on_pause = NULL,
                                                                  .on_resume = NULL};

static lv_obj_t *_create_sensor_test_scr(void)
{
    eos_activity_t *activity = eos_activity_create(&_s_sensor_test_lifecycle);
    if (!activity)
    {
        return NULL;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return NULL;
    }

    eos_activity_set_title(activity, "Sensor Tester");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    eos_activity_enter(activity);
    return view;
}

static void _test_sensor(lv_event_t *e)
{
    lv_obj_t *scr = _create_sensor_test_scr();
    if (!scr)
    {
        return;
    }

    /* Initialize sensor service if not already done */
    eos_result_t svc_result = eos_service_sensor_init();
    (void)svc_result;

    lv_obj_t *list = eos_list_create(scr);

    lv_obj_t *tb = lv_table_create(list);
    lv_table_set_row_count(tb, EOS_SENSOR_TYPE_MAX + 1);
    lv_table_set_column_count(tb, 2);
    lv_obj_set_width(tb, lv_pct(100));
    lv_table_set_cell_value(tb, 0, 0, "Sensor");
    lv_table_set_cell_value(tb, 0, 1, "Value");
    lv_table_set_column_width(tb, 0, 180);
    lv_table_set_column_width(tb, 1, 180);

    _sensor_cleanup(&sensor_data);

    sensor_data.table = tb;
    sensor_data.sensor_count = 0;

    /* Track which types have already been enabled to avoid duplicate calls */
    bool type_enabled[EOS_SENSOR_TYPE_MAX] = {false};

    /* Iterate through all registered sensor devices */
    eos_dev_sensor_t *dev = eos_dev_sensor_get_list_head();
    while (dev && sensor_data.sensor_count < EOS_SENSOR_TYPE_MAX)
    {
        if (dev->name)
        {
            sensor_data.sensors[sensor_data.sensor_count++] = dev;
            lv_table_set_cell_value(tb, sensor_data.sensor_count, 0, dev->name);
            lv_table_set_cell_value(tb, sensor_data.sensor_count, 1, "N/A");

            /* Enable the sensor type (100ms = 10Hz) */
            if (dev->type > EOS_SENSOR_TYPE_UNKNOWN && dev->type < EOS_SENSOR_TYPE_MAX && !type_enabled[dev->type])
            {
                eos_sensor_set_sample_period(dev->type, 100);
                type_enabled[dev->type] = true;
            }
        }
        dev = dev->_next;
    }

    if (sensor_data.sensor_count > 0)
    {
        lv_table_set_row_count(tb, sensor_data.sensor_count + 1);
    }

    sensor_data.timer = lv_timer_create(_sensor_timer_cb, 100, &sensor_data);
}

static void _test_sensor_chart_cb(lv_event_t *e)
{
    (void)e;
    eos_test_sensor_chart_start();
}

static void _test_sensor_multi_chart_cb(lv_event_t *e)
{
    (void)e;
    eos_test_sensor_multi_chart_start();
}

static void _test_input_page_cb(lv_event_t *e)
{
    (void)e;
    eos_test_input_page_start();
}

static void _test_battery_history_cb(lv_event_t *e)
{
    (void)e;
    eos_test_battery_history_start();
}

static void _test_package_cb(lv_event_t *e)
{
    (void)e;
    eos_test_package_start();
}

static void _test_unit_runner_cb(lv_event_t *e)
{
    (void)e;
    eos_test_runner_start();
}

void eos_test_start(void)
{
    eos_activity_t *activity = eos_activity_create(&s_test_activity_lifecycle);
    if (!activity)
    {
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return;
    }

    eos_activity_set_title(activity, "ElenixOS Test");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    lv_obj_t *test_list = lv_list_create(view);
    lv_obj_set_size(test_list, lv_pct(100), lv_pct(100));
    eos_crown_encoder_set_target_obj(test_list);

    lv_obj_t *btn;
    lv_obj_t *label = lv_list_add_text(test_list, RI_ELENIX_WATCH " ElenixOS Test List");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    // Unit Tests
    btn = lv_list_add_button(test_list, LV_SYMBOL_LIST, "Unit Tests");
    lv_obj_add_event_cb(btn, _test_unit_runner_cb, LV_EVENT_CLICKED, NULL);
    // App Debugger
    btn = lv_list_add_button(test_list, RI_BUG_LINE, "App Debugger");
    lv_obj_add_event_cb(btn, _test_app_debug_page, LV_EVENT_CLICKED, NULL);
    // Sensor Tester
    btn = lv_list_add_button(test_list, RI_SENSOR_LINE, "SensorTester");
    lv_obj_add_event_cb(btn, _test_sensor, LV_EVENT_CLICKED, NULL);
    // Sensor Chart Test
    btn = lv_list_add_button(test_list, RI_SENSOR_LINE, "Sensor Chart");
    lv_obj_add_event_cb(btn, _test_sensor_chart_cb, LV_EVENT_CLICKED, NULL);
    // Multi-Sensor Status Chart
    btn = lv_list_add_button(test_list, RI_SENSOR_LINE, "Multi-Sensor Status");
    lv_obj_add_event_cb(btn, _test_sensor_multi_chart_cb, LV_EVENT_CLICKED, NULL);
    // Input Page Test
    btn = lv_list_add_button(test_list, RI_KEYBOARD_BOX_FILL, "Input Page Test");
    lv_obj_add_event_cb(btn, _test_input_page_cb, LV_EVENT_CLICKED, NULL);
    // Battery History Chart Test
    btn = lv_list_add_button(test_list, RI_BATTERY_FILL, "Battery History");
    lv_obj_add_event_cb(btn, _test_battery_history_cb, LV_EVENT_CLICKED, NULL);
    // Package Installer Test
    btn = lv_list_add_button(test_list, LV_SYMBOL_FILE, "Package Installer");
    lv_obj_add_event_cb(btn, _test_package_cb, LV_EVENT_CLICKED, NULL);
    // Audio Playback Test
    btn = lv_list_add_button(test_list, LV_SYMBOL_AUDIO, "Audio Playback");
    lv_obj_add_event_cb(btn, _test_audio_page, LV_EVENT_CLICKED, NULL);
    // Recording Test
    btn = lv_list_add_button(test_list, RI_RECORD_CIRCLE_FILL, "Recording");
    lv_obj_add_event_cb(btn, _test_recording_page, LV_EVENT_CLICKED, NULL);
    // Slide Widget Test
    btn = lv_list_add_button(test_list, RI_CAROUSEL_VIEW, "Slide Widget");
    lv_obj_add_event_cb(btn, _test_slide_widget, LV_EVENT_CLICKED, NULL);
    // Navigation Test
    btn = lv_list_add_button(test_list, RI_MENU_UNFOLD_FILL, "Navigation");
    lv_obj_add_event_cb(btn, _test_nav_cb_1, LV_EVENT_CLICKED, NULL);
    // Message List Test
    btn = lv_list_add_button(test_list, RI_CHAT_SMILE_FILL, "Message List");
    lv_obj_add_event_cb(btn, _test_msg_list, LV_EVENT_CLICKED, NULL);
    // Font Test
    btn = lv_list_add_button(test_list, RI_FONT_SANS_SERIF, "Font");
    lv_obj_add_event_cb(btn, _test_font, LV_EVENT_CLICKED, NULL);
    // Font Size Test
    btn = lv_list_add_button(test_list, RI_FONT_SIZE, "Font Size");
    lv_obj_add_event_cb(btn, _test_font_size, LV_EVENT_CLICKED, NULL);
    // Language Switch Test
    btn = lv_list_add_button(test_list, RI_TRANSLATE, "Language");
    lv_obj_add_event_cb(btn, _test_lang, LV_EVENT_CLICKED, NULL);
    // Image Test
    btn = lv_list_add_button(test_list, RI_IMAGE_FILL, "Image");
    lv_obj_add_event_cb(btn, _test_image, LV_EVENT_CLICKED, NULL);
    // App List Test
    btn = lv_list_add_button(test_list, RI_APPS_FILL, "App List");
    lv_obj_add_event_cb(btn, _test_app_list, LV_EVENT_CLICKED, NULL);
    // Watchface List Test
    btn = lv_list_add_button(test_list, RI_APPS_FILL, "Watchface List");
    lv_obj_add_event_cb(btn, _test_watchface_list, LV_EVENT_CLICKED, NULL);
    // Toast Test
    btn = lv_list_add_button(test_list, RI_MESSAGE_2_FILL, "Toast");
    lv_obj_add_event_cb(btn, _test_toast, LV_EVENT_CLICKED, NULL);
    // Panel Test
    btn = lv_list_add_button(test_list, RI_WINDOW_LINE, "Panel");
    lv_obj_add_event_cb(btn, _test_panel_list, LV_EVENT_CLICKED, NULL);
    // LVGL Symbols Test
    btn = lv_list_add_button(test_list, RI_OMEGA, "LVGL Symbols");
    lv_obj_add_event_cb(btn, _test_show_all_lv_symbols_list, LV_EVENT_CLICKED, NULL);

    eos_activity_enter(activity);
}

#endif /* EOS_ENABLE_TEST_APP */
