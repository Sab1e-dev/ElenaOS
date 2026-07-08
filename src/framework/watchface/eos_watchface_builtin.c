/**
 * @file eos_watchface_builtin.c
 * @brief Built-in fallback watchface implementation
 */

#include "eos_watchface_builtin.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#define EOS_LOG_TAG "WFBuiltin"
#include "eos_log.h"
#include "eos_service_time.h"
#include "eos_theme.h"
#include "eos_mem.h"
#include "eos_watchface_list.h"
#include "eos_msg_list.h"
#include "eos_control_center.h"
#include "eos_activity.h"

/* Static Variables ------------------------------------------*/

static void _builtin_on_enter(eos_activity_t *activity);
static void _builtin_on_pause(eos_activity_t *activity);
static void _builtin_on_resume(eos_activity_t *activity);
static void _builtin_on_destroy(eos_activity_t *activity);

/**
 * @brief Activity lifecycle callbacks for built-in watchface
 */
static const eos_activity_lifecycle_t _builtin_lifecycle = {
    .on_enter = _builtin_on_enter,
    .on_pause = _builtin_on_pause,
    .on_resume = _builtin_on_resume,
    .on_destroy = _builtin_on_destroy,
};

/* Function Implementations -----------------------------------*/
static void _builtin_time_update_cb(lv_timer_t *timer);
static void _builtin_view_delete_cb(lv_event_t *e);
static void _builtin_long_pressed_cb(lv_event_t *e);

eos_watchface_instance_t *eos_watchface_builtin_create(void)
{
    eos_watchface_instance_t *instance = eos_malloc(sizeof(eos_watchface_instance_t));
    if (!instance)
    {
        EOS_LOG_E("Failed to allocate builtin watchface instance");
        return NULL;
    }

    memset(instance, 0, sizeof(*instance));
    instance->type = EOS_WATCHFACE_TYPE_BUILTIN;
    snprintf(instance->id, sizeof(instance->id), "%s", EOS_WATCHFACE_BUILTIN_FALLBACK_ID);
    instance->lifecycle = &_builtin_lifecycle;

    instance->activity = eos_activity_create_root(&_builtin_lifecycle);
    if (!instance->activity)
    {
        EOS_LOG_E("Failed to create activity for builtin watchface");
        eos_free(instance);
        return NULL;
    }

    eos_activity_set_type(instance->activity, EOS_ACTIVITY_TYPE_WATCHFACE);
    eos_activity_set_user_data(instance->activity, instance);

    EOS_LOG_I("Created builtin watchface instance");
    return instance;
}

static void _builtin_on_enter(eos_activity_t *activity)
{
    EOS_LOG_I("Builtin watchface: enter");

    eos_watchface_instance_t *self = eos_activity_get_user_data(activity);

    // View is auto-created by framework (eos_activity_create_root + controller_init/replace_root)
    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        EOS_LOG_E("Builtin watchface: view is NULL!");
        return;
    }

    lv_obj_t *title = lv_label_create(view);
    lv_label_set_text(title, "ElenixOS");
    lv_obj_set_style_text_color(title, lv_color_hex(0xD7E2F2), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    lv_obj_t *time_label = lv_label_create(view);
    lv_label_set_text(time_label, "00:00");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *hint = lv_label_create(view);
    lv_label_set_text(hint, "Hello World!");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x91A4BF), 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 36);

    self->data.builtin.time_update_timer = lv_timer_create(_builtin_time_update_cb, 1000, time_label);

    if (self->data.builtin.time_update_timer)
    {
        lv_obj_add_event_cb(view, _builtin_view_delete_cb, LV_EVENT_DELETE, self->data.builtin.time_update_timer);
        lv_timer_ready(self->data.builtin.time_update_timer);
        _builtin_time_update_cb(self->data.builtin.time_update_timer);
    }

    lv_obj_add_event_cb(view, _builtin_long_pressed_cb, LV_EVENT_LONG_PRESSED, NULL);

    eos_msg_list_show();
    eos_control_center_show();
}

static void _builtin_on_pause(eos_activity_t *activity)
{
    EOS_LOG_I("Builtin watchface: pause");

    eos_watchface_instance_t *self = eos_activity_get_user_data(activity);

    if (self->data.builtin.time_update_timer)
    {
        lv_timer_pause(self->data.builtin.time_update_timer);
    }

    eos_control_center_hide();
    eos_msg_list_hide();
}

static void _builtin_on_resume(eos_activity_t *activity)
{
    EOS_LOG_I("Builtin watchface: resume");

    eos_watchface_instance_t *self = eos_activity_get_user_data(activity);

    if (self->data.builtin.time_update_timer)
    {
        lv_timer_resume(self->data.builtin.time_update_timer);
    }

    eos_msg_list_show();
    eos_control_center_show();
}

static void _builtin_on_destroy(eos_activity_t *activity)
{
    EOS_LOG_I("Builtin watchface: destroy");
}

static void _builtin_time_update_cb(lv_timer_t *timer)
{
    lv_obj_t *time_label = lv_timer_get_user_data(timer);
    if (!time_label || !lv_obj_is_valid(time_label))
    {
        return;
    }

    eos_datetime_t now = eos_time_get();
    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d", now.hour, now.min);
    lv_label_set_text(time_label, buf);
}

static void _builtin_view_delete_cb(lv_event_t *e)
{
    lv_timer_t *timer = lv_event_get_user_data(e);
    if (timer)
    {
        lv_timer_delete(timer);
    }
}

static void _builtin_long_pressed_cb(lv_event_t *e)
{
    eos_watchface_list_enter();
}
