/**
 * @file eos_test_event.c
 * @brief Comprehensive event system test module
 */

#include "eos_test_event.h"
#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_crown.h"
#include "eos_event.h"
#include "eos_log.h"
#include "eos_basic_widgets.h"
#include "eos_lang.h"
#include "lvgl.h"
#include "eos_error.h"
#include "eos_test_framework.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "EventTest"

/* Variables --------------------------------------------------*/
typedef struct {
    lv_obj_t *container;
    lv_obj_t *list;
    lv_obj_t *result_label;
    struct {
        uint32_t total_tests;
        uint32_t passed_tests;
        uint32_t failed_tests;
    } stats;
} _test_context_t;

static _test_context_t _ctx = {0};

static uint32_t _cb_call_count = 0;
static void *_cb_last_user_data = NULL;
static void *_cb_last_param = NULL;
static lv_obj_t *_cb_last_obj = NULL;
static eos_event_code_t _test_event_id = EOS_EVENT_UNKNOWN;

/* Function Implementations -----------------------------------*/
static void _update_result(const char *text)
{
    if (_ctx.result_label) {
        lv_label_set_text(_ctx.result_label, text);
    }
    EOS_LOG_I("%s", text);
}

static void _record_test(const char *name, bool passed, const char *details)
{
    _ctx.stats.total_tests++;
    if (passed) {
        _ctx.stats.passed_tests++;
    } else {
        _ctx.stats.failed_tests++;
    }

    eos_test_record(name, passed, details);

    if (_ctx.list) {
        char label_text[256];
        snprintf(label_text, sizeof(label_text),
                 "%s: %s",
                 name,
                 passed ? "PASS" : "FAIL");

        lv_obj_t *btn = lv_list_add_button(_ctx.list, NULL, label_text);

        if (!passed) {
            lv_obj_set_style_text_color(btn, lv_color_hex(0xFF0000), 0);
        }
    }
}

static void _test_event_cb(eos_event_t *e)
{
    _cb_call_count++;
    _cb_last_user_data = eos_event_get_user_data(e);
    _cb_last_param = eos_event_get_param(e);
    _cb_last_obj = eos_event_get_obj(e);
}

static void _test_event_cb2(eos_event_t *e)
{
    _cb_call_count++;
    (void)e;
}

static bool _test_event_register_id(void)
{
    _test_event_id = eos_event_register_id();

    bool passed = (_test_event_id >= EOS_EVENT_LAST);
    _record_test("Register Dynamic ID", passed,
                 passed ? "Dynamic event ID registered successfully" : "Failed to register dynamic ID");
    return passed;
}

static bool _test_event_subscribe_basic(void)
{
    _cb_call_count = 0;
    void *test_data = (void*)0x12345678;

    eos_event_subscribe(_test_event_id, _test_event_cb, test_data);
    eos_event_post(_test_event_id, NULL, NULL);

    bool passed = (_cb_call_count == 1);

    eos_event_unsubscribe(_test_event_id, _test_event_cb);
    _record_test("Basic Subscribe/Post", passed,
                 passed ? "Basic subscription works" : "Basic subscription failed");
    return passed;
}

static bool _test_event_subscribe_ex(void)
{
    _cb_call_count = 0;
    void *test_data = (void*)0xABCDEF01;
    lv_obj_t *test_obj = lv_obj_create(lv_screen_active());

    eos_event_subscribe_ex(_test_event_id, _test_event_cb, test_data, test_obj);
    eos_event_post(_test_event_id, NULL, test_obj);

    bool passed = (_cb_call_count == 1 && _cb_last_obj == test_obj);

    eos_event_unsubscribe_with_obj(_test_event_id, _test_event_cb, test_obj);
    lv_obj_delete(test_obj);
    _record_test("Extended Subscribe (with obj)", passed,
                 passed ? "Extended subscription works" : "Extended subscription failed");
    return passed;
}

