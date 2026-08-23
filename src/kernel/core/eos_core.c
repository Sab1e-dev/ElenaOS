/**
 * @file eos_core.c
 * @brief Elenix OS core code implementation
 */

#include "eos_core.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "lvgl.h"
#include "eos_image.h"
#include "eos_msg_list.h"
#include "eos_lang.h"
#include "eos_basic_widgets.h"
#include "eos_event.h"
#include "eos_git_info.h"
#include "eos_test.h"
#include "eos_version.h"
#include "eos_port.h"
#include "eos_swipe_panel.h"
#include "eos_service_display.h"
#include "eos_service_config.h"
#include "eos_service_lock.h"
#include "eos_app.h"
#include "script_engine_core.h"
#include "spm.h"
#include "eos_watchface.h"
#include "eos_watchface_list.h"
#include "eos_app_list.h"
#include "eos_theme.h"
#include "eos_config.h"
#include "eos_config_internal.h"
#include "jerryscript.h"
#include "eos_font.h"
#include "eos_service_sensor.h"
#include "eos_service_cache.h"
#include "eos_dispatcher.h"
#include "eos_anim.h"
#include "eos_control_center.h"
#include "eos_recent_apps.h"
#include "eos_chrome_manager.h"
#include "eos_service_audio.h"
#include "eos_service_storage.h"
#include "eos_service_state.h"
#include "eos_service_battery.h"
#include "eos_developer_options.h"
#include "eos_service_pm.h"
#include "eos_dfw.h"
#include "eos_app_header.h"
#include "eos_toast.h"
#include "eos_service_haptic.h"
#include "eos_crown.h"
#include "eos_service_permission.h"
#include "eos_overlay_layer.h"
#include "eos_icon.h"
#include "eos_activity.h"
#include "eos_std_widgets.h"
#include "eos_service_storage.h"
#define EOS_LOG_TAG "Core"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/
static bool _is_inited = false;

/* Function Implementations -----------------------------------*/

static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";

static void _format_build_date(char *out, size_t size)
{
    char month_str[4];
    int day, year;

    sscanf(__DATE__, "%3s %d %d", month_str, &day, &year);

    int month = (strstr(months, month_str) - months) / 3 + 1;

    snprintf(out, size, "%04d-%02d-%02d", year, month, day);
}

static void _print_boot_info(void)
{
    char build_date[16];
    _format_build_date(build_date, sizeof(build_date));
    EOS_LOG_I("System initializing...");
    char lvgl_version[32];
    char jerry_version[32];
    char cjson_version[32];

    snprintf(lvgl_version, sizeof(lvgl_version),
            "%d.%d.%d%s",
            LVGL_VERSION_MAJOR,
            LVGL_VERSION_MINOR,
            LVGL_VERSION_PATCH,
            LVGL_VERSION_INFO);

    snprintf(jerry_version, sizeof(jerry_version),
            "%d.%d.%d",
            JERRY_API_MAJOR_VERSION,
            JERRY_API_MINOR_VERSION,
            JERRY_API_PATCH_VERSION);

    snprintf(cjson_version, sizeof(cjson_version),
            "%d.%d.%d",
            CJSON_VERSION_MAJOR,
            CJSON_VERSION_MINOR,
            CJSON_VERSION_PATCH);

    /* ElenixOS */
    EOS_LOG_I("ElenixOS v" ELENIX_OS_VERSION_FULL);
    EOS_LOG_I("  %-12s %s", "Git:", EOS_GIT_INFO);
    EOS_LOG_I("  %-12s %s %s", "Build:", build_date, __TIME__);
    EOS_LOG_I("  %-12s %s", "Mode:",
            EOS_COMPILE_MODE == EOS_DEBUG ? "DEBUG" : "RELEASE");

    EOS_LOG_I("");

    /* Third-party components */
    EOS_LOG_I("Components:");
    EOS_LOG_I("  %-12s %-16s (%s)", "LVGL:", lvgl_version, EOS_GIT_LVGL);
    EOS_LOG_I("  %-12s %-16s (%s)", "JerryScript:", jerry_version, EOS_GIT_JERRYSCRIPT);
    EOS_LOG_I("  %-12s %-16s (%s)", "cJSON:", cjson_version, EOS_GIT_CJSON);
    EOS_LOG_I("  %-12s %-16s (%s)", "RemixIcon:", "-", EOS_GIT_REMIXICON);
}

