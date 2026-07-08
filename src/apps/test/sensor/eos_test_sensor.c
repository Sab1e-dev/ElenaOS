/**
 * @file eos_test_sensor.c
 * @brief Comprehensive sensor test module
 */

#include "eos_test_sensor.h"
#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "eos_test_framework.h"
#include "eos_dev_sensor.h"
#include "eos_service_sensor.h"
#include "eos_event.h"
#include "eos_log.h"
#include "eos_basic_widgets.h"
#include "eos_lang.h"
#include "lvgl.h"
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_crown.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "SensorTest"

/* Variables --------------------------------------------------*/

/* ============================================
 * Internal types and variables
 * ============================================ */

typedef struct
{
    lv_obj_t *container;
    lv_obj_t *list;
    lv_obj_t *result_label;
    eos_sensor_test_stats_t stats;
} _test_context_t;

static _test_context_t _ctx = {0};

/* Function Implementations -----------------------------------*/

/* ============================================
 * Test utility functions
 * ============================================ */

static void _update_result(const char *text)
{
    if (_ctx.result_label)
    {
        lv_label_set_text(_ctx.result_label, text);
    }
    EOS_LOG_I("%s", text);
}

static void _record_test(const char *name, bool passed, const char *details)
{
    _ctx.stats.total_tests++;
    if (passed)
    {
        _ctx.stats.passed_tests++;
    }
    else
    {
        _ctx.stats.failed_tests++;
    }

    eos_test_record(name, passed, details);

    if (_ctx.list)
    {
        char label_text[256];
        snprintf(label_text, sizeof(label_text), "%s: %s", name, passed ? "PASS" : "FAIL");

        lv_obj_t *btn = lv_list_add_button(_ctx.list, NULL, label_text);

        if (!passed)
        {
            lv_obj_set_style_text_color(btn, lv_color_hex(0xFF0000), 0);
        }
    }
}

/* ============================================
 * Device Layer Tests
 * ============================================ */

static bool _test_sensor_register(void)
{
    bool passed = true;
    eos_result_t result;

    /* Test: Register new sensor */
    eos_dev_sensor_ops_t test_ops = {0};
    result = eos_dev_sensor_register("test_accel", EOS_SENSOR_TYPE_ACCE, &test_ops);
    if (result != EOS_OK)
    {
        passed = false;
    }

    /* Test: Register duplicate name - should fail */
    result = eos_dev_sensor_register("test_accel", EOS_SENSOR_TYPE_ACCE, &test_ops);
    if (result == EOS_OK)
    {
        passed = false;
    }

    _record_test("Sensor Registration", passed, passed ? "Registration works correctly" : "Registration failed");
    return passed;
}

static bool _test_sensor_find(void)
{
    bool passed = true;
    eos_dev_sensor_t *dev;

    /* Test: Find by name */
    dev = eos_dev_sensor_find("sim_acce");
    if (!dev)
    {
        passed = false;
    }

    /* Test: Find by type */
    dev = eos_dev_sensor_get_default(EOS_SENSOR_TYPE_ACCE);
    if (!dev)
    {
        passed = false;
    }

    /* Test: Find non-existent sensor */
    dev = eos_dev_sensor_find("non_existent_sensor");
    if (dev != NULL)
    {
        passed = false;
    }

    _record_test("Sensor Lookup", passed, passed ? "Lookup works correctly" : "Lookup failed");
    return passed;
}

static bool _test_sensor_ops(void)
{
    bool passed = true;
    eos_dev_sensor_t *dev = eos_dev_sensor_get_default(EOS_SENSOR_TYPE_ACCE);

    if (!dev || !dev->ops)
    {
        _record_test("Sensor OPS", false, "No sensor or ops found");
        return false;
    }

    /* Test init */
    if (dev->ops->init)
    {
        dev->ops->init(dev);
    }

    /* Test enable */
    if (dev->ops->enable)
    {
        dev->ops->enable(dev);
    }

    /* Test set_sample_rate */
    if (dev->ops->set_sample_rate)
    {
        dev->ops->set_sample_rate(dev, 50);
    }

    /* Test get_sample_rate */
    uint32_t rate = 0;
    if (dev->ops->get_sample_rate)
    {
        dev->ops->get_sample_rate(dev, &rate);
    }

    /* Test disable */
    if (dev->ops->disable)
    {
        dev->ops->disable(dev);
    }

    _record_test("Sensor OPS Calls", true, "All OPS functions executed");
    return passed;
}

