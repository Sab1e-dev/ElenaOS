/**
 * @file eos_ww_weekday_ring.c
 * @brief Watchface weekday strip (7 labels, today highlighted)
 */

#include "eos_ww_weekday_ring.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#include "eos_mem.h"
#include "eos_font.h"
#include "eos_service_time.h"
#define EOS_LOG_TAG "WeekdayRing"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/
typedef struct
{
    lv_obj_t *labels[7];
    lv_timer_t *timer;
    lv_color_t idle_color;
    lv_color_t active_color;
} _weekday_ring_t;

/* Function Implementations -----------------------------------*/

static void _weekday_update(_weekday_ring_t *w)
{
    eos_datetime_t now = eos_time_get();
    uint8_t today = (now.day_of_week < 7) ? now.day_of_week : 0;
    for (uint8_t i = 0; i < 7; i++)
    {
        lv_obj_set_style_text_color(w->labels[i], (i == today) ? w->active_color : w->idle_color, 0);
    }
}

static void _weekday_timer_cb(lv_timer_t *timer)
{
    _weekday_ring_t *w = lv_timer_get_user_data(timer);
    if (!w)
    {
        return;
    }
    _weekday_update(w);
}

static void _weekday_delete_cb(lv_event_t *e)
{
    _weekday_ring_t *w = lv_event_get_user_data(e);
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

lv_obj_t *eos_ww_weekday_ring_create(lv_obj_t *parent, uint32_t idle_color, uint32_t active_color)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    _weekday_ring_t *w = eos_malloc_zeroed(sizeof(_weekday_ring_t));
    EOS_CHECK_PTR_RETURN_VAL(w, NULL);

    lv_obj_t *ring = lv_obj_create(parent);
    lv_obj_set_size(ring, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(ring, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ring, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ring, 3, 0);
    eos_ww_make_static(ring);

    w->idle_color = lv_color_hex(idle_color);
    w->active_color = lv_color_hex(active_color);

    for (uint8_t i = 0; i < 7; i++)
    {
        lv_obj_t *label = lv_label_create(ring);
        lv_label_set_text(label, eos_ww_weekday_abbr(i));
        eos_label_set_font_size(label, EOS_FONT_SIZE_SMALL);
        eos_ww_make_static(label);
        w->labels[i] = label;
    }

    w->timer = lv_timer_create(_weekday_timer_cb, 60000, w);
    lv_obj_add_event_cb(ring, _weekday_delete_cb, LV_EVENT_DELETE, w);

    _weekday_update(w);
    return ring;
}
