/**
 * @file eos_watchface_js.c
 * @brief JavaScript script watchface implementation
 */

#include "eos_watchface_js.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#define EOS_LOG_TAG "WFJS"
#include "eos_log.h"
#include "spm.h"
#include "eos_app.h"
#include "eos_theme.h"
#include "eos_mem.h"
#include "eos_service_storage.h"
#include "eos_watchface_list.h"
#include "eos_pkg_mgr.h"
#include "eos_msg_list.h"
#include "eos_control_center.h"
#include "eos_event.h"

/* Function Prototypes ----------------------------------------*/
static void _js_on_enter(eos_activity_t *activity);
static void _js_on_pause(eos_activity_t *activity);
static void _js_on_resume(eos_activity_t *activity);
static void _js_on_destroy(eos_activity_t *activity);
static script_pkg_t _js_load_package_from_disk(const char *watchface_id);
static void _js_handle_error(eos_watchface_instance_t *self, int32_t error_code);
static void _js_long_pressed_cb(lv_event_t *e);

/* Static Variables -------------------------------------------*/

/**
 * @brief Activity lifecycle callbacks for JS watchface
 */
static const eos_activity_lifecycle_t _js_lifecycle = {
    .on_enter = _js_on_enter,
    .on_pause = _js_on_pause,
    .on_resume = _js_on_resume,
    .on_destroy = _js_on_destroy,
};

/* Function Implementations -----------------------------------*/

eos_watchface_instance_t *eos_watchface_js_create(const char *watchface_id)
{
    if (!watchface_id)
    {
        EOS_LOG_E("watchface_id is NULL");
        return NULL;
    }

    script_pkg_t pkg = _js_load_package_from_disk(watchface_id);
    if (!pkg.script_str)
    {
        EOS_LOG_E("Failed to load JS watchface package: %s", watchface_id);
        return NULL;
    }

    eos_watchface_instance_t *instance = eos_malloc(sizeof(eos_watchface_instance_t));
    if (!instance)
    {
        EOS_LOG_E("Failed to allocate JS watchface instance");
        eos_pkg_free(&pkg);
        return NULL;
    }

    memset(instance, 0, sizeof(*instance));
    instance->type = EOS_WATCHFACE_TYPE_JS;
    snprintf(instance->id, sizeof(instance->id), "%s", watchface_id);
    instance->lifecycle = &_js_lifecycle;
    instance->data.js.pkg = pkg;

    instance->activity = eos_activity_create_root(&_js_lifecycle);
    if (!instance->activity)
    {
        EOS_LOG_E("Failed to create activity for JS watchface");
        eos_pkg_free(&instance->data.js.pkg);
        eos_free(instance);
        return NULL;
    }

    eos_activity_set_type(instance->activity, EOS_ACTIVITY_TYPE_WATCHFACE);
    eos_activity_set_user_data(instance->activity, instance);

    EOS_LOG_I("Created JS watchface instance: %s", watchface_id);
    return instance;
}

static void _js_on_enter(eos_activity_t *activity)
{
    EOS_LOG_I("JS watchface '%s': enter", ((eos_watchface_instance_t *)eos_activity_get_user_data(activity))->id);

    eos_watchface_instance_t *self = eos_activity_get_user_data(activity);

    // View is auto-created by framework (eos_activity_create_root + controller_init/replace_root)
    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        EOS_LOG_E("JS watchface: view is NULL!");
        return;
    }

    lv_obj_add_event_cb(view, _js_long_pressed_cb, LV_EVENT_LONG_PRESSED, NULL);

    eos_result_t ret = spm_watchface_start(&self->data.js.pkg, view);
    if (ret != EOS_OK)
    {
        _js_handle_error(self, ret);
    }

    eos_msg_list_show();
    eos_control_center_show();
}

static void _js_on_pause(eos_activity_t *activity)
{
    EOS_LOG_I("JS watchface: pause");

    eos_control_center_hide();
    eos_msg_list_hide();

    spm_watchface_pause();
}

static void _js_on_resume(eos_activity_t *activity)
{
    EOS_LOG_I("JS watchface: resume");

    eos_watchface_instance_t *self = eos_activity_get_user_data(activity);

    eos_result_t ret = spm_watchface_resume();
    if (ret != EOS_OK)
    {
        EOS_LOG_W("watchface_resume failed (%d), falling back to full reload", ret);

        lv_obj_t *view = eos_activity_get_view(activity);
        script_pkg_t pkg = _js_load_package_from_disk(self->id);
        if (pkg.script_str)
        {
            ret = spm_watchface_start(&pkg, view);
            if (ret != EOS_OK)
            {
                _js_handle_error(self, ret);
            }
            eos_pkg_free(&pkg);
        }
        else
        {
            EOS_LOG_E("JS watchface: failed to reload package on resume");
        }
    }

    eos_msg_list_show();
    eos_control_center_show();
}

static void _js_on_destroy(eos_activity_t *activity)
{
    EOS_LOG_I("JS watchface: destroy");

    eos_watchface_instance_t *self = eos_activity_get_user_data(activity);

    spm_watchface_destroy();

    if (self)
    {
        eos_pkg_free(&self->data.js.pkg);
    }
}