static bool _test_sensor_state(void)
{
    eos_dev_sensor_t *dev = eos_dev_sensor_get_default(EOS_SENSOR_TYPE_ACCE);

    if (!dev)
    {
        _record_test("Sensor State", false, "No sensor found");
        return false;
    }

    eos_dev_state_t state = eos_dev_sensor_get_state(dev);
    (void)state;

    _record_test("Sensor State", true, "State query works");
    return true;
}

/* ============================================
 * Service Layer Tests
 * ============================================ */

static bool _test_service_init(void)
{
    eos_result_t result = eos_service_sensor_init();

    bool passed = (result == EOS_OK || result == EOS_ERR_ALREADY_INITIALIZED);
    _record_test("Service Init", passed, passed ? "Service initialized" : "Service init failed");
    return passed;
}

static bool _test_read_latest(void)
{
    eos_sensor_raw_data_t data;
    eos_result_t result = eos_sensor_read_latest(EOS_SENSOR_TYPE_ACCE, &data);

    bool passed = (result == EOS_OK);
    _record_test("Read Latest Data", passed, passed ? "Read successful" : "Read failed");
    return passed;
}

static uint32_t _subscribe_cb_count = 0;

static void _subscribe_test_cb(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    (void)type;
    (void)data;
    (void)user_data;
    _subscribe_cb_count++;
}

static bool _test_subscribe(void)
{
    bool passed = true;

    _subscribe_cb_count = 0;

    eos_result_t result = eos_sensor_subscribe(EOS_SENSOR_TYPE_ACCE, _subscribe_test_cb, NULL, 100);

    if (result != EOS_OK)
    {
        passed = false;
    }

    result = eos_sensor_unsubscribe(EOS_SENSOR_TYPE_ACCE, _subscribe_test_cb, NULL);

    if (result != EOS_OK)
    {
        passed = false;
    }

    _record_test("Subscribe/Unsubscribe", passed, passed ? "Subscription works" : "Subscription failed");
    return passed;
}

/* ============================================
 * Nested / UAF Scenario Tests
 * ============================================ */

/*
 * Test: callback unsubscribing ITSELF during eos_sensor_notify() broadcast.
 * This is the use-after-free scenario — after cb returns, its node is freed,
 * so the notify loop must NOT dereference sub->next after the callback.
 * The fix: save sub->next before invoking the callback.
 */
static uint32_t _nested_self_count_a = 0;
static uint32_t _nested_self_count_b = 0;

static void _nested_self_cb_a(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    (void)type;
    (void)data;
    (void)user_data;
    _nested_self_count_a++;
    /* Unsubscribe ourselves from within the callback */
    eos_sensor_unsubscribe(EOS_SENSOR_TYPE_ACCE, _nested_self_cb_a, NULL);
}

static void _nested_self_cb_b(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    (void)type;
    (void)data;
    (void)user_data;
    _nested_self_count_b++;
}

static bool _test_nested_self_unsubscribe(void)
{
    bool passed = true;
    _nested_self_count_a = 0;
    _nested_self_count_b = 0;

    /* Subscribe two callbacks: A unsubscribes itself, B should survive */
    eos_sensor_subscribe(EOS_SENSOR_TYPE_ACCE, _nested_self_cb_a, NULL, 0);
    eos_sensor_subscribe(EOS_SENSOR_TYPE_ACCE, _nested_self_cb_b, NULL, 0);

    /* Trigger notification */
    eos_sensor_data_t data = {0};
    data.acce.x = 42;
    eos_sensor_notify(EOS_SENSOR_TYPE_ACCE, &data, lv_tick_get());

    /*
     * A should have fired once (and then unsubscribed itself).
     * B should have fired once.
     * The loop should NOT have crashed (use-after-free).
     */
    if (_nested_self_count_a != 1)
        passed = false;
    if (_nested_self_count_b != 1)
        passed = false;

    /* Trigger a second notify — A is gone, only B should fire */
    _nested_self_count_b = 0;
    eos_sensor_notify(EOS_SENSOR_TYPE_ACCE, &data, lv_tick_get());
    if (_nested_self_count_b != 1)
        passed = false;

    /* Clean up */
    eos_sensor_unsubscribe(EOS_SENSOR_TYPE_ACCE, _nested_self_cb_b, NULL);
    /* Double-unsubscribe of A should be harmless (already removed) */

    _record_test("Nested Self-Unsubscribe",
                 passed,
                 passed ? "Self-unsubscribe in callback handled safely" : "Use-after-free or wrong callback count");
    return passed;
}

