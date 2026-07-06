/**
 * @file eos_test_sensor_chart.c
 * @brief Sensor chart visualization test module
 */

#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

#include "eos_test_sensor_chart.h"
#include "eos_dev_sensor.h"
#include "eos_service_sensor.h"
#include "eos_log.h"
#include "eos_basic_widgets.h"
#include "eos_lang.h"
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_crown.h"
#include "lvgl.h"
#include <math.h>

#define EOS_LOG_TAG "SensorChart"

/* ============================================
 * Internal types and variables
 * ============================================ */

typedef struct
{
    lv_obj_t *chart;
    lv_obj_t *container;
    lv_chart_series_t *series_x;
    lv_chart_series_t *series_y;
    lv_chart_series_t *series_z;
    eos_dev_sensor_t *test_sensor;
    lv_timer_t *update_timer;
    uint32_t sample_count;
} _chart_context_t;

static _chart_context_t _ctx = {0};

/* ============================================
 * Test sensor operations
 * ============================================ */

static eos_sensor_data_t _test_sensor_generate_data(void)
{
    eos_sensor_data_t data = {0};
    /* Generate sine wave data for visualization */
    static uint32_t counter = 0;
    counter++;

    data.acce.x = (int32_t)(500 * sin(counter * 0.1f));
    data.acce.y = (int32_t)(500 * cos(counter * 0.1f));
    data.acce.z = (int32_t)(1000 + 200 * sin(counter * 0.05f));

    return data;
}

static void _test_sensor_init(eos_dev_sensor_t *dev)
{
    (void)dev;
    EOS_LOG_I("Test chart sensor initialized");
}

static void _test_sensor_deinit(eos_dev_sensor_t *dev)
{
    (void)dev;
    EOS_LOG_I("Test chart sensor deinitialized");
}

static void _test_sensor_enable(eos_dev_sensor_t *dev)
{
    (void)dev;
    EOS_LOG_I("Test chart sensor enabled");
}

static void _test_sensor_disable(eos_dev_sensor_t *dev)
{
    (void)dev;
    EOS_LOG_I("Test chart sensor disabled");
}

static void _test_sensor_set_sample_rate(eos_dev_sensor_t *dev, uint32_t hz)
{
    (void)dev;
    EOS_LOG_I("Test chart sensor sample rate set to %u Hz", hz);
}

static void _test_sensor_get_sample_rate(eos_dev_sensor_t *dev, uint32_t *hz)
{
    (void)dev;
    *hz = 10;
}

static const eos_dev_sensor_ops_t _test_sensor_ops = {
    .init = _test_sensor_init,
    .deinit = _test_sensor_deinit,
    .enable = _test_sensor_enable,
    .disable = _test_sensor_disable,
    .set_sample_rate = _test_sensor_set_sample_rate,
    .get_sample_rate = _test_sensor_get_sample_rate,
};

/* ============================================
 * Chart update callback
 * ============================================ */

static void _chart_update_cb(lv_timer_t *timer)
{
    if (!_ctx.chart || !_ctx.series_x || !_ctx.series_y || !_ctx.series_z)
    {
        return;
    }

    eos_sensor_raw_data_t data;
    eos_result_t result = eos_sensor_read_latest(EOS_SENSOR_TYPE_ACCE, &data);

    if (result == EOS_OK)
    {
        /* Add data to chart series */
        lv_chart_set_next_value(_ctx.chart, _ctx.series_x, data.data.acce.x);
        lv_chart_set_next_value(_ctx.chart, _ctx.series_y, data.data.acce.y);
        lv_chart_set_next_value(_ctx.chart, _ctx.series_z, data.data.acce.z);
        _ctx.sample_count++;
    }
}

/* ============================================
 * Activity lifecycle
 * ============================================ */

