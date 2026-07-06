/**
 * @file eos_test_battery_history.c
 * @brief Battery history chart visualization test module
 */

#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

#include "eos_test_battery_history.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "eos_service_battery.h"
#include "eos_log.h"
#include "eos_basic_widgets.h"
#include "eos_lang.h"
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_crown.h"
#include "lvgl.h"
#include <math.h>

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "BatteryHistory"

/* Variables --------------------------------------------------*/

typedef struct
{
    lv_obj_t *chart;
    lv_obj_t *container;
    lv_obj_t *info_label;
    lv_chart_series_t *series;
    lv_timer_t *update_timer;
    uint32_t history_count;
} _chart_context_t;

static _chart_context_t _ctx = {0};

/* Function Implementations -----------------------------------*/

static void _chart_update_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (!_ctx.chart || !_ctx.series)
    {
        return;
    }

    /* Reset series data to 0 */
    lv_chart_set_all_value(_ctx.chart, _ctx.series, 0);

    /* Get history count */
    uint32_t count = eos_battery_get_history_count();
    if (count == 0)
    {
        if (_ctx.info_label)
        {
            lv_label_set_text(_ctx.info_label, "No battery history data");
        }
        return;
    }

    /* Update history count display */
    if (_ctx.info_label)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "History: %u entries", count);
        lv_label_set_text(_ctx.info_label, buf);
    }

    /* Add history data to chart */
    eos_battery_state_t entry;
    uint32_t max_entries = lv_chart_get_point_count(_ctx.chart);
    uint32_t start_idx = (count > max_entries) ? (count - max_entries) : 0;

    for (uint32_t i = start_idx; i < count; i++)
    {
        if (eos_battery_get_history_entry(i, &entry))
        {
            lv_chart_set_next_value(_ctx.chart, _ctx.series, entry.percent);
        }
    }
}

static void _battery_history_on_destroy(eos_activity_t *activity)
{
    LV_UNUSED(activity);

    /* Cleanup timer */
    if (_ctx.update_timer)
    {
        lv_timer_delete(_ctx.update_timer);
        _ctx.update_timer = NULL;
    }

    /* Reset context */
    _ctx.chart = NULL;
    _ctx.series = NULL;
    _ctx.container = NULL;
    _ctx.info_label = NULL;
    _ctx.history_count = 0;
}

static const eos_activity_lifecycle_t _s_battery_history_lifecycle = {.on_enter = NULL,
                                                                      .on_destroy = _battery_history_on_destroy,
                                                                      .on_pause = NULL,
                                                                      .on_resume = NULL};

void eos_test_battery_history_start(void)
{
    eos_activity_t *activity = eos_activity_create(&_s_battery_history_lifecycle);
    if (!activity)
    {
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return;
    }

    eos_activity_set_title(activity, "Battery History");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    /* Create container */
    _ctx.container = lv_obj_create(view);
    lv_obj_set_size(_ctx.container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(_ctx.container, 10, 0);
    lv_obj_set_flex_flow(_ctx.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_ctx.container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

    /* Create info label */
    _ctx.info_label = lv_label_create(_ctx.container);
    lv_label_set_text(_ctx.info_label, "Loading battery history...");
    lv_obj_set_style_text_color(_ctx.info_label, lv_color_white(), 0);

    /* Create chart */
    _ctx.chart = lv_chart_create(_ctx.container);
    lv_obj_set_size(_ctx.chart, lv_pct(90), lv_pct(70));
    lv_chart_set_type(_ctx.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(_ctx.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(_ctx.chart, 64);

    /* Set chart style */
    lv_obj_set_style_bg_color(_ctx.chart, lv_color_black(), 0);
    lv_obj_set_style_border_color(_ctx.chart, lv_color_white(), 0);
    lv_obj_set_style_border_width(_ctx.chart, 1, 0);

    /* Create series */
    _ctx.series = lv_chart_add_series(_ctx.chart, lv_color_hex(0x4CAF50), LV_CHART_AXIS_PRIMARY_Y);

    /* Add grid lines */
    lv_chart_set_div_line_count(_ctx.chart, 5, 5);

    /* Create update timer */
    _ctx.update_timer = lv_timer_create(_chart_update_cb, 2000, NULL);

    /* Initial update */
    _chart_update_cb(NULL);

    /* Enter activity */
    eos_activity_enter(activity);
}

#endif /* EOS_ENABLE_TEST_APP */