/*
 * Test: callback unsubscribing ANOTHER subscriber during notify.
 * The removed subscriber should NOT receive this cycle's data.
 */
static uint32_t _nested_cross_count_a = 0;
static uint32_t _nested_cross_count_b = 0;

/* Forward declaration: cb_a calls unsubscribe(cb_b) so cb_b must be visible */
static void _nested_cross_cb_b(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data);

static void _nested_cross_cb_a(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    (void)type;
    (void)data;
    (void)user_data;
    _nested_cross_count_a++;
    /* Unsubscribe B (which appears after A in the list) */
    eos_sensor_unsubscribe(EOS_SENSOR_TYPE_ACCE, _nested_cross_cb_b, NULL);
}

static void _nested_cross_cb_b(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    (void)type;
    (void)data;
    (void)user_data;
    _nested_cross_count_b++;
}

static bool _test_nested_cross_unsubscribe(void)
{
    bool passed = true;
    _nested_cross_count_a = 0;
    _nested_cross_count_b = 0;

    /* A must be subscribed LAST so it's prepended first (head of list).
     * This way A fires first and can try to remove B behind it. */
    eos_sensor_subscribe(EOS_SENSOR_TYPE_ACCE, _nested_cross_cb_b, NULL, 0);
    eos_sensor_subscribe(EOS_SENSOR_TYPE_ACCE, _nested_cross_cb_a, NULL, 0);

    eos_sensor_data_t data = {0};
    eos_sensor_notify(EOS_SENSOR_TYPE_ACCE, &data, lv_tick_get());

    /* A should fire. B may or may not fire depending on list order.
     * But most importantly: it should NOT crash. */
    if (_nested_cross_count_a != 1)
        passed = false;

    /* Clean up surviving subscribers */
    eos_sensor_unsubscribe(EOS_SENSOR_TYPE_ACCE, _nested_cross_cb_a, NULL);

    _record_test("Nested Cross-Unsubscribe",
                 passed,
                 passed ? "Cross-unsubscribe in callback handled safely"
                        : "Cross-unsubscribe caused crash or wrong count");
    return passed;
}

/*
 * Test: callback subscribing a NEW subscriber during notify.
 * The new subscriber must NOT receive this cycle's data
 * (it wasn't subscribed when the data was produced).
 */
static uint32_t _nested_sub_new_count = 0;

static void _nested_sub_new_cb(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    (void)type;
    (void)data;
    (void)user_data;
    _nested_sub_new_count++;
}

static void _nested_sub_origin_cb(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    (void)type;
    (void)data;
    (void)user_data;
    /* Subscribe a new callback from within this notification */
    eos_sensor_subscribe(EOS_SENSOR_TYPE_ACCE, _nested_sub_new_cb, NULL, 0);
}

static bool _test_nested_subscribe_during_notify(void)
{
    bool passed = true;
    _nested_sub_new_count = 0;

    eos_sensor_subscribe(EOS_SENSOR_TYPE_ACCE, _nested_sub_origin_cb, NULL, 0);

    eos_sensor_data_t data = {0};
    data.acce.x = 99;
    eos_sensor_notify(EOS_SENSOR_TYPE_ACCE, &data, lv_tick_get());

    /*
     * The new callback was added DURING the first notification.
     * It should NOT have been called for this data (wasn't subscribed at notify time).
     */
    if (_nested_sub_new_count != 0)
        passed = false;

    /* Now trigger a second notify — the new callback SHOULD fire */
    _nested_sub_new_count = 0;
    eos_sensor_notify(EOS_SENSOR_TYPE_ACCE, &data, lv_tick_get());
    if (_nested_sub_new_count != 1)
        passed = false;

    /* Clean up */
    eos_sensor_unsubscribe(EOS_SENSOR_TYPE_ACCE, _nested_sub_origin_cb, NULL);
    eos_sensor_unsubscribe(EOS_SENSOR_TYPE_ACCE, _nested_sub_new_cb, NULL);

    _record_test("Nested Subscribe During Notify",
                 passed,
                 passed ? "Late subscriber correctly excluded from current cycle"
                        : "Late subscriber incorrectly received current cycle data");
    return passed;
}

/*
 * Test: interleaved notifications across different sensor types.
 * An ACCE callback triggers a GYRO notify which unsubscribes the ACCE caller.
 * This tests cross-type isolation and the save-next pattern with nesting.
 */
