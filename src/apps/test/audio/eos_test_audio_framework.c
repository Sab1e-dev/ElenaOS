#include "eos_test_audio_framework.h"
#if EOS_ENABLE_TEST_APP

#include <string.h>
#include <stdio.h>
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_crown.h"
#include "eos_log.h"
#include "eos_basic_widgets.h"
#include "lvgl.h"

#define EOS_LOG_TAG "AudioTestFW"

typedef struct {
    char name[EOS_TEST_NAME_MAX];
    eos_test_fn_t fn;
    bool has_run;
    bool passed;
} eos_test_entry_t;

static eos_test_entry_t s_entries[EOS_TEST_MAX];
static int s_count = 0;
static lv_obj_t *s_summary_label = NULL;
static lv_obj_t *s_run_all_btn = NULL;
static int s_passed = 0;
static int s_failed = 0;
static int s_total = 0;
static bool s_is_running = false;

static void _update_summary(void)
{
    if (!s_summary_label) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "Pass: %d/%d  Fail: %d",
             s_passed, s_total, s_failed);
    lv_label_set_text(s_summary_label, buf);
}

static void _run_all_cb(lv_event_t *e)
{
    (void)e;
    eos_test_run_all();
}

void eos_test_register(const char *name, eos_test_fn_t fn)
{
    if (!name || !fn) return;
    if (s_count >= EOS_TEST_MAX) return;
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_entries[i].name, name) == 0)
            return;
    }
    strncpy(s_entries[s_count].name, name, EOS_TEST_NAME_MAX - 1);
    s_entries[s_count].name[EOS_TEST_NAME_MAX - 1] = '\0';
    s_entries[s_count].fn = fn;
    s_entries[s_count].has_run = false;
    s_entries[s_count].passed = false;
    s_count++;
}

void eos_test_record(const char *name, bool passed, const char *detail)
{
    EOS_LOG_I("%s: %s (%s)", name, passed ? "PASS" : "FAIL", detail);
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_entries[i].name, name) == 0) {
            if (!s_entries[i].has_run) {
                s_entries[i].has_run = true;
                s_entries[i].passed = passed;
                if (passed) s_passed++; else s_failed++;
                s_total++;
            }
            _update_summary();
            return;
        }
    }
    EOS_LOG_W("Test not registered: %s", name);
}

void eos_test_reset(void)
{
    s_passed = 0;
    s_failed = 0;
    s_total = 0;
    for (int i = 0; i < s_count; i++) {
        s_entries[i].has_run = false;
        s_entries[i].passed = false;
    }
    if (s_summary_label)
        lv_label_set_text(s_summary_label, "Running...");
}

void eos_test_run_all(void)
{
    if (s_is_running) return;
    s_is_running = true;
    eos_test_reset();
    if (s_run_all_btn) lv_obj_add_state(s_run_all_btn, LV_STATE_DISABLED);
    for (int i = 0; i < s_count; i++) {
        s_entries[i].fn();
    }
    if (s_run_all_btn) lv_obj_clear_state(s_run_all_btn, LV_STATE_DISABLED);
    s_is_running = false;
}

uint32_t eos_test_get_total(void) { return (uint32_t)s_total; }
uint32_t eos_test_get_passed(void) { return (uint32_t)s_passed; }
uint32_t eos_test_get_failed(void) { return (uint32_t)s_failed; }

static void _fw_on_destroy(eos_activity_t *activity)
{
    LV_UNUSED(activity);
    s_summary_label = NULL;
    s_run_all_btn = NULL;
}

static eos_activity_lifecycle_t s_fw_lifecycle = {
    .on_enter = NULL,
    .on_destroy = _fw_on_destroy,
    .on_pause = NULL,
    .on_resume = NULL,
};

void eos_test_audio_page_start(void)
{
    eos_activity_t *activity = eos_activity_create(&s_fw_lifecycle);
    if (!activity) return;

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view) return;

    eos_activity_set_title(activity, "Audio Tests");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    lv_obj_t *cont = lv_obj_create(view);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(cont, 8, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

    s_run_all_btn = lv_btn_create(cont);
    lv_obj_set_size(s_run_all_btn, lv_pct(100), 40);
    lv_obj_set_flex_grow(s_run_all_btn, 1);
    lv_obj_t *run_label = lv_label_create(s_run_all_btn);
    lv_label_set_text(run_label, LV_SYMBOL_PLAY " Run All Tests");
    lv_obj_center(run_label);
    lv_obj_add_event_cb(s_run_all_btn, _run_all_cb, LV_EVENT_CLICKED, NULL);

    s_summary_label = lv_label_create(cont);
    lv_label_set_text(s_summary_label, "Ready");
    lv_obj_set_style_text_align(s_summary_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_summary_label, lv_pct(100));
    lv_obj_set_flex_grow(s_summary_label, 1);

    eos_crown_encoder_set_target_obj(cont);
    eos_activity_enter(activity);
}

#endif /* EOS_ENABLE_TEST_APP */
