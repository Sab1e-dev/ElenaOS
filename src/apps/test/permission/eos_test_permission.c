/**
 * @file eos_test_permission.c
 * @brief Comprehensive permission system test module
 */

#include "eos_test_permission.h"
#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_crown.h"
#include "eos_service_permission.h"
#include "eos_log.h"
#include "eos_basic_widgets.h"
#include "eos_lang.h"
#include "lvgl.h"
#include "eos_test_framework.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "PermTest"
#define TEST_APP_ID_1 "com.test.app1"
#define TEST_APP_ID_2 "com.test.app2"

/* Variables --------------------------------------------------*/
typedef struct
{
    lv_obj_t *container;
    lv_obj_t *list;
    lv_obj_t *result_label;
    struct
    {
        uint32_t total_tests;
        uint32_t passed_tests;
        uint32_t failed_tests;
    } stats;
} _test_context_t;

static _test_context_t _ctx = {0};

/* Function Implementations -----------------------------------*/
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

/* Test Cases -------------------------------------------------*/

/**
 * @brief Test: Default permission state is DENIED for new apps
 */
static bool _test_default_state_denied(void)
{
    /* Use a unique app ID to avoid conflicts with existing data */
    const char *app_id = "com.test.unique.default";

    /* Get permission state before setting anything */
    eos_perm_state_t state = eos_permission_get(app_id, EOS_PERM_CATEGORY_LOCATION);

    bool passed = (state == EOS_PERM_STATE_DENIED);
    _record_test("Default State is DENIED",
                 passed,
                 passed ? "New app permissions default to DENIED" : "Default state incorrect");
    return passed;
}

/**
 * @brief Test: Set and get permission state
 */
static bool _test_set_and_get(void)
{
    const char *app_id = TEST_APP_ID_1;

    /* Set location permission to ALLOW_ALWAYS */
    bool set_ok = eos_permission_set(app_id, EOS_PERM_CATEGORY_LOCATION, EOS_PERM_STATE_ALLOW_ALWAYS);
    if (!set_ok)
    {
        _record_test("Set Permission", false, "Failed to set permission");
        return false;
    }

    /* Read it back */
    eos_perm_state_t state = eos_permission_get(app_id, EOS_PERM_CATEGORY_LOCATION);
    bool passed = (state == EOS_PERM_STATE_ALLOW_ALWAYS);
    _record_test("Set and Get Permission",
                 passed,
                 passed ? "Permission state persisted correctly" : "State mismatch after set");
    return passed;
}

/**
 * @brief Test: All permission states can be set and retrieved
 */
static bool _test_all_states(void)
{
    const char *app_id = "com.test.states";
    bool all_passed = true;

    eos_perm_state_t test_states[] = {EOS_PERM_STATE_DENIED,
                                      EOS_PERM_STATE_ALLOW_ONCE,
                                      EOS_PERM_STATE_ALLOW_FOREGROUND,
                                      EOS_PERM_STATE_ALLOW_ALWAYS};

    const char *state_names[] = {"DENIED", "ALLOW_ONCE", "ALLOW_FOREGROUND", "ALLOW_ALWAYS"};

    for (int i = 0; i < 4; i++)
    {
        bool ok = eos_permission_set(app_id, EOS_PERM_CATEGORY_SENSOR, test_states[i]);
        if (!ok)
        {
            all_passed = false;
            break;
        }

        eos_perm_state_t read_back = eos_permission_get(app_id, EOS_PERM_CATEGORY_SENSOR);
        if (read_back != test_states[i])
        {
            all_passed = false;
            break;
        }
    }

    char details[128];
    snprintf(details,
             sizeof(details),
             "%s",
             all_passed ? "All 4 states can be set and read back correctly"
                        : "State round-trip failed for one or more states");
    _record_test("All Grant States", all_passed, details);
    return all_passed;
}

/**
 * @brief Test: Different apps have isolated permission stores
 */