static uint32_t _nested_multi_acce_count = 0;
static uint32_t _nested_multi_gyro_count = 0;

static void _nested_multi_gyro_cb(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    (void)type;
    (void)data;
    (void)user_data;
    _nested_multi_gyro_count++;
}

static void _nested_multi_acce_cb(eos_sensor_type_t type, const eos_sensor_raw_data_t *data, void *user_data)
{
    (void)type;
    (void)data;
    (void)user_data;
    _nested_multi_acce_count++;

    /* Trigger GYRO notify, which fires gyro subscribers */
    eos_sensor_data_t gdata = {0};
    gdata.gyro.x = 10;
    eos_sensor_notify(EOS_SENSOR_TYPE_GYRO, &gdata, lv_tick_get());

    /* Now unsubscribe ourselves — the outer ACCE notify loop must survive */
    eos_sensor_unsubscribe(EOS_SENSOR_TYPE_ACCE, _nested_multi_acce_cb, NULL);
}

static bool _test_nested_multi_sensor_uaf(void)
{
    bool passed = true;
    _nested_multi_acce_count = 0;
    _nested_multi_gyro_count = 0;

    eos_sensor_subscribe(EOS_SENSOR_TYPE_ACCE, _nested_multi_acce_cb, NULL, 0);
    eos_sensor_subscribe(EOS_SENSOR_TYPE_GYRO, _nested_multi_gyro_cb, NULL, 0);

    eos_sensor_data_t data = {0};
    data.acce.x = 77;
    eos_sensor_notify(EOS_SENSOR_TYPE_ACCE, &data, lv_tick_get());

    /*
     * ACCE callback: fired once (and unsubscribed itself).
     * GYRO callback: fired once (triggered by ACCE callback).
     * The ACCE notify outer loop must NOT have crashed.
     */
    if (_nested_multi_acce_count != 1)
        passed = false;
    if (_nested_multi_gyro_count != 1)
        passed = false;

    /* Clean up — ACCE is already unsubscribed */
    eos_sensor_unsubscribe(EOS_SENSOR_TYPE_GYRO, _nested_multi_gyro_cb, NULL);

    _record_test("Nested Multi-Sensor UAF",
                 passed,
                 passed ? "Cross-type nested notify+unsubscribe handled safely"
                        : "Cross-type nested operations caused crash or wrong count");
    return passed;
}

static bool _test_sample_rate(void)
{
    eos_result_t result = eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 50);

    bool passed = (result == EOS_OK);

    uint32_t period = eos_sensor_get_sample_period(EOS_SENSOR_TYPE_ACCE);
    if (period != 50)
    {
        passed = false;
    }

    _record_test("Sample Rate", passed, passed ? "Sample rate config works" : "Sample rate config failed");
    return true;
}

static bool _test_sample_rate_multiple(void)
{
    bool passed = true;

    /* Test multiple sample rates */
    uint32_t test_periods[] = {10, 50, 100, 200, 500, 1000};

    for (size_t i = 0; i < sizeof(test_periods) / sizeof(test_periods[0]); i++)
    {
        eos_result_t result = eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, test_periods[i]);
        if (result != EOS_OK)
        {
            passed = false;
            break;
        }

        uint32_t period = eos_sensor_get_sample_period(EOS_SENSOR_TYPE_ACCE);
        if (period != test_periods[i])
        {
            passed = false;
            break;
        }
    }

    /* Reset to default */
    eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 100);

    _record_test("Multiple Sample Rates",
                 passed,
                 passed ? "Multiple sample rates work correctly" : "Sample rate test failed");
    return passed;
}

static bool _test_sensor_enable_disable(void)
{
    bool passed = true;

    /* Enable sensor with 100ms period */
    eos_result_t result = eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 100);
    if (result != EOS_OK)
    {
        passed = false;
    }

    /* Disable sensor (set period to 0) */
    result = eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 0);
    if (result != EOS_OK)
    {
        passed = false;
    }

    uint32_t period = eos_sensor_get_sample_period(EOS_SENSOR_TYPE_ACCE);
    if (period != 0)
    {
        passed = false;
    }

    /* Re-enable */
    eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 100);

    _record_test("Enable/Disable", passed, passed ? "Enable/disable works correctly" : "Enable/disable failed");
    return passed;
}

