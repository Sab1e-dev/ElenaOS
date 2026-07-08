/**
 * @file eos_test_sensor_multi_chart.c
 * @brief Multi-sensor sampling status visualization test
 */

#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

#include "eos_test_sensor_multi_chart.h"
#include "eos_dev_sensor.h"
#include "eos_service_sensor.h"
#include "eos_log.h"
#include "eos_basic_widgets.h"
#include "eos_lang.h"
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_crown.h"
#include "lvgl.h"

#define EOS_LOG_TAG "MultiSensorChart"

/* ============================================
 * Internal types and variables
 * ============================================ */

#define MAX_SENSORS 10

typedef struct
{
    lv_obj_t *chart;
    lv_obj_t *container;
    lv_chart_series_t *series[MAX_SENSORS];
    eos_dev_sensor_t *sensors[MAX_SENSORS];
    uint8_t sensor_count;
    lv_timer_t *update_timer;
    uint32_t tick_count;
} _multi_chart_context_t;

static _multi_chart_context_t _ctx = {0};

static const lv_color_t _sensor_colors[MAX_SENSORS] = {
    LV_COLOR_MAKE(255, 0, 0), /* Red */
    LV_COLOR_MAKE(0, 255, 0), /* Green */
    LV_COLOR_MAKE(0, 0, 255), /* Blue */
    LV_COLOR_MAKE(255, 255, 0), /* Yellow */
    LV_COLOR_MAKE(255, 0, 255), /* Magenta */
    LV_COLOR_MAKE(0, 255, 255), /* Cyan */
    LV_COLOR_MAKE(255, 128, 0), /* Orange */
    LV_COLOR_MAKE(128, 0, 128), /* Purple */
    LV_COLOR_MAKE(255, 192, 203), /* Pink */
    LV_COLOR_MAKE(128, 128, 128) /* Gray */
};

/* ============================================
 * Chart update callback
 * ============================================ */

static void _multi_chart_update_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!_ctx.chart)
    {
        return;
    }

    _ctx.tick_count++;

    /* Update each sensor's series */
    for (uint8_t i = 0; i < _ctx.sensor_count; i++)
    {
        if (!_ctx.sensors[i] || !_ctx.series[i])
        {
            continue;
        }

        uint32_t period = eos_sensor_get_sample_period(_ctx.sensors[i]->type);

        /* Place a marker when it's time for this sensor to sample */
        int32_t value = 0;
        if (period > 0 && (_ctx.tick_count % (period / 100)) == 0)
        {
            value = 100 + (i * 50); // Different height for each sensor
        }

        lv_chart_set_next_value(_ctx.chart, _ctx.series[i], value);
    }
}

/* ============================================
 * Activity lifecycle
 * ============================================ */

static void _multi_sensor_chart_on_destroy(eos_activity_t *activity)
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
    _ctx.container = NULL;
    _ctx.sensor_count = 0;
    _ctx.tick_count = 0;

    for (uint8_t i = 0; i < MAX_SENSORS; i++)
    {
        _ctx.series[i] = NULL;
        _ctx.sensors[i] = NULL;
    }
}

static const eos_activity_lifecycle_t _s_multi_sensor_chart_lifecycle = {.on_enter = NULL,
                                                                         .on_destroy = _multi_sensor_chart_on_destroy,
                                                                         .on_pause = NULL,
                                                                         .on_resume = NULL};

/* ============================================
 * Main test function
 * ============================================ */

void eos_test_sensor_multi_chart_start(void)
{
    eos_activity_t *activity = eos_activity_create(&_s_multi_sensor_chart_lifecycle);
    if (!activity)
    {
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return;
    }

    eos_activity_set_title(activity, "Multi-Sensor Status");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    /* Create container */
    _ctx.container = lv_obj_create(view);
    lv_obj_set_size(_ctx.container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(_ctx.container, 10, 0);
    lv_obj_set_flex_flow(_ctx.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_ctx.container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

    /* Discover all registered sensors */
    eos_dev_sensor_t *dev = eos_dev_sensor_get_list_head();
    while (dev && _ctx.sensor_count < MAX_SENSORS)
    {
        if (dev->name)
        {
            _ctx.sensors[_ctx.sensor_count++] = dev;
            /* Set different sample periods for demonstration */
            uint32_t periods[] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
            eos_sensor_set_sample_period(dev->type, periods[_ctx.sensor_count - 1]);
        }
        dev = dev->_next;
    }

    /* Create chart */
    _ctx.chart = lv_chart_create(_ctx.container);
    lv_obj_set_size(_ctx.chart, lv_pct(90), lv_pct(70));
    lv_chart_set_type(_ctx.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(_ctx.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 600);
    lv_chart_set_point_count(_ctx.chart, 30);
    lv_chart_set_div_line_count(_ctx.chart, 5, 5);

    /* Add series for each sensor */
    for (uint8_t i = 0; i < _ctx.sensor_count; i++)
    {
        _ctx.series[i] = lv_chart_add_series(_ctx.chart, _sensor_colors[i], LV_CHART_AXIS_PRIMARY_Y);

        /* Initialize with zeros */
        for (uint32_t j = 0; j < 30; j++)
        {
            lv_chart_set_next_value(_ctx.chart, _ctx.series[i], 0);
        }
    }

    /* Create legend */
    lv_obj_t *legend = lv_obj_create(_ctx.container);
    lv_obj_set_size(legend, lv_pct(90), lv_pct(25));
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

    /* Add legend entries */
    for (uint8_t i = 0; i < _ctx.sensor_count; i++)
    {
        if (_ctx.sensors[i])
        {
            uint32_t period = eos_sensor_get_sample_period(_ctx.sensors[i]->type);

            lv_obj_t *legend_item = lv_obj_create(legend);
            lv_obj_set_size(legend_item, lv_pct(100), 30);
            lv_obj_set_flex_flow(legend_item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(legend_item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_BETWEEN);

            /* Color indicator */
            lv_obj_t *color_box = lv_obj_create(legend_item);
            lv_obj_set_size(color_box, 20, 20);
            lv_obj_set_style_bg_color(color_box, _sensor_colors[i], 0);

            /* Sensor name and period */
            char label_text[64];
            snprintf(label_text, sizeof(label_text), "%s (%ums)", _ctx.sensors[i]->name, period);
            lv_obj_t *label = lv_label_create(legend_item);
            lv_label_set_text(label, label_text);
        }
    }

    /* Create update timer (100ms interval) */
    _ctx.update_timer = lv_timer_create(_multi_chart_update_cb, 100, NULL);

    eos_activity_enter(activity);
}

#endif /* EOS_ENABLE_TEST_APP */