static bool _test_app_isolation(void)
{
    /* Set permission for app1 */
    eos_permission_set(TEST_APP_ID_1, EOS_PERM_CATEGORY_NOTIFICATION, EOS_PERM_STATE_ALLOW_ALWAYS);

    /* App2 should still have default DENIED */
    eos_perm_state_t app2_state = eos_permission_get(TEST_APP_ID_2, EOS_PERM_CATEGORY_NOTIFICATION);

    bool passed = (app2_state == EOS_PERM_STATE_DENIED);
    _record_test("App Isolation",
                 passed,
                 passed ? "Different apps have independent permission stores" : "App isolation violated");
    return passed;
}

/**
 * @brief Test: Same app can have different permissions per category
 */
static bool _test_category_independence(void)
{
    const char *app_id = "com.test.categories";

    eos_permission_set(app_id, EOS_PERM_CATEGORY_LOCATION, EOS_PERM_STATE_ALLOW_ALWAYS);
    eos_permission_set(app_id, EOS_PERM_CATEGORY_STORAGE, EOS_PERM_STATE_DENIED);
    eos_permission_set(app_id, EOS_PERM_CATEGORY_AUDIO, EOS_PERM_STATE_ALLOW_FOREGROUND);

    eos_perm_state_t loc = eos_permission_get(app_id, EOS_PERM_CATEGORY_LOCATION);
    eos_perm_state_t sto = eos_permission_get(app_id, EOS_PERM_CATEGORY_STORAGE);
    eos_perm_state_t aud = eos_permission_get(app_id, EOS_PERM_CATEGORY_AUDIO);

    bool passed =
        (loc == EOS_PERM_STATE_ALLOW_ALWAYS && sto == EOS_PERM_STATE_DENIED && aud == EOS_PERM_STATE_ALLOW_FOREGROUND);
    _record_test("Category Independence",
                 passed,
                 passed ? "Each category has independent state" : "Categories not independent");
    return passed;
}

/**
 * @brief Test: Revoke all permissions for an app
 */
static bool _test_revoke_all(void)
{
    const char *app_id = "com.test.revoke";

    /* Set multiple permissions */
    eos_permission_set(app_id, EOS_PERM_CATEGORY_LOCATION, EOS_PERM_STATE_ALLOW_ALWAYS);
    eos_permission_set(app_id, EOS_PERM_CATEGORY_BLUETOOTH, EOS_PERM_STATE_ALLOW_FOREGROUND);
    eos_permission_set(app_id, EOS_PERM_CATEGORY_CONTACTS, EOS_PERM_STATE_ALLOW_ONCE);

    /* Verify they were set */
    if (eos_permission_get(app_id, EOS_PERM_CATEGORY_LOCATION) != EOS_PERM_STATE_ALLOW_ALWAYS
        || eos_permission_get(app_id, EOS_PERM_CATEGORY_BLUETOOTH) != EOS_PERM_STATE_ALLOW_FOREGROUND
        || eos_permission_get(app_id, EOS_PERM_CATEGORY_CONTACTS) != EOS_PERM_STATE_ALLOW_ONCE)
    {
        _record_test("Revoke All Permissions", false, "Pre-condition: failed to set permissions");
        return false;
    }

    /* Revoke all */
    eos_permission_revoke_all(app_id);

    /* Verify all are now DENIED */
    bool loc_revoked = (eos_permission_get(app_id, EOS_PERM_CATEGORY_LOCATION) == EOS_PERM_STATE_DENIED);
    bool bt_revoked = (eos_permission_get(app_id, EOS_PERM_CATEGORY_BLUETOOTH) == EOS_PERM_STATE_DENIED);
    bool contacts_revoked = (eos_permission_get(app_id, EOS_PERM_CATEGORY_CONTACTS) == EOS_PERM_STATE_DENIED);

    bool passed = (loc_revoked && bt_revoked && contacts_revoked);
    _record_test("Revoke All Permissions",
                 passed,
                 passed ? "All permissions revoked successfully" : "Some permissions survived revocation");
    return passed;
}

/**
 * @brief Test: Permission name string to category enum conversion
 */