static script_pkg_t _js_load_package_from_disk(const char *watchface_id)
{
    script_pkg_t pkg = {0};
    pkg.type = SCRIPT_TYPE_WATCHFACE;

    char manifest_path[EOS_FS_PATH_MAX];
    snprintf(manifest_path,
             sizeof(manifest_path),
             EOS_WATCHFACE_INSTALLED_DIR "%s/" EOS_WATCHFACE_MANIFEST_FILE_NAME,
             watchface_id);

    if (script_engine_get_manifest(manifest_path, &pkg) != EOS_OK)
    {
        EOS_LOG_E("Read manifest failed: %s", manifest_path);
        return pkg;
    }

    char script_path[EOS_FS_PATH_MAX];
    snprintf(script_path,
             sizeof(script_path),
             EOS_WATCHFACE_INSTALLED_DIR "%s/" EOS_WATCHFACE_SCRIPT_ENTRY_FILE_NAME,
             watchface_id);

    char base_path[EOS_FS_PATH_MAX];
    snprintf(base_path, sizeof(base_path), EOS_WATCHFACE_INSTALLED_DIR "%s/", watchface_id);
    pkg.base_path = eos_strdup(base_path);

    if (!eos_storage_is_file(script_path))
    {
        EOS_LOG_E("Can't find script: %s", script_path);
        eos_pkg_free(&pkg);
        return (script_pkg_t){0};
    }

    pkg.script_str = eos_storage_read_file(script_path);
    return pkg;
}

/* Forward declaration for restart callback */
static void _js_restart_cb(lv_event_t *e);

static void _js_handle_error(eos_watchface_instance_t *self, int32_t error_code)
{
    const char *error_info = script_engine_get_error_info();
    eos_script_error_type_t error_type = EOS_SCRIPT_FAULT_ERROR_EXCEPTION;

    if (error_info && strstr(error_info, "Engine crash"))
    {
        error_type = EOS_SCRIPT_FAULT_ENGINE_CRASH;
    }
    else if (error_info && strstr(error_info, "timeout"))
    {
        error_type = EOS_SCRIPT_FAULT_UNRESPONSIVE;
    }

    eos_script_error_handler_cfg_t cfg = {
        .title_id = STR_ID_WATCHFACE_RUN_ERR_TITLE,
        .confirm_btn_id = STR_ID_RESTART,
        .confirm_cb = _js_restart_cb,
        .cancel_btn_id = STR_ID_WATCHFACE_SWITCH,
        .cancel_cb = _js_long_pressed_cb,
    };

    eos_app_handle_script_error(error_type, error_code, self->id, &cfg);
    EOS_LOG_E("Watchface encountered a fatal error");
}

static void _js_restart_cb(lv_event_t *e)
{
    (void)e;
    char wf_id[64] = {0};

    /* Try crash context first (for engine crash recovery path) */
    const spm_crash_state_t *crash = spm_get_crash_state();
    if (crash && crash->has_crash && crash->script_id[0])
    {
        snprintf(wf_id, sizeof(wf_id), "%s", crash->script_id);
    }
    else
    {
        /* Fallback: get watchface_id from current activity */
        eos_activity_t *current = eos_activity_get_current();
        if (current)
        {
            eos_watchface_instance_t *self = eos_activity_get_user_data(current);
            if (self && self->id[0])
            {
                snprintf(wf_id, sizeof(wf_id), "%s", self->id);
            }
        }
    }

    spm_clear_crash_state();

    if (wf_id[0])
    {
        EOS_LOG_I("Restarting watchface: %s", wf_id);
        eos_watchface_switch_to(wf_id);
    }
    else
    {
        EOS_LOG_W("Restart watchface failed: no id available, entering watchface list");
        eos_watchface_list_enter();
    }
}

/* EOS_EVENT_SCRIPT_FATAL handler for watchface callback crashes */
static void _js_on_script_fatal(eos_event_t *event)
{
    (void)event;
    const spm_crash_state_t *crash = spm_get_crash_state();
    if (!crash || !crash->has_crash)
    {
        EOS_LOG_W("Script fatal event but no crash context (WF)");
        return;
    }

    if (crash->script_type != SCRIPT_TYPE_WATCHFACE)
        return;

    EOS_LOG_W("Handling watchface script fatal: id=%s", crash->script_id);

    eos_activity_t *current = eos_activity_get_current();
    if (current && eos_activity_get_type(current) == EOS_ACTIVITY_TYPE_WATCHFACE)
    {
        eos_script_error_handler_cfg_t cfg = {
            .title_id = STR_ID_WATCHFACE_RUN_ERR_TITLE,
            .confirm_btn_id = STR_ID_RESTART,
            .confirm_cb = _js_restart_cb,
            .cancel_btn_id = STR_ID_WATCHFACE_SWITCH,
            .cancel_cb = _js_long_pressed_cb,
        };
        eos_app_handle_script_error(EOS_SCRIPT_FAULT_ENGINE_CRASH, EOS_ERR_SCRIPT_EXCEPTION, crash->script_id, &cfg);
    }

    /* NOTE: Do NOT clear crash state here — callbacks clear it when user taps a button */
}

static void _js_long_pressed_cb(lv_event_t *e)
{
    eos_watchface_list_enter();
}

void eos_watchface_js_crash_handler_init(void)
{
    eos_event_subscribe(EOS_EVENT_SCRIPT_FATAL, _js_on_script_fatal, NULL);
    EOS_LOG_I("Watchface JS crash handler initialized");
}