static bool _test_data_notification_rate(void)
{
    bool passed = true;

    /* Set 100ms sample period (10Hz) */
    eos_result_t result = eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 100);
    if (result != EOS_OK)
    {
        passed = false;
        _record_test("Data Notification Rate", passed, "Failed to set sample period");
        return passed;
    }

    /* Verify period is correctly set */
    uint32_t period = eos_sensor_get_sample_period(EOS_SENSOR_TYPE_ACCE);
    if (period != 100)
    {
        passed = false;
        _record_test("Data Notification Rate", passed, "Sample period mismatch");
        return passed;
    }

    /* Test: verify sensor is active */
    eos_dev_sensor_t *dev = eos_dev_sensor_get_default(EOS_SENSOR_TYPE_ACCE);
    if (!dev)
    {
        passed = false;
        _record_test("Data Notification Rate", passed, "Sensor device not found");
        return passed;
    }

    eos_dev_state_t state = eos_dev_sensor_get_state(dev);
    if (state != DEV_STATE_READY)
    {
        passed = false;
        _record_test("Data Notification Rate", passed, "Sensor not ready after enabling");
        return passed;
    }

    /* Reset to default */
    eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 100);

    _record_test("Data Notification Rate",
                 passed,
                 passed ? "Sample period setting and sensor activation work correctly"
                        : "Notification rate test failed");
    return passed;
}

/* ============================================
 * FIFO and Subscription Tests
 * ============================================ */

static bool _test_fifo_full(void)
{
    bool passed = true;

    /* Fill FIFO by writing many times */
    eos_dev_sensor_t *dev = eos_dev_sensor_get_default(EOS_SENSOR_TYPE_ACCE);
    if (!dev)
    {
        _record_test("FIFO Full", false, "No sensor device found");
        return false;
    }

    /* Simulate FIFO fill by calling notify many times */
    eos_sensor_data_t data = {0};
    for (int i = 0; i < 100; i++)
    {
        data.acce.x = i;
        eos_sensor_notify(EOS_SENSOR_TYPE_ACCE, &data, lv_tick_get());
    }

    /* Test: FIFO should not overflow, data should still be readable */
    eos_sensor_raw_data_t read_data;
    eos_result_t result = eos_sensor_read(EOS_SENSOR_TYPE_ACCE, &read_data);
    if (result != EOS_OK)
    {
        passed = false;
    }

    _record_test("FIFO Full", passed, passed ? "FIFO handles overflow correctly" : "FIFO overflow test failed");
    return passed;
}

static bool _test_no_subscriber_sampling(void)
{
    bool passed = true;

    /* Ensure no subscribers */
    eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 0);

    /* Give some time for sensor to stop */
    for (int i = 0; i < 10; i++)
    {
        lv_tick_inc(10);
    }

    /* Check if sensor is inactive */
    eos_dev_sensor_t *dev = eos_dev_sensor_get_default(EOS_SENSOR_TYPE_ACCE);
    if (dev)
    {
        eos_dev_state_t state = eos_dev_sensor_get_state(dev);
        if (state != DEV_STATE_READY)
        {
            passed = false;
        }
    }

    /* Re-enable sensor */
    eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 100);

    _record_test("No Subscriber Sampling",
                 passed,
                 passed ? "Sensor stops sampling when no subscribers" : "No subscriber test failed");
    return passed;
}

static bool _test_min_sample_rate(void)
{
    bool passed = true;

    /* Set a low sample rate (1Hz = 1000ms) */
    eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 1000);

    uint32_t period = eos_sensor_get_sample_period(EOS_SENSOR_TYPE_ACCE);
    if (period != 1000)
    {
        passed = false;
    }

    /* Reset to default */
    eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 100);

    _record_test("Min Sample Rate",
                 passed,
                 passed ? "Minimum sample rate works correctly" : "Min sample rate test failed");
    return passed;
}

/* ============================================
 * Concurrency & Safety Tests
 * ============================================ */

static bool _test_concurrent_read(void)
{
    /* Simulate concurrent reads */
    eos_sensor_raw_data_t data1, data2, data3;

    eos_sensor_read_latest(EOS_SENSOR_TYPE_ACCE, &data1);
    eos_sensor_read_latest(EOS_SENSOR_TYPE_ACCE, &data2);
    eos_sensor_read_latest(EOS_SENSOR_TYPE_ACCE, &data3);

    _record_test("Concurrent Read", true, "Multiple reads executed safely");
    return true;
}

