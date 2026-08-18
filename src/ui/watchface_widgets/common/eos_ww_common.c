/**
 * @file eos_ww_common.c
 * @brief Common helpers shared by watchface widgets
 */

#include "eos_ww_common.h"

/* Includes ---------------------------------------------------*/
#include <stdarg.h>
#include <stdio.h>
#include "eos_mem.h"
#include "eos_service_sensor.h"
#define EOS_LOG_TAG "WatchfaceWidget"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/
#define _STATUS_VALUE_BUF_SIZE 64
/* Variables --------------------------------------------------*/
static const char *_weekday_abbrs[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

/* Function Implementations -----------------------------------*/

void eos_ww_make_static(lv_obj_t *obj)
{
    EOS_CHECK_PTR_RETURN(obj);

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
}

static void _timer_delete_cb(lv_event_t *e)
{
    lv_timer_t *timer = lv_event_get_user_data(e);
    if (timer)
    {
        lv_timer_delete(timer);
    }
}

void eos_ww_timer_autodelete(lv_obj_t *host, lv_timer_t *timer)
{
    if (!host || !timer)
    {
        return;
    }
    lv_obj_add_event_cb(host, _timer_delete_cb, LV_EVENT_DELETE, timer);
}

const char *eos_ww_weekday_abbr(uint8_t day_of_week)
{
    return (day_of_week < 7) ? _weekday_abbrs[day_of_week] : "???";
}

static void _status_timer_cb(lv_timer_t *timer)
{
    eos_ww_status_t *s = lv_timer_get_user_data(timer);
    if (!s || !s->update_cb)
    {
        return;
    }
    if (!s->container || !lv_obj_is_valid(s->container))
    {
        return;
    }
    s->update_cb(s);
}

static void _status_sensor_cb(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    eos_ww_status_t *s = user_data;
    (void)type;
    if (!s || !s->sensor_cb)
    {
        return;
    }
    if (!s->container || !lv_obj_is_valid(s->container))
    {
        return;
    }
    s->sensor_cb(s, data);
}

static void _status_delete_cb(lv_event_t *e)
{
    eos_ww_status_t *s = lv_event_get_user_data(e);
    if (!s)
    {
        return;
    }

    if (s->sensor_type != EOS_SENSOR_TYPE_UNKNOWN)
    {
        eos_sensor_unsubscribe(s->sensor_type, _status_sensor_cb, s);
    }
    if (s->timer)
    {
        lv_timer_delete(s->timer);
    }

    eos_free(s);
}

eos_ww_status_t *eos_ww_status_create(lv_obj_t *parent, const char *icon)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    eos_ww_status_t *s = eos_malloc_zeroed(sizeof(eos_ww_status_t));
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);

    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(container, 4, 0);
    eos_ww_make_static(container);

    if (icon)
    {
        lv_obj_t *icon_label = lv_label_create(container);
        lv_label_set_text(icon_label, icon);
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0xFFFFFF), 0);
        eos_ww_make_static(icon_label);
        s->icon = icon_label;
    }

    lv_obj_t *value = lv_label_create(container);
    lv_label_set_text(value, "0");
    lv_obj_set_style_text_color(value, lv_color_hex(0xFFFFFF), 0);
    eos_ww_make_static(value);
    s->value = value;
    s->container = container;
    s->sensor_type = EOS_SENSOR_TYPE_UNKNOWN;

    lv_obj_add_event_cb(container, _status_delete_cb, LV_EVENT_DELETE, s);

    return s;
}

lv_obj_t *eos_ww_status_get_container(eos_ww_status_t *s)
{
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);
    return s->container;
}

lv_obj_t *eos_ww_status_get_icon(eos_ww_status_t *s)
{
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);
    return s->icon;
}

lv_obj_t *eos_ww_status_get_value(eos_ww_status_t *s)
{
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);
    return s->value;
}

void eos_ww_status_set_value(eos_ww_status_t *s, const char *fmt, ...)
{
    if (!s || !s->value || !fmt)
    {
        return;
    }

    char buf[_STATUS_VALUE_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    lv_label_set_text(s->value, buf);
}

void eos_ww_status_set_icon(eos_ww_status_t *s, const char *icon)
{
    if (!s || !s->icon)
    {
        return;
    }
    lv_label_set_text(s->icon, icon ? icon : "");
}

void eos_ww_status_start_timer(eos_ww_status_t *s, eos_ww_status_update_cb_t cb, uint32_t interval_ms)
{
    EOS_CHECK_PTR_RETURN(s);
    EOS_CHECK_PTR_RETURN(cb);

    s->update_cb = cb;
    s->timer = lv_timer_create(_status_timer_cb, interval_ms, s);
    if (s->timer)
    {
        lv_timer_ready(s->timer);
    }
    cb(s);
}

void eos_ww_status_start_sensor(eos_ww_status_t *s,
                                eos_sensor_type_t type,
                                eos_ww_status_sensor_cb_t cb,
                                uint32_t min_interval_ms)
{
    EOS_CHECK_PTR_RETURN(s);
    EOS_CHECK_PTR_RETURN(cb);

    s->sensor_type = type;
    s->sensor_cb = cb;
    eos_sensor_subscribe(type, _status_sensor_cb, s, min_interval_ms);
}