static bool _test_event_callback_data(void)
{
    _cb_call_count = 0;
    _cb_last_user_data = NULL;
    _cb_last_param = NULL;

    void *user_data = (void*)0xDEADBEEF;
    void *param = (void*)0xCAFEBABE;

    eos_event_subscribe(_test_event_id, _test_event_cb, user_data);
    eos_event_post(_test_event_id, param, NULL);

    bool passed = (_cb_call_count == 1 &&
                   _cb_last_user_data == user_data &&
                   _cb_last_param == param);

    eos_event_unsubscribe(_test_event_id, _test_event_cb);
    _record_test("Callback Data Passing", passed,
                 passed ? "User data and param passed correctly" : "Data passing failed");
    return passed;
}

static bool _test_event_unsubscribe_basic(void)
{
    _cb_call_count = 0;

    eos_event_subscribe(_test_event_id, _test_event_cb, NULL);
    eos_event_post(_test_event_id, NULL, NULL);
    eos_event_unsubscribe(_test_event_id, _test_event_cb);
    eos_event_post(_test_event_id, NULL, NULL);

    bool passed = (_cb_call_count == 1);

    _record_test("Basic Unsubscribe", passed,
                 passed ? "Unsubscribe works correctly" : "Unsubscribe failed");
    return passed;
}

static bool _test_event_unsubscribe_with_user_data(void)
{
    _cb_call_count = 0;
    void *data1 = (void*)0x00000001;
    void *data2 = (void*)0x00000002;

    eos_event_subscribe(_test_event_id, _test_event_cb, data1);
    eos_event_subscribe(_test_event_id, _test_event_cb, data2);

    eos_event_post(_test_event_id, NULL, NULL);
    int count_after_first_post = _cb_call_count;

    eos_event_unsubscribe_with_user_data(_test_event_id, _test_event_cb, data1);
    eos_event_post(_test_event_id, NULL, NULL);

    bool passed = (count_after_first_post == 2 && _cb_call_count == 3);

    eos_event_unsubscribe_with_user_data(_test_event_id, _test_event_cb, data2);
    _record_test("Unsubscribe with User Data", passed,
                 passed ? "User data-based unsubscribe works" : "User data unsubscribe failed");
    return passed;
}

static bool _test_event_unsubscribe_all(void)
{
    _cb_call_count = 0;

    eos_event_subscribe(_test_event_id, _test_event_cb, NULL);
    eos_event_subscribe(_test_event_id, _test_event_cb2, NULL);
    eos_event_subscribe(EOS_EVENT_LANGUAGE_CHANGED, _test_event_cb, NULL);

    eos_event_post(_test_event_id, NULL, NULL);
    int count_before_unsubscribe = _cb_call_count;

    eos_event_unsubscribe_all(_test_event_cb);
    eos_event_post(_test_event_id, NULL, NULL);

    bool passed = (count_before_unsubscribe == 2 && _cb_call_count == 3);

    eos_event_unsubscribe_all(_test_event_cb2);
    _record_test("Unsubscribe All", passed,
                 passed ? "Global unsubscribe works" : "Global unsubscribe failed");
    return passed;
}

static bool _test_event_unsubscribe_with_obj(void)
{
    _cb_call_count = 0;
    lv_obj_t *obj1 = lv_obj_create(lv_screen_active());
    lv_obj_t *obj2 = lv_obj_create(lv_screen_active());

    eos_event_subscribe_ex(_test_event_id, _test_event_cb, NULL, obj1);
    eos_event_subscribe_ex(_test_event_id, _test_event_cb, NULL, obj2);

    eos_event_post(_test_event_id, NULL, NULL);
    int count_after_first_post = _cb_call_count;

    eos_event_unsubscribe_with_obj(_test_event_id, _test_event_cb, obj1);
    eos_event_post(_test_event_id, NULL, NULL);

    bool passed = (count_after_first_post == 2 && _cb_call_count == 3);

    eos_event_unsubscribe_with_obj(_test_event_id, _test_event_cb, obj2);
    lv_obj_delete(obj1);
    lv_obj_delete(obj2);
    _record_test("Unsubscribe with Obj", passed,
                 passed ? "Obj-based unsubscribe works" : "Obj unsubscribe failed");
    return passed;
}