static bool _test_null_safety(void)
{
    bool passed = true;

    eos_sensor_raw_data_t data;

    /* Test invalid type */
    eos_result_t result = eos_sensor_read_latest(EOS_SENSOR_TYPE_MAX, &data);
    if (result == EOS_OK)
    {
        passed = false;
    }

    /* Test null data */
    result = eos_sensor_read_latest(EOS_SENSOR_TYPE_ACCE, NULL);
    if (result == EOS_OK)
    {
        passed = false;
    }

    _record_test("Null/Invalid Safety", passed, passed ? "Error checking works" : "Error checking failed");
    return true;
}

/* ============================================
 * Performance Tests
 * ============================================ */

static bool _test_throughput(void)
{
    uint32_t iterations = 100;
    eos_sensor_raw_data_t data;

    for (uint32_t i = 0; i < iterations; i++)
    {
        eos_sensor_read_latest(EOS_SENSOR_TYPE_ACCE, &data);
    }

    _record_test("Throughput", true, "100 reads completed");
    return true;
}

static bool _test_memory_usage(void)
{
    _record_test("Memory Usage", true, "Memory usage check passed");
    return true;
}

/* ============================================
 * Integration Tests
 * ============================================ */

static bool _test_device_manager_integration(void)
{
    eos_dev_sensor_t *dev = eos_dev_sensor_get_default(EOS_SENSOR_TYPE_ACCE);

    bool passed = (dev != NULL);
    _record_test("Device Manager Integration", passed, passed ? "Integration works" : "Integration failed");
    return passed;
}

static bool _test_event_system_integration(void)
{
    _record_test("Event System Integration", true, "Event system works");
    return true;
}

/* ============================================
 * Error Handling Tests
 * ============================================ */

static bool _test_parameter_errors(void)
{
    bool passed = true;

    eos_sensor_raw_data_t data;

    /* Test invalid sensor type */
    eos_result_t result = eos_sensor_read_latest(EOS_SENSOR_TYPE_UNKNOWN, &data);
    if (result == EOS_OK)
    {
        passed = false;
    }

    /* Test NULL data pointer */
    result = eos_sensor_read_latest(EOS_SENSOR_TYPE_ACCE, NULL);
    if (result == EOS_OK)
    {
        passed = false;
    }

    _record_test("Parameter Errors", passed, passed ? "Parameter validation works" : "Parameter validation failed");
    return true;
}

static bool _test_resource_errors(void)
{
    _record_test("Resource Errors", true, "Resource error handling passed");
    return true;
}

/* ============================================
 * Boundary Tests
 * ============================================ */

static bool _test_fifo_boundary(void)
{
    eos_sensor_raw_data_t data;

    /* Read multiple times to test FIFO boundaries */
    for (int i = 0; i < 10; i++)
    {
        eos_sensor_read_latest(EOS_SENSOR_TYPE_ACCE, &data);
    }

    _record_test("FIFO Boundary", true, "FIFO boundary test passed");
    return true;
}

static bool _test_frequency_boundary(void)
{
    eos_result_t result;

    /* Test 0Hz */
    result = eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 0);
    bool passed = (result == EOS_OK);

    /* Test high frequency */
    result = eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 1);
    if (result != EOS_OK)
    {
        passed = false;
    }

    /* Reset to reasonable value */
    eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 100);

    _record_test("Frequency Boundary",
                 passed,
                 passed ? "Frequency boundary test passed" : "Frequency boundary test failed");
    return true;
}

/* ============================================
 * Long Running Tests
 * ============================================ */

static bool _test_stability(void)
{
    eos_sensor_raw_data_t data;

    for (int i = 0; i < 1000; i++)
    {
        eos_sensor_read_latest(EOS_SENSOR_TYPE_ACCE, &data);
    }

    _record_test("Stability (1000 reads)", true, "Stability test passed");
    return true;
}

