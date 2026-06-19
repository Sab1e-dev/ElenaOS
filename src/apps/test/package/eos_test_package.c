/**
 * @file eos_test_package.c
 * @brief Package installation test module
 */

#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

#include "eos_test_package.h"
#include "eos_app.h"
#include "eos_watchface.h"
#include "eos_log.h"
#include "eos_activity.h"
#include "eos_basic_widgets.h"
#include "eos_storage_paths.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

#define EOS_LOG_TAG "PackageTest"
#define MAX_PATH_LEN 256

/* ============================================
 * Internal types and variables
 * ============================================ */

typedef struct {
    lv_obj_t *container;
    lv_obj_t *input_field;
    lv_obj_t *install_btn;
    lv_obj_t *status_label;
    char path_buffer[MAX_PATH_LEN];
} _package_context_t;

static _package_context_t _ctx = {0};

/* ============================================
 * Helper functions
 * ============================================ */

static bool _is_eapk_file(const char *path)
{
    size_t len = strlen(path);
    if (len < 5) return false;
    return strcmp(path + len - 5, ".eapk") == 0;
}

static bool _is_ewpk_file(const char *path)
{
    size_t len = strlen(path);
    if (len < 5) return false;
    return strcmp(path + len - 5, ".ewpk") == 0;
}

/* ============================================
 * Install button callback
 * ============================================ */

static void _install_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    if (!_ctx.input_field || !_ctx.status_label) {
        return;
    }

    const char *input_path = lv_textarea_get_text(_ctx.input_field);
    if (!input_path || strlen(input_path) == 0) {
        lv_label_set_text(_ctx.status_label, "Error: Please enter a path");
        lv_obj_set_style_text_color(_ctx.status_label, lv_color_hex(0xFF0000), 0);
        return;
    }

    // Auto prepend filesystem root if not absolute
    // EOS_SYS_ROOT_DIR = "fs/" is the filesystem root
    // User packages go directly under fs/, not fs/elenixos/
    char full_path[MAX_PATH_LEN];
    if (input_path[0] == '/') {
        // Absolute path, use as-is
        snprintf(full_path, sizeof(full_path), "%s", input_path);
    } else {
        // Relative path, prepend filesystem root (EOS_SYS_ROOT_DIR)
        snprintf(full_path, sizeof(full_path), "%s%s", EOS_SYS_ROOT_DIR, input_path);
    }

    lv_label_set_text(_ctx.status_label, "Installing...");
    lv_obj_set_style_text_color(_ctx.status_label, lv_color_hex(0xFFFF00), 0);
    lv_refr_now(NULL);

    EOS_LOG_I("Full package path: %s", full_path);

    eos_result_t ret;
    if (_is_eapk_file(full_path)) {
        EOS_LOG_I("Installing application: %s", full_path);
        ret = eos_app_install(full_path);
    } else if (_is_ewpk_file(full_path)) {
        EOS_LOG_I("Installing watchface: %s", full_path);
        ret = eos_watchface_install(full_path);
    } else {
        lv_label_set_text(_ctx.status_label, "Error: Unsupported file type");
        lv_obj_set_style_text_color(_ctx.status_label, lv_color_hex(0xFF0000), 0);
        return;
    }

    if (ret == EOS_OK) {
        lv_label_set_text(_ctx.status_label, "Installation successful!");
        lv_obj_set_style_text_color(_ctx.status_label, lv_color_hex(0x4CAF50), 0);
        lv_textarea_set_text(_ctx.input_field, "");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Installation failed (code: %d)", ret);
        lv_label_set_text(_ctx.status_label, msg);
        lv_obj_set_style_text_color(_ctx.status_label, lv_color_hex(0xFF0000), 0);
    }
}

/* ============================================
 * Activity lifecycle
 * ============================================ */

static void _package_test_on_destroy(eos_activity_t *activity)
{
    LV_UNUSED(activity);

    /* Reset context */
    _ctx.container = NULL;
    _ctx.input_field = NULL;
    _ctx.install_btn = NULL;
    _ctx.status_label = NULL;
    memset(_ctx.path_buffer, 0, sizeof(_ctx.path_buffer));
}

static const eos_activity_lifecycle_t _s_package_test_lifecycle = {
    .on_enter = NULL,
    .on_destroy = _package_test_on_destroy,
    .on_pause = NULL,
    .on_resume = NULL
};

/* ============================================
 * Main test function
 * ============================================ */

void eos_test_package_start(void)
{
    eos_activity_t *activity = eos_activity_create(&_s_package_test_lifecycle);
    if (!activity) {
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view) {
        return;
    }

    eos_activity_set_title(activity, "Package Installer");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    /* Create container */
    _ctx.container = lv_obj_create(view);
    lv_obj_set_size(_ctx.container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(_ctx.container, 15, 0);
    lv_obj_set_flex_flow(_ctx.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_ctx.container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

    /* Create path label */
    lv_obj_t *path_label = lv_label_create(_ctx.container);
    lv_label_set_text(path_label, "Package Path:");
    lv_obj_set_style_text_color(path_label, lv_color_white(), 0);

    /* Create input field */
    _ctx.input_field = lv_textarea_create(_ctx.container);
    lv_textarea_set_one_line(_ctx.input_field, true);
    lv_textarea_set_placeholder_text(_ctx.input_field, "my_app.eapk");
    lv_obj_set_width(_ctx.input_field, lv_pct(85));
    lv_textarea_set_max_length(_ctx.input_field, MAX_PATH_LEN);
    lv_obj_set_style_bg_color(_ctx.input_field, lv_color_black(), 0);
    lv_obj_set_style_border_color(_ctx.input_field, lv_color_white(), 0);
    lv_obj_set_style_border_width(_ctx.input_field, 1, 0);
    lv_obj_set_style_text_color(_ctx.input_field, lv_color_white(), 0);

    /* Create install button */
    _ctx.install_btn = lv_btn_create(_ctx.container);
    lv_obj_set_size(_ctx.install_btn, lv_pct(60), 40);
    lv_obj_add_event_cb(_ctx.install_btn, _install_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(_ctx.install_btn);
    lv_label_set_text(btn_label, "Install");
    lv_obj_center(btn_label);

    /* Create status label */
    _ctx.status_label = lv_label_create(_ctx.container);
    lv_label_set_text(_ctx.status_label, " ");
    lv_obj_set_style_text_color(_ctx.status_label, lv_color_white(), 0);
    lv_obj_set_width(_ctx.status_label, lv_pct(85));
    lv_label_set_long_mode(_ctx.status_label, LV_LABEL_LONG_WRAP);

    /* Create hint label */
    lv_obj_t *hint_label = lv_label_create(_ctx.container);
    lv_label_set_text(hint_label, "Supported: .eapk (app), .ewpk (watchface)\nPath auto-prefixed with '" EOS_SYS_ROOT_DIR "'");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x808080), 0);

    /* Enter activity */
    eos_activity_enter(activity);
}

#endif /* EOS_ENABLE_TEST_APP */