static bool _test_event_internal_events(void)
{
    _cb_call_count = 0;

    eos_event_subscribe(EOS_EVENT_LANGUAGE_CHANGED, _test_event_cb, NULL);
    eos_event_post(EOS_EVENT_LANGUAGE_CHANGED, NULL, NULL);

    bool passed = (_cb_call_count == 1);

    eos_event_unsubscribe(EOS_EVENT_LANGUAGE_CHANGED, _test_event_cb);
    _record_test("Internal Events", passed,
                 passed ? "Internal events work" : "Internal events failed");
    return passed;
}

static bool _test_event_multiple_subscribers(void)
{
    _cb_call_count = 0;

    eos_event_subscribe(_test_event_id, _test_event_cb, NULL);
    eos_event_subscribe(_test_event_id, _test_event_cb2, NULL);

    eos_event_post(_test_event_id, NULL, NULL);

    bool passed = (_cb_call_count == 2);

    eos_event_unsubscribe(_test_event_id, _test_event_cb);
    eos_event_unsubscribe(_test_event_id, _test_event_cb2);
    _record_test("Multiple Subscribers", passed,
                 passed ? "Multiple subscribers work correctly" : "Multiple subscribers failed");
    return passed;
}

static bool _test_event_cleanup(void)
{
    eos_event_subscribe(_test_event_id, _test_event_cb, NULL);
    eos_event_unsubscribe(_test_event_id, _test_event_cb);

    eos_event_cleanup_now();

    _record_test("Event Cleanup", true, "Cleanup executed without error");
    return true;
}

static bool _test_event_null_safety(void)
{
    bool passed = true;

    /* These should not crash */
    eos_event_subscribe(EOS_EVENT_UNKNOWN, NULL, NULL);
    eos_event_unsubscribe(EOS_EVENT_UNKNOWN, NULL);
    eos_event_post(EOS_EVENT_UNKNOWN, NULL, NULL);
    eos_event_unsubscribe_all(NULL);

    _record_test("Null Safety", passed,
                 passed ? "Null parameters handled safely" : "Null safety failed");
    return passed;
}

static bool _test_event_obj_payload(void)
{
    _cb_call_count = 0;
    lv_obj_t *test_obj = lv_obj_create(lv_screen_active());

    eos_event_subscribe_ex(_test_event_id, _test_event_cb, NULL, test_obj);
    eos_event_post(_test_event_id, NULL, test_obj);

    bool passed = (_cb_last_obj == test_obj);

    eos_event_unsubscribe_with_obj(_test_event_id, _test_event_cb, test_obj);
    lv_obj_delete(test_obj);
    _record_test("Obj Payload", passed,
                 passed ? "Object payload passed correctly" : "Object payload failed");
    return passed;
}

static void _run_basic_tests(void)
{
    _update_result("Running Basic Event Tests...");

    _test_event_register_id();
    _test_event_subscribe_basic();
    _test_event_subscribe_ex();
    _test_event_callback_data();
}

static void _run_unsubscribe_tests(void)
{
    _update_result("Running Unsubscribe Tests...");

    _test_event_unsubscribe_basic();
    _test_event_unsubscribe_with_user_data();
    _test_event_unsubscribe_with_obj();
    _test_event_unsubscribe_all();
}

static void _run_advanced_tests(void)
{
    _update_result("Running Advanced Tests...");

    _test_event_internal_events();
    _test_event_multiple_subscribers();
    _test_event_cleanup();
    _test_event_null_safety();
    _test_event_obj_payload();
}