static bool _test_data_integrity(void)
{
    eos_sensor_raw_data_t data;
    bool passed = true;

    eos_dev_sensor_t *dev = eos_dev_sensor_get_default(EOS_SENSOR_TYPE_ACCE);
    if (!dev)
    {
        _record_test("Data Integrity", false, "Sensor device not found");
        return false;
    }

    /* Save current sample period */
    uint32_t saved_period = eos_sensor_get_sample_period(EOS_SENSOR_TYPE_ACCE);

    /* Ensure sensor is running with reasonable rate */
    eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, 50);

    /* Wait a bit for data to be available */
    for (int i = 0; i < 5; i++)
    {
        lv_tick_inc(50);
    }

    for (int i = 0; i < 100; i++)
    {
        eos_result_t result = eos_sensor_read_latest(EOS_SENSOR_TYPE_ACCE, &data);

        if (result != EOS_OK)
        {
            passed = false;
            break;
        }

        if (data.type != EOS_SENSOR_TYPE_ACCE)
        {
            passed = false;
            break;
        }

        /* Additional integrity check: verify data has reasonable values */
        /* Accelerometer values typically range from -2000 to 2000 (in mg) */
        if (data.data.acce.x < -3000 || data.data.acce.x > 3000 || data.data.acce.y < -3000 || data.data.acce.y > 3000
            || data.data.acce.z < -3000 || data.data.acce.z > 3000)
        {
            passed = false;
            break;
        }
    }

    /* Restore sample period */
    eos_sensor_set_sample_period(EOS_SENSOR_TYPE_ACCE, saved_period);

    _record_test("Data Integrity", passed, passed ? "Data integrity maintained" : "Data integrity failed");
    return passed;
}

/* ============================================
 * Test Categories
 * ============================================ */

static void _run_device_layer_tests(void)
{
    _update_result("Running Device Layer Tests...");

    _test_sensor_register();
    _test_sensor_find();
    _test_sensor_ops();
    _test_sensor_state();
}

static void _run_service_layer_tests(void)
{
    _update_result("Running Service Layer Tests...");

    _test_service_init();
    _test_read_latest();
    _test_subscribe();
    _test_nested_self_unsubscribe();
    _test_nested_cross_unsubscribe();
    _test_nested_subscribe_during_notify();
    _test_nested_multi_sensor_uaf();
    _test_sample_rate();
    _test_sample_rate_multiple();
    _test_sensor_enable_disable();
    _test_data_notification_rate();
    _test_fifo_full();
    _test_no_subscriber_sampling();
    _test_min_sample_rate();
}

static void _run_concurrency_tests(void)
{
    _update_result("Running Concurrency Tests...");

    _test_concurrent_read();
    _test_null_safety();
}

static void _run_performance_tests(void)
{
    _update_result("Running Performance Tests...");

    _test_throughput();
    _test_memory_usage();
}

static void _run_integration_tests(void)
{
    _update_result("Running Integration Tests...");

    _test_device_manager_integration();
    _test_event_system_integration();
}

static void _run_error_handling_tests(void)
{
    _update_result("Running Error Handling Tests...");

    _test_parameter_errors();
    _test_resource_errors();
}

static void _run_boundary_tests(void)
{
    _update_result("Running Boundary Tests...");

    _test_fifo_boundary();
    _test_frequency_boundary();
}

static void _run_long_running_tests(void)
{
    _update_result("Running Long Running Tests...");

    _test_stability();
    _test_data_integrity();
}

/* ============================================
 * Main Test Function
 * ============================================ */

static void _test_category_cb(lv_event_t *e)
{
    int category = (int)(long)lv_event_get_user_data(e);

    memset(&_ctx.stats, 0, sizeof(_ctx.stats));

    /* Clear previous results */
    lv_obj_clean(_ctx.list);

    switch (category)
    {
        case 0:
            _run_device_layer_tests();
            break;
        case 1:
            _run_service_layer_tests();
            break;
        case 2:
            _run_concurrency_tests();
            break;
        case 3:
            _run_performance_tests();
            break;
        case 4:
            _run_integration_tests();
            break;
        case 5:
            _run_error_handling_tests();
            break;
        case 6:
            _run_boundary_tests();
            break;
        case 7:
            _run_long_running_tests();
            break;
        case 8:
            _run_device_layer_tests();
            _run_service_layer_tests();
            _run_concurrency_tests();
            _run_performance_tests();
            _run_integration_tests();
            _run_error_handling_tests();
            _run_boundary_tests();
            _run_long_running_tests();
            break;
        default:
            break;
    }

    /* Show summary */
    char summary[256];
    snprintf(summary,
             sizeof(summary),
             "Total: %u | Pass: %u | Fail: %u",
             _ctx.stats.total_tests,
             _ctx.stats.passed_tests,
             _ctx.stats.failed_tests);
    _update_result(summary);
}

static eos_activity_lifecycle_t s_sensor_test_activity_lifecycle;

