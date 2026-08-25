/**
 * @file eos_ww_date_window.c
 * @brief Watchface date window (weekday above day-of-month)
 */

#include "eos_ww_date_window.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include "eos_ww_common.h"
#include "eos_mem.h"
#include "eos_font.h"
#include "eos_service_time.h"
#define EOS_LOG_TAG "DateWindow"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/
typedef struct
{
    lv_obj_t *weekday;
    lv_obj_t *day;
    lv_timer_t *timer;
} _date_window_t;

/* Function Implementations -----------------------------------*/

static void _date_window_update(_date_window_t *w)
{
    eos_datetime_t now = eos_time_get();
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", now.day);
    lv_label_set_text(w->weekday, eos_ww_weekday_abbr(now.day_of_week));
    lv_label_set_text(w->day, buf);
}

static void _date_window_timer_cb(lv_timer_t *timer)
{
    _date_window_t *w = lv_timer_get_user_data(timer);
    if (!w)
    {
        return;
    }
    _date_window_update(w);
}

static void _date_window_delete_cb(lv_event_t *e)
{
    _date_window_t *w = lv_event_get_user_data(e);
    if (!w)
    {
        return;
    }
    if (w->timer)
    {
        lv_timer_delete(w->timer);
    }
    eos_free(w);
}

lv_obj_t *eos_ww_date_window_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    _date_window_t *w = eos_malloc_zeroed(sizeof(_date_window_t));
    EOS_CHECK_PTR_RETURN_VAL(w, NULL);

    lv_obj_t *win = lv_obj_create(parent);
    lv_obj_set_size(win, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(win, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(win, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(win, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(win, 0, 0);
    eos_ww_make_static(win);

    lv_obj_t *weekday = lv_label_create(win);
    eos_label_set_font_size(weekday, EOS_FONT_SIZE_SMALL);
    lv_obj_set_style_text_align(weekday, LV_TEXT_ALIGN_CENTER, 0);
    eos_ww_make_static(weekday);

    lv_obj_t *day = lv_label_create(win);
    eos_label_set_font_size(day, EOS_FONT_SIZE_MEDIUM);
    lv_obj_set_style_text_align(day, LV_TEXT_ALIGN_CENTER, 0);
    eos_ww_make_static(day);

    w->weekday = weekday;
    w->day = day;
    w->timer = lv_timer_create(_date_window_timer_cb, 60000, w);
    lv_obj_add_event_cb(win, _date_window_delete_cb, LV_EVENT_DELETE, w);

    _date_window_update(w);
    return win;
}