static lv_indev_t *_get_key_indev()
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev)
    {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD)
        {
            return indev;
        }
        indev = lv_indev_get_next(indev);
    }
    EOS_LOG_W("Not found input device: key");
}

void _sys_init_err_handler(const char *err_msg)
{
    EOS_LOG_E("System initialization failed: %s", err_msg);
    lv_obj_t *list = eos_std_info_create(lv_layer_sys(),
                                         EOS_COLOR_RED,
                                         RI_BUG_LINE,
                                         eos_lang_get_text(STR_ID_SYS_INIT_FAILED),
                                         eos_lang_get_text(STR_ID_SYS_INIT_FAILED_CONTENT));
    lv_obj_set_style_pad_top(list, 80, 0);
    char info_str[1024];
    snprintf(info_str, sizeof(info_str), "Error: %s", err_msg);
    lv_obj_t *err_label = eos_list_add_comment(list, info_str);
    (void)err_label;

    for (uint8_t i = 0; i < 3; ++i)
    {
        uint32_t delay = lv_timer_handler();
        eos_delay(delay);
    }
}

void eos_logo_play(bool anim)
{
    eos_display_set_brightness(EOS_DISPLAY_BRIGHTNESS_MAX, EOS_DISPLAY_DURATION_OFF, false);

    lv_obj_t *scr = lv_screen_active();

    if (!scr)
    {
        EOS_LOG_W("Active screen not found, creating a new screen for logo");
        lv_obj_t *scr = lv_obj_create(NULL);
        lv_screen_load(scr);
    }

    // Create full screen container
    lv_obj_t *logo_container = lv_obj_create(scr);
    lv_obj_set_style_bg_color(logo_container, EOS_COLOR_BLACK, 0);
    lv_obj_set_size(logo_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_border_width(logo_container, 0, 0);
    lv_obj_move_foreground(logo_container);

    // Create LOGO image object
    lv_obj_t *logo_img = lv_image_create(logo_container);
    lv_image_set_src(logo_img, EOS_IMG_LOGO);
    lv_obj_center(logo_img);

    lv_timer_handler();
}

void eos_init(void)
{
    /* Log system initialization ----------------------------------*/
    eos_service_log_init();

    /* Image cache initialization ---------------------------------*/
    eos_service_cache_init();

    _print_boot_info();
    /* System components initialization ---------------------------*/
    eos_service_storage_init();
    eos_logo_play(true);
    eos_lang_init();
    eos_dispatcher_init();
    eos_toast_init();
    eos_service_haptic_init();
    eos_crown_init();
    script_engine_init();
    spm_init();
    eos_service_config_init();
    eos_service_state_init();
    eos_service_permission_init();
    eos_service_battery_init();
    lv_font_t *default_font = eos_font_init();
    if (!default_font)
        _sys_init_err_handler("Failed to initialize default font");
    eos_theme_set(lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), default_font);
    eos_app_init();
    eos_watchface_init();
    eos_overlay_layer_init();
    eos_app_header_init();
    eos_msg_list_init();
    eos_control_center_init();
    eos_chrome_manager_init();
#if EOS_RECENT_APPS_ENABLE
    eos_recent_apps_init();
#endif
    eos_service_pm_init();
    eos_service_audio_init();
    eos_service_lock_init();

    eos_activity_t *watchface_activity = eos_watchface_get_activity();
    if (!watchface_activity)
        _sys_init_err_handler("Failed to get watchface activity");

    // Activity controller will automatically delete Logo Screen
    if (eos_activity_controller_init(watchface_activity) != EOS_OK)
        _sys_init_err_handler("Failed to initialize activity controller");

    _is_inited = true;
}

bool eos_is_initialized(void)
{
    return _is_inited;
}

/* Main Loop --------------------------------------------------*/

uint32_t eos_main_loop(void)
{
    if (!_is_inited)
    {
        EOS_LOG_E("System not initialized. Please call eos_init() before eos_main_loop().");
        return 0;
    }
    eos_dispatch_tick();
    uint32_t d = lv_timer_handler();

    eos_developer_options_update();

    return d;
}

uint32_t eos_tick_get(void)
{
    return lv_tick_get();
}