static bool _test_name_to_category(void)
{
    bool all_passed = true;

    struct
    {
        const char *name;
        eos_perm_category_t expected;
    } cases[] = {
        {"location", EOS_PERM_CATEGORY_LOCATION},
        {"sensor", EOS_PERM_CATEGORY_SENSOR},
        {"notification", EOS_PERM_CATEGORY_NOTIFICATION},
        {"storage", EOS_PERM_CATEGORY_STORAGE},
        {"bluetooth", EOS_PERM_CATEGORY_BLUETOOTH},
        {"audio", EOS_PERM_CATEGORY_AUDIO},
        {"health", EOS_PERM_CATEGORY_HEALTH},
        {"contacts", EOS_PERM_CATEGORY_CONTACTS},
        {"calendar", EOS_PERM_CATEGORY_CALENDAR},
    };

    int num_cases = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < num_cases; i++)
    {
        eos_perm_category_t result = eos_permission_name_to_category(cases[i].name);
        if (result != cases[i].expected)
        {
            all_passed = false;
            break;
        }
    }

    /* Also test unknown name returns COUNT */
    eos_perm_category_t unknown = eos_permission_name_to_category("nonexistent_perm");
    if (unknown != EOS_PERM_CATEGORY_COUNT)
    {
        all_passed = false;
    }

    char details[128];
    snprintf(details,
             sizeof(details),
             "%s",
             all_passed ? "All known names mapped correctly, unknown returns COUNT" : "Name mapping failed");
    _record_test("Name to Category Conversion", all_passed, details);
    return all_passed;
}

/**
 * @brief Test: Category key retrieval
 */
static bool _test_category_key(void)
{
    bool all_passed = true;

    struct
    {
        eos_perm_category_t cat;
        const char *expected_key;
    } cases[] = {
        {EOS_PERM_CATEGORY_LOCATION, "location"},
        {EOS_PERM_CATEGORY_SENSOR, "sensor"},
        {EOS_PERM_CATEGORY_NOTIFICATION, "notification"},
        {EOS_PERM_CATEGORY_STORAGE, "storage"},
        {EOS_PERM_CATEGORY_BLUETOOTH, "bluetooth"},
        {EOS_PERM_CATEGORY_AUDIO, "audio"},
        {EOS_PERM_CATEGORY_HEALTH, "health"},
        {EOS_PERM_CATEGORY_CONTACTS, "contacts"},
        {EOS_PERM_CATEGORY_CALENDAR, "calendar"},
    };

    int num_cases = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < num_cases; i++)
    {
        const char *key = eos_permission_category_key(cases[i].cat);
        if (!key || strcmp(key, cases[i].expected_key) != 0)
        {
            all_passed = false;
            break;
        }
    }

    /* Invalid category should return NULL */
    const char *invalid_key = eos_permission_category_key(EOS_PERM_CATEGORY_COUNT);
    if (invalid_key != NULL)
    {
        all_passed = false;
    }

    char details[128];
    snprintf(details,
             sizeof(details),
             "%s",
             all_passed ? "All category keys correct, invalid returns NULL" : "Category key mismatch");
    _record_test("Category Key Retrieval", all_passed, details);
    return all_passed;
}

/**
 * @brief Test: Null/invalid parameter handling
 */