static void _test_category_cb(lv_event_t *e)
{
    int category = (int)(long)lv_event_get_user_data(e);

    memset(&_ctx.stats, 0, sizeof(_ctx.stats));
    _cb_call_count = 0;
    _cb_last_user_data = NULL;
    _cb_last_param = NULL;
    _cb_last_obj = NULL;

    lv_obj_clean(_ctx.list);

    switch (category) {
        case 0:
            _run_basic_tests();
            break;
        case 1:
            _run_unsubscribe_tests();
            break;
        case 2:
            _run_advanced_tests();
            break;
        case 3:
            _run_basic_tests();
            _run_unsubscribe_tests();
            _run_advanced_tests();
            break;
        default:
            break;
    }

    char summary[256];
    snprintf(summary, sizeof(summary),
             "Total: %u | Pass: %u | Fail: %u",
             _ctx.stats.total_tests,
             _ctx.stats.passed_tests,
             _ctx.stats.failed_tests);
    _update_result(summary);
}

static eos_activity_lifecycle_t s_event_test_activity_lifecycle;

void eos_test_event_start(void)
{
    eos_activity_t *activity = eos_activity_create(&s_event_test_activity_lifecycle);
    if (!activity) {
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view) {
        return;
    }

    eos_activity_set_title(activity, "Event Tests");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    _ctx.container = lv_obj_create(view);
    lv_obj_set_size(_ctx.container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(_ctx.container, 8, 0);
    lv_obj_set_flex_flow(_ctx.container, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *cat_list = lv_list_create(_ctx.container);
    lv_obj_set_size(cat_list, lv_pct(100), lv_pct(35));
    lv_obj_set_flex_grow(cat_list, 1);
    eos_crown_encoder_set_target_obj(cat_list);

    const char *categories[] = {
        "Basic Tests",
        "Unsubscribe Tests",
        "Advanced Tests",
        "Run All Tests"
    };

    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_list_add_button(cat_list, NULL, categories[i]);
        lv_obj_add_event_cb(btn, _test_category_cb, LV_EVENT_CLICKED, (void*)(long)i);
    }

    _ctx.list = lv_list_create(_ctx.container);
    lv_obj_set_size(_ctx.list, lv_pct(100), lv_pct(50));
    lv_obj_set_flex_grow(_ctx.list, 1);
    lv_obj_set_style_pad_all(_ctx.list, 4, 0);

    _ctx.result_label = lv_label_create(_ctx.container);
    lv_label_set_text(_ctx.result_label, "Select a test category to begin");
    lv_obj_set_style_text_align(_ctx.result_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(_ctx.result_label, lv_pct(100), LV_SIZE_CONTENT);

    eos_activity_enter(activity);
}

void eos_test_event_register_tests(void)
{
    eos_test_register("Event: register dynamic ID", _test_event_register_id);
    eos_test_register("Event: basic subscribe/post", _test_event_subscribe_basic);
    eos_test_register("Event: subscribe with obj", _test_event_subscribe_ex);
    eos_test_register("Event: callback data passing", _test_event_callback_data);
    eos_test_register("Event: basic unsubscribe", _test_event_unsubscribe_basic);
    eos_test_register("Event: unsubscribe with user data", _test_event_unsubscribe_with_user_data);
    eos_test_register("Event: unsubscribe all", _test_event_unsubscribe_all);
    eos_test_register("Event: unsubscribe with obj", _test_event_unsubscribe_with_obj);
    eos_test_register("Event: internal events", _test_event_internal_events);
    eos_test_register("Event: multiple subscribers", _test_event_multiple_subscribers);
    eos_test_register("Event: cleanup no crash", _test_event_cleanup);
    eos_test_register("Event: null safety", _test_event_null_safety);
    eos_test_register("Event: obj payload", _test_event_obj_payload);
}

#endif /* EOS_ENABLE_TEST_APP */