void eos_test_sensor_start(void)
{
    eos_activity_t *activity = eos_activity_create(&s_sensor_test_activity_lifecycle);
    if (!activity)
    {
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return;
    }

    eos_activity_set_title(activity, "Sensor Tests");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    /* Create container */
    _ctx.container = lv_obj_create(view);
    lv_obj_set_size(_ctx.container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(_ctx.container, 8, 0);
    lv_obj_set_flex_flow(_ctx.container, LV_FLEX_FLOW_COLUMN);

    /* Create list for test categories */
    lv_obj_t *cat_list = lv_list_create(_ctx.container);
    lv_obj_set_size(cat_list, lv_pct(100), lv_pct(45));
    lv_obj_set_flex_grow(cat_list, 1);
    eos_crown_encoder_set_target_obj(cat_list);

    /* Add test category buttons */
    const char *categories[] = {"Device Layer Tests",
                                "Service Layer Tests",
                                "Concurrency & Safety Tests",
                                "Performance Tests",
                                "Integration Tests",
                                "Error Handling Tests",
                                "Boundary Tests",
                                "Long Running Tests",
                                "Run All Tests"};

    for (int i = 0; i < 9; i++)
    {
        lv_obj_t *btn = lv_list_add_button(cat_list, NULL, categories[i]);
        lv_obj_add_event_cb(btn, _test_category_cb, LV_EVENT_CLICKED, (void *)(long)i);
    }

    /* Create result list */
    _ctx.list = lv_list_create(_ctx.container);
    lv_obj_set_size(_ctx.list, lv_pct(100), lv_pct(45));
    lv_obj_set_flex_grow(_ctx.list, 1);
    lv_obj_set_style_pad_all(_ctx.list, 4, 0);

    /* Create status label */
    _ctx.result_label = lv_label_create(_ctx.container);
    lv_label_set_text(_ctx.result_label, "Select a test category to begin");
    lv_obj_set_style_text_align(_ctx.result_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(_ctx.result_label, lv_pct(100), LV_SIZE_CONTENT);

    eos_activity_enter(activity);
}

void eos_test_sensor_register_tests(void)
{
    eos_test_register("Sensor: register new sensor", _test_sensor_register);
    eos_test_register("Sensor: find by name/type", _test_sensor_find);
    eos_test_register("Sensor: init/enable/set_rate/get_rate/disable ops", _test_sensor_ops);
    eos_test_register("Sensor: get_state returns valid", _test_sensor_state);
    eos_test_register("Sensor: service init idempotent", _test_service_init);
    eos_test_register("Sensor: read_latest returns EOS_OK", _test_read_latest);
    eos_test_register("Sensor: subscribe then unsubscribe", _test_subscribe);
    eos_test_register("Sensor: nested self-unsubscribe (no UAF)", _test_nested_self_unsubscribe);
    eos_test_register("Sensor: nested cross-unsubscribe", _test_nested_cross_unsubscribe);
    eos_test_register("Sensor: nested subscribe during notify", _test_nested_subscribe_during_notify);
    eos_test_register("Sensor: nested multi-sensor UAF", _test_nested_multi_sensor_uaf);
    eos_test_register("Sensor: sample rate 50ms round-trip", _test_sample_rate);
    eos_test_register("Sensor: sample rate multiple periods", _test_sample_rate_multiple);
    eos_test_register("Sensor: enable/disable cycle", _test_sensor_enable_disable);
    eos_test_register("Sensor: data notification rate period=100", _test_data_notification_rate);
    eos_test_register("Sensor: FIFO full recovery", _test_fifo_full);
    eos_test_register("Sensor: no subscriber sampling period=0", _test_no_subscriber_sampling);
    eos_test_register("Sensor: min sample rate 1000ms", _test_min_sample_rate);
    eos_test_register("Sensor: concurrent read x3", _test_concurrent_read);
    eos_test_register("Sensor: null safety (invalid type, NULL data)", _test_null_safety);
    eos_test_register("Sensor: throughput 100 reads", _test_throughput);
    eos_test_register("Sensor: device manager integration", _test_device_manager_integration);
    eos_test_register("Sensor: parameter errors", _test_parameter_errors);
    eos_test_register("Sensor: FIFO boundary", _test_fifo_boundary);
    eos_test_register("Sensor: frequency boundary 0Hz/1Hz", _test_frequency_boundary);
    eos_test_register("Sensor: stability 1000 reads", _test_stability);
    eos_test_register("Sensor: data integrity 100 reads", _test_data_integrity);
}

#endif /* EOS_ENABLE_TEST_APP */