static void _sensor_chart_on_destroy(eos_activity_t *activity)
{
    LV_UNUSED(activity);

    /* Cleanup timer */
    if (_ctx.update_timer)
    {
        lv_timer_delete(_ctx.update_timer);
        _ctx.update_timer = NULL;
    }

    /* Unregister test sensor */
    /* Note: Current implementation doesn't support unregister, so we just clean up */
    _ctx.test_sensor = NULL;

    /* Reset context */
    _ctx.chart = NULL;
    _ctx.series_x = NULL;
    _ctx.series_y = NULL;
    _ctx.series_z = NULL;
    _ctx.container = NULL;
    _ctx.sample_count = 0;
}

static const eos_activity_lifecycle_t _s_sensor_chart_lifecycle = {.on_enter = NULL,
                                                                   .on_destroy = _sensor_chart_on_destroy,
                                                                   .on_pause = NULL,
                                                                   .on_resume = NULL};

/* ============================================
 * Main test function
 * ============================================ */

void eos_test_sensor_chart_start(void)
{
    eos_activity_t *activity = eos_activity_create(&_s_sensor_chart_lifecycle);
    if (!activity)
    {
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return;
    }

    eos_activity_set_title(activity, "Sensor Chart");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    /* Create container */
    _ctx.container = lv_obj_create(view);
    lv_obj_set_size(_ctx.container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(_ctx.container, 10, 0);
    lv_obj_set_flex_flow(_ctx.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_ctx.container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

    /* Register test sensor */
    eos_result_t result = eos_dev_sensor_register("chart_acce", EOS_SENSOR_TYPE_ACCE, &_test_sensor_ops);
    if (result == EOS_OK)
    {
        _ctx.test_sensor = eos_dev_sensor_find("chart_acce");
        EOS_LOG_I("Test sensor registered successfully");
    }
    else
    {
        EOS_LOG_W("Test sensor registration failed, using existing sensor");
        _ctx.test_sensor = eos_dev_sensor_get_default(EOS_SENSOR_TYPE_ACCE);
    }

    /* Set sample period (100ms = 10Hz) */
    eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 100);

    /* Create chart */
    _ctx.chart = lv_chart_create(_ctx.container);
    lv_obj_set_size(_ctx.chart, lv_pct(90), lv_pct(80));
    lv_chart_set_type(_ctx.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(_ctx.chart, LV_CHART_AXIS_PRIMARY_Y, -1500, 1500);
    lv_chart_set_point_count(_ctx.chart, 50);

    /* Add series */
    _ctx.series_x = lv_chart_add_series(_ctx.chart, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);
    _ctx.series_y = lv_chart_add_series(_ctx.chart, lv_color_hex(0x00FF00), LV_CHART_AXIS_PRIMARY_Y);
    _ctx.series_z = lv_chart_add_series(_ctx.chart, lv_color_hex(0x0000FF), LV_CHART_AXIS_PRIMARY_Y);

    /* Initialize series with zeros */
    for (uint32_t i = 0; i < 50; i++)
    {
        lv_chart_set_next_value(_ctx.chart, _ctx.series_x, 0);
        lv_chart_set_next_value(_ctx.chart, _ctx.series_y, 0);
        lv_chart_set_next_value(_ctx.chart, _ctx.series_z, 0);
    }

    /* Create legend */
    lv_obj_t *legend = lv_obj_create(_ctx.container);
    lv_obj_set_size(legend, lv_pct(90), 40);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

    /* X axis label */
    lv_obj_t *label_x = lv_label_create(legend);
    lv_label_set_text(label_x, "X");
    lv_obj_set_style_text_color(label_x, lv_color_hex(0xFF0000), 0);

    /* Y axis label */
    lv_obj_t *label_y = lv_label_create(legend);
    lv_label_set_text(label_y, "Y");
    lv_obj_set_style_text_color(label_y, lv_color_hex(0x00FF00), 0);

    /* Z axis label */
    lv_obj_t *label_z = lv_label_create(legend);
    lv_label_set_text(label_z, "Z");
    lv_obj_set_style_text_color(label_z, lv_color_hex(0x0000FF), 0);

    /* Create update timer (100ms interval to match sensor sample rate) */
    _ctx.update_timer = lv_timer_create(_chart_update_cb, 100, NULL);

    eos_activity_enter(activity);
}

#endif /* EOS_ENABLE_TEST_APP */