static bool _test_null_safety(void)
{
    bool all_passed = true;

    /* Get with NULL app_id should return DENIED */
    eos_perm_state_t null_app = eos_permission_get(NULL, EOS_PERM_CATEGORY_LOCATION);
    if (null_app != EOS_PERM_STATE_DENIED)
    {
        all_passed = false;
    }

    /* Get with invalid category should return DENIED */
    eos_perm_state_t invalid_cat = eos_permission_get(TEST_APP_ID_1, EOS_PERM_CATEGORY_COUNT);
    if (invalid_cat != EOS_PERM_STATE_DENIED)
    {
        all_passed = false;
    }

    /* Set with NULL app_id should return false */
    bool null_set = eos_permission_set(NULL, EOS_PERM_CATEGORY_LOCATION, EOS_PERM_STATE_ALLOW_ALWAYS);
    if (null_set != false)
    {
        all_passed = false;
    }

    /* Set with invalid category should return false */
    bool invalid_cat_set = eos_permission_set(TEST_APP_ID_1, EOS_PERM_CATEGORY_COUNT, EOS_PERM_STATE_ALLOW_ALWAYS);
    if (invalid_cat_set != false)
    {
        all_passed = false;
    }

    /* Set with invalid state should return false */
    bool invalid_state_set = eos_permission_set(TEST_APP_ID_1, EOS_PERM_CATEGORY_LOCATION, (eos_perm_state_t)99);
    if (invalid_state_set != false)
    {
        all_passed = false;
    }

    /* Revoke with NULL should not crash */
    eos_permission_revoke_all(NULL);

    /* Name conversion with NULL should return COUNT */
    eos_perm_category_t null_name = eos_permission_name_to_category(NULL);
    if (null_name != EOS_PERM_CATEGORY_COUNT)
    {
        all_passed = false;
    }

    char details[128];
    snprintf(details,
             sizeof(details),
             "%s",
             all_passed ? "All invalid parameters handled safely" : "Null safety violation detected");
    _record_test("Null Safety / Edge Cases", all_passed, details);
    return all_passed;
}

/**
 * @brief Test: Overwrite existing permission state
 */
static bool _test_state_overwrite(void)
{
    const char *app_id = "com.test.overwrite";

    /* Set initial state */
    eos_permission_set(app_id, EOS_PERM_CATEGORY_HEALTH, EOS_PERM_STATE_ALLOW_ALWAYS);

    /* Overwrite with different state */
    eos_permission_set(app_id, EOS_PERM_CATEGORY_HEALTH, EOS_PERM_STATE_DENIED);

    eos_perm_state_t final = eos_permission_get(app_id, EOS_PERM_CATEGORY_HEALTH);
    bool passed = (final == EOS_PERM_STATE_DENIED);
    _record_test("State Overwrite", passed, passed ? "Permission state can be overwritten" : "Overwrite failed");
    return passed;
}

/**
 * @brief Test: All 9 categories are valid
 */
static bool _test_all_categories_valid(void)
{
    bool all_passed = true;

    for (int i = 0; i < EOS_PERM_CATEGORY_COUNT; i++)
    {
        const char *key = eos_permission_category_key((eos_perm_category_t)i);
        const char *name = eos_permission_category_name((eos_perm_category_t)i);
        const char *desc = eos_permission_category_desc((eos_perm_category_t)i);

        if (!key || !name || !desc)
        {
            all_passed = false;
            break;
        }
    }

    char details[128];
    snprintf(details,
             sizeof(details),
             "Checked %d categories%s",
             EOS_PERM_CATEGORY_COUNT,
             all_passed ? ", all valid" : ", some invalid");
    _record_test("All Categories Valid", all_passed, details);
    return all_passed;
}

/**
 * @brief Test: Revoke non-existent app does not crash
 */
static bool _test_revoke_nonexistent(void)
{
    /* Should not crash */
    eos_permission_revoke_all("com.test.does.not.exist");

    _record_test("Revoke Non-existent App", true, "Revoking non-existent app handled gracefully");
    return true;
}

/* Test Runners -----------------------------------------------*/

static void _run_basic_tests(void)
{
    _update_result("Running Basic Permission Tests...");

    _test_default_state_denied();
    _test_set_and_get();
    _test_all_states();
}

static void _run_isolation_tests(void)
{
    _update_result("Running Isolation Tests...");

    _test_app_isolation();
    _test_category_independence();
}

static void _run_advanced_tests(void)
{
    _update_result("Running Advanced Tests...");

    _test_revoke_all();
    _test_name_to_category();
    _test_category_key();
    _test_null_safety();
    _test_state_overwrite();
    _test_all_categories_valid();
    _test_revoke_nonexistent();
}

static void _cleanup_test_data(void)
{
    /* Clean up test artifacts so tests are repeatable */
    eos_permission_revoke_all(TEST_APP_ID_1);
    eos_permission_revoke_all(TEST_APP_ID_2);
    eos_permission_revoke_all("com.test.unique.default");
    eos_permission_revoke_all("com.test.states");
    eos_permission_revoke_all("com.test.categories");
    eos_permission_revoke_all("com.test.revoke");
    eos_permission_revoke_all("com.test.overwrite");
}

/* UI Callbacks -----------------------------------------------*/

static void _test_category_cb(lv_event_t *e)
{
    int category = (int)(long)lv_event_get_user_data(e);

    memset(&_ctx.stats, 0, sizeof(_ctx.stats));

    lv_obj_clean(_ctx.list);

    switch (category)
    {
        case 0:
            _run_basic_tests();
            break;
        case 1:
            _run_isolation_tests();
            break;
        case 2:
            _run_advanced_tests();
            break;
        case 3:
            _run_basic_tests();
            _run_isolation_tests();
            _run_advanced_tests();
            break;
        default:
            break;
    }

    /* Cleanup test data */
    _cleanup_test_data();

    char summary[256];
    snprintf(summary,
             sizeof(summary),
             "Total: %u | Pass: %u | Fail: %u",
             _ctx.stats.total_tests,
             _ctx.stats.passed_tests,
             _ctx.stats.failed_tests);
    _update_result(summary);
}

/* Activity Entry Point ---------------------------------------*/

static eos_activity_lifecycle_t s_perm_test_activity_lifecycle = {
    .on_enter = NULL,
    .on_destroy = NULL,
    .on_pause = NULL,
    .on_resume = NULL,
};

void eos_test_permission_start(void)
{
    eos_activity_t *activity = eos_activity_create(&s_perm_test_activity_lifecycle);
    if (!activity)
    {
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return;
    }

    eos_activity_set_title(activity, "Permission Tests");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    _ctx.container = lv_obj_create(view);
    lv_obj_set_size(_ctx.container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(_ctx.container, 8, 0);
    lv_obj_set_flex_flow(_ctx.container, LV_FLEX_FLOW_COLUMN);

    /* Category selection list */
    lv_obj_t *cat_list = lv_list_create(_ctx.container);
    lv_obj_set_size(cat_list, lv_pct(100), lv_pct(35));
    lv_obj_set_flex_grow(cat_list, 1);
    eos_crown_encoder_set_target_obj(cat_list);

    const char *categories[] = {"Basic Tests", "Isolation Tests", "Advanced Tests", "Run All Tests"};

    for (int i = 0; i < 4; i++)
    {
        lv_obj_t *btn = lv_list_add_button(cat_list, NULL, categories[i]);
        lv_obj_add_event_cb(btn, _test_category_cb, LV_EVENT_CLICKED, (void *)(long)i);
    }

    /* Results list */
    _ctx.list = lv_list_create(_ctx.container);
    lv_obj_set_size(_ctx.list, lv_pct(100), lv_pct(50));
    lv_obj_set_flex_grow(_ctx.list, 1);
    lv_obj_set_style_pad_all(_ctx.list, 4, 0);

    /* Summary label */
    _ctx.result_label = lv_label_create(_ctx.container);
    lv_label_set_text(_ctx.result_label, "Select a test category to begin");
    lv_obj_set_style_text_align(_ctx.result_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(_ctx.result_label, lv_pct(100), LV_SIZE_CONTENT);

    eos_activity_enter(activity);
}

void eos_test_permission_register_tests(void)
{
    eos_test_register("Permission: default state DENIED", _test_default_state_denied);
    eos_test_register("Permission: set and get", _test_set_and_get);
    eos_test_register("Permission: all grant states", _test_all_states);
    eos_test_register("Permission: app isolation", _test_app_isolation);
    eos_test_register("Permission: category independence", _test_category_independence);
    eos_test_register("Permission: revoke all", _test_revoke_all);
    eos_test_register("Permission: name to category", _test_name_to_category);
    eos_test_register("Permission: category key", _test_category_key);
    eos_test_register("Permission: null safety", _test_null_safety);
    eos_test_register("Permission: state overwrite", _test_state_overwrite);
    eos_test_register("Permission: all categories valid", _test_all_categories_valid);
    eos_test_register("Permission: revoke nonexistent", _test_revoke_nonexistent);
}

#endif /* EOS_ENABLE_TEST_APP */
