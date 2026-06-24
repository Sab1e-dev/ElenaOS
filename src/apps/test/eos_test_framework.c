/**
 * @file eos_test_framework.c
 * @brief Unit test framework implementation
 */

#include "eos_test_framework.h"
#if EOS_ENABLE_TEST_APP

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_crown.h"
#include "eos_log.h"
#include "eos_basic_widgets.h"
#include "eos_theme.h"
#include "lvgl.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "TestFW"
#define EOS_TEST_GROUP_MAX 32
#define EOS_TEST_GROUP_NAME_MAX 32

/* Variables --------------------------------------------------*/

typedef struct {
    char name[EOS_TEST_NAME_MAX];
    eos_test_fn_t fn;
    bool has_run;
    bool passed;
    bool selected;
} eos_test_entry_t;

typedef struct {
    char name[EOS_TEST_GROUP_NAME_MAX];
    int start_idx;
    int count;
} eos_test_group_t;

static eos_test_entry_t s_entries[EOS_TEST_MAX];
static int s_count = 0;
static eos_test_group_t s_groups[EOS_TEST_GROUP_MAX];
static int s_group_count = 0;

static lv_obj_t *s_info_label = NULL;
static lv_obj_t *s_current_label = NULL;
static lv_obj_t *s_summary_label = NULL;
static lv_obj_t *s_progress_bar = NULL;
static lv_obj_t *s_run_btn = NULL;
static lv_obj_t *s_run_btn_label = NULL;
static lv_obj_t *s_status_icon = NULL;
static lv_obj_t *s_select_all_btn = NULL;
static lv_obj_t *s_select_all_label = NULL;
static lv_obj_t *s_checklist = NULL;
static lv_obj_t *s_checkboxes[EOS_TEST_MAX];
static lv_obj_t *s_group_headers[EOS_TEST_GROUP_MAX];
static lv_obj_t *s_group_header_labels[EOS_TEST_GROUP_MAX];
static lv_obj_t *s_group_checkboxes[EOS_TEST_GROUP_MAX];
static bool s_group_expanded[EOS_TEST_GROUP_MAX];
static bool s_all_selected = true;

static int s_passed = 0;
static int s_failed = 0;
static int s_total = 0;
static bool s_is_running = false;

/* Function Implementations -----------------------------------*/

static void _force_refresh(void)
{
    lv_timer_handler();
}

static int _selected_count(void)
{
    int n = 0;
    for (int i = 0; i < s_count; i++)
        if (s_entries[i].selected) n++;
    return n;
}

static void _sync_progress_range(void)
{
    if (!s_progress_bar) return;
    int sel = _selected_count();
    lv_bar_set_range(s_progress_bar, 0, sel > 0 ? sel : 1);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
}

static void _update_summary(void)
{
    if (!s_summary_label) return;
    int sel = _selected_count();
    char buf[64];
    snprintf(buf, sizeof(buf), "Pass: %d/%d  Fail: %d", s_passed, sel, s_failed);
    lv_label_set_text(s_summary_label, buf);
}

static void _update_current(const char *status)
{
    if (!s_current_label) return;
    lv_label_set_text(s_current_label, status);
}

static void _update_progress(void)
{
    if (!s_progress_bar) return;
    lv_bar_set_value(s_progress_bar, s_total, LV_ANIM_OFF);
}

static void _set_status_icon(const char *symbol, lv_color_t color)
{
    if (!s_status_icon) return;
    lv_label_set_text(s_status_icon, symbol);
    lv_obj_set_style_text_color(s_status_icon, color, 0);
}

static void _update_info_label(void)
{
    if (!s_info_label) return;
    int sel = _selected_count();
    char buf[40];
    if (sel == s_count)
        snprintf(buf, sizeof(buf), "%d tests loaded", s_count);
    else
        snprintf(buf, sizeof(buf), "%d loaded, %d selected", s_count, sel);
    lv_label_set_text(s_info_label, buf);
}

static void _sync_group_ui(int group_idx)
{
    if (group_idx < 0 || group_idx >= s_group_count) return;
    eos_test_group_t *g = &s_groups[group_idx];

    int all_sel = 0;
    for (int i = 0; i < g->count; i++)
        if (s_entries[g->start_idx + i].selected) all_sel++;

    if (s_group_checkboxes[group_idx]) {
        if (all_sel == g->count)
            lv_obj_add_state(s_group_checkboxes[group_idx], LV_STATE_CHECKED);
        else
            lv_obj_remove_state(s_group_checkboxes[group_idx], LV_STATE_CHECKED);
    }

    if (s_group_header_labels[group_idx]) {
        char buf[EOS_TEST_GROUP_NAME_MAX + 20];
        snprintf(buf, sizeof(buf), "%s %s (%d)",
                 s_group_expanded[group_idx] ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT,
                 g->name, g->count);
        lv_label_set_text(s_group_header_labels[group_idx], buf);
    }
}

static void _set_group_visible(int group_idx, bool visible)
{
    eos_test_group_t *g = &s_groups[group_idx];
    for (int i = 0; i < g->count; i++) {
        int idx = g->start_idx + i;
        if (s_checkboxes[idx]) {
            if (visible)
                lv_obj_remove_flag(s_checkboxes[idx], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(s_checkboxes[idx], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void _toggle_group_select(int group_idx)
{
    if (group_idx < 0 || group_idx >= s_group_count) return;
    eos_test_group_t *g = &s_groups[group_idx];

    int all_sel = 0;
    for (int i = 0; i < g->count; i++)
        if (s_entries[g->start_idx + i].selected) all_sel++;

    bool new_state = (all_sel < g->count);
    for (int i = 0; i < g->count; i++) {
        int idx = g->start_idx + i;
        s_entries[idx].selected = new_state;
        if (s_checkboxes[idx]) {
            if (new_state) lv_obj_add_state(s_checkboxes[idx], LV_STATE_CHECKED);
            else lv_obj_remove_state(s_checkboxes[idx], LV_STATE_CHECKED);
        }
    }

    if (new_state) s_all_selected = (_selected_count() == s_count);
    else s_all_selected = false;

    _sync_group_ui(group_idx);
    _sync_progress_range();
    _update_summary();
    _update_info_label();
}

static void _toggle_group_expand(int group_idx)
{
    if (group_idx < 0 || group_idx >= s_group_count) return;
    s_group_expanded[group_idx] = !s_group_expanded[group_idx];
    _set_group_visible(group_idx, s_group_expanded[group_idx]);
    _sync_group_ui(group_idx);
}

static void _toggle_all(bool select)
{
    s_all_selected = select;
    for (int i = 0; i < s_count; i++) {
        s_entries[i].selected = select;
        if (s_checkboxes[i]) {
            if (select) lv_obj_add_state(s_checkboxes[i], LV_STATE_CHECKED);
            else lv_obj_remove_state(s_checkboxes[i], LV_STATE_CHECKED);
        }
    }
    for (int g = 0; g < s_group_count; g++)
        _sync_group_ui(g);
    if (s_select_all_label)
        lv_label_set_text(s_select_all_label, select ? "Deselect All" : "Select All");
    _sync_progress_range();
    _update_summary();
    _update_info_label();
}

static void _select_all_cb(lv_event_t *e)
{
    (void)e;
    _toggle_all(!s_all_selected);
}

static void _checkbox_cb(lv_event_t *e)
{
    lv_event_stop_bubbling(e);
    lv_obj_t *cb = lv_event_get_target(e);
    int idx = (int)(long)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_count) return;

    bool checked = lv_obj_has_state(cb, LV_STATE_CHECKED);
    s_entries[idx].selected = checked;
    if (!checked) s_all_selected = false;
    else s_all_selected = (_selected_count() == s_count);

    for (int g = 0; g < s_group_count; g++) {
        if (idx >= s_groups[g].start_idx &&
            idx < s_groups[g].start_idx + s_groups[g].count) {
            _sync_group_ui(g);
            break;
        }
    }
    _sync_progress_range();
    _update_summary();
    _update_info_label();
}

static void _group_select_cb(lv_event_t *e)
{
    lv_event_stop_bubbling(e);
    int group_idx = (int)(long)lv_event_get_user_data(e);
    _toggle_group_select(group_idx);
}

static void _group_expand_cb(lv_event_t *e)
{
    int group_idx = (int)(long)lv_event_get_user_data(e);
    _toggle_group_expand(group_idx);
}

static void _run_cb(lv_event_t *e)
{
    (void)e;
    eos_test_run_all();
}

static void _on_enter(eos_activity_t *activity) { (void)activity; }

static void _on_destroy(eos_activity_t *activity)
{
    LV_UNUSED(activity);
    s_info_label = NULL;
    s_current_label = NULL;
    s_summary_label = NULL;
    s_progress_bar = NULL;
    s_run_btn = NULL;
    s_run_btn_label = NULL;
    s_status_icon = NULL;
    s_select_all_btn = NULL;
    s_select_all_label = NULL;
    s_checklist = NULL;
    for (int i = 0; i < s_count; i++) s_checkboxes[i] = NULL;
    for (int i = 0; i < s_group_count; i++) {
        s_group_headers[i] = NULL;
        s_group_header_labels[i] = NULL;
        s_group_checkboxes[i] = NULL;
    }
}

static const eos_activity_lifecycle_t s_fw_lifecycle = {
    .on_enter = _on_enter, .on_destroy = _on_destroy,
    .on_pause = NULL, .on_resume = NULL,
};

/* ---- Public API ---- */

void eos_test_register(const char *name, eos_test_fn_t fn)
{
    if (!name || !fn) return;
    if (s_count >= EOS_TEST_MAX) return;
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_entries[i].name, name) == 0) return;
    }
    strncpy(s_entries[s_count].name, name, EOS_TEST_NAME_MAX - 1);
    s_entries[s_count].name[EOS_TEST_NAME_MAX - 1] = '\0';
    s_entries[s_count].fn = fn;
    s_entries[s_count].has_run = false;
    s_entries[s_count].passed = false;
    s_entries[s_count].selected = true;
    s_count++;
}

void eos_test_record(const char *name, bool passed, const char *detail)
{
    EOS_LOG_I("%s: %s (%s)", name, passed ? "PASS" : "FAIL",
              detail ? detail : "");
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_entries[i].name, name) == 0) {
            if (!s_entries[i].has_run) {
                s_entries[i].has_run = true;
                s_entries[i].passed = passed;
                if (passed) s_passed++; else s_failed++;
                s_total++;
                if (!passed)
                    EOS_LOG_E("%s: FAIL - %s", name, detail ? detail : "no details");
            }
            _update_summary(); _update_progress();
            return;
        }
    }
    EOS_LOG_W("Test not registered: %s", name);
}

void eos_test_reset(void)
{
    s_passed = s_failed = s_total = 0;
    for (int i = 0; i < s_count; i++)
        s_entries[i].has_run = s_entries[i].passed = false;

    int sel = _selected_count();
    if (s_summary_label) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Pass: 0/%d  Fail: 0", sel);
        lv_label_set_text(s_summary_label, buf);
    }
    if (s_current_label) lv_label_set_text(s_current_label, "Idle");
    if (s_progress_bar) {
        lv_bar_set_range(s_progress_bar, 0, sel > 0 ? sel : 1);
        lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
    }
    if (s_status_icon) _set_status_icon(LV_SYMBOL_LIST, lv_color_hex(0x888888));
}

void eos_test_run_all(void)
{
    if (s_is_running) return;
    s_is_running = true;
    eos_test_reset();

    if (s_run_btn_label) lv_label_set_text(s_run_btn_label, "Running...");
    if (s_run_btn) lv_obj_add_state(s_run_btn, LV_STATE_DISABLED);
    _set_status_icon(LV_SYMBOL_REFRESH, lv_color_hex(0x2196F3));
    _force_refresh();

    int selected = _selected_count();
    EOS_LOG_I("========== Running %d/%d selected tests ==========", selected, s_count);
    int run_count = 0;
    for (int i = 0; i < s_count; i++) {
        if (!s_entries[i].selected) continue;
        run_count++;
        char status[EOS_TEST_NAME_MAX + 16];
        snprintf(status, sizeof(status), "[%d/%d] %s", run_count, selected, s_entries[i].name);
        _update_current(status); _force_refresh();
        EOS_LOG_I("[%d/%d] %s", run_count, selected, s_entries[i].name);
        bool passed = s_entries[i].fn();
        if (!s_entries[i].has_run)
            eos_test_record(s_entries[i].name, passed, passed ? "OK" : "Failed");
    }
    EOS_LOG_I("========== Test Results: %d passed, %d failed, %d total ==========",
              s_passed, s_failed, s_total);

    if (s_failed > 0) {
        _set_status_icon(LV_SYMBOL_CLOSE, lv_color_hex(0xF44336));
        _update_current("Complete - some tests FAILED");
    } else {
        _set_status_icon(LV_SYMBOL_OK, lv_color_hex(0x4CAF50));
        _update_current("Complete - all tests passed");
    }
    if (s_run_btn_label) lv_label_set_text(s_run_btn_label, "Run Tests");
    if (s_run_btn) lv_obj_remove_state(s_run_btn, LV_STATE_DISABLED);
    _force_refresh();
    s_is_running = false;
}

uint32_t eos_test_get_total(void) { return (uint32_t)s_total; }
uint32_t eos_test_get_passed(void) { return (uint32_t)s_passed; }
uint32_t eos_test_get_failed(void) { return (uint32_t)s_failed; }

bool eos_test_assert(bool cond, const char *file, int line, const char *msg)
{
    if (!cond) EOS_LOG_E("Assertion failed at %s:%d - %s", file, line, msg);
    return cond;
}

void eos_test_run_group(const char *prefix)
{
    if (s_is_running || !prefix) return;
    s_is_running = true;
    size_t prelen = strlen(prefix);
    EOS_LOG_I("========== Running group '%s' ==========", prefix);
    for (int i = 0; i < s_count; i++) {
        if (strncmp(s_entries[i].name, prefix, prelen) != 0) continue;
        eos_test_entry_t *e = &s_entries[i];
        bool passed = e->fn();
        if (!e->has_run) eos_test_record(e->name, passed, passed ? "OK" : "Failed");
    }
    EOS_LOG_I("========== Group '%s' complete ==========", prefix);
    s_is_running = false;
}

/* ---- Group / Checklist ---- */

static void _extract_group_name(const char *test_name, char *group, size_t maxlen)
{
    const char *colon = strchr(test_name, ':');
    if (colon) {
        size_t len = colon - test_name;
        if (len >= maxlen) len = maxlen - 1;
        memcpy(group, test_name, len);
        group[len] = '\0';
    } else {
        strncpy(group, test_name, maxlen - 1);
        group[maxlen - 1] = '\0';
    }
}

static void _build_groups(void)
{
    s_group_count = 0;
    for (int i = 0; i < s_count; i++) {
        char grp[EOS_TEST_GROUP_NAME_MAX];
        _extract_group_name(s_entries[i].name, grp, sizeof(grp));
        int found = -1;
        for (int g = 0; g < s_group_count; g++) {
            if (strcmp(s_groups[g].name, grp) == 0) { found = g; break; }
        }
        if (found >= 0) {
            s_groups[found].count++;
        } else if (s_group_count < EOS_TEST_GROUP_MAX) {
            strncpy(s_groups[s_group_count].name, grp, EOS_TEST_GROUP_NAME_MAX - 1);
            s_groups[s_group_count].start_idx = i;
            s_groups[s_group_count].count = 1;
            s_group_count++;
        }
    }
}

static void _build_checklist_page(lv_obj_t *cont)
{
    if (s_count == 0) return;
    _build_groups();

    for (int g = 0; g < s_group_count; g++)
        s_group_expanded[g] = false;

    /* Select All button */
    s_select_all_btn = lv_button_create(cont);
    lv_obj_set_size(s_select_all_btn, lv_pct(100), 28);
    lv_obj_set_style_radius(s_select_all_btn, 6, 0);
    lv_obj_set_style_bg_color(s_select_all_btn, lv_color_hex(0x607D8B), 0);
    s_select_all_label = lv_label_create(s_select_all_btn);
    lv_label_set_text(s_select_all_label, "Deselect All");
    lv_obj_set_style_text_color(s_select_all_label, lv_color_white(), 0);
    lv_obj_center(s_select_all_label);
    lv_obj_add_event_cb(s_select_all_btn, _select_all_cb, LV_EVENT_CLICKED, NULL);

    /* Scrollable checklist */
    s_checklist = lv_obj_create(cont);
    lv_obj_set_size(s_checklist, lv_pct(100), lv_pct(60));
    lv_obj_set_flex_grow(s_checklist, 1);
    lv_obj_set_flex_flow(s_checklist, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_checklist, 4, 0);
    lv_obj_set_style_border_width(s_checklist, 0, 0);
    lv_obj_add_flag(s_checklist, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_checklist, LV_SCROLLBAR_MODE_AUTO);

    int gi = 0;
    for (int i = 0; i < s_count; i++) {
        /* Group header */
        if (gi < s_group_count && i == s_groups[gi].start_idx) {
            eos_test_group_t *g = &s_groups[gi];

            lv_obj_t *header = lv_obj_create(s_checklist);
            lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_all(header, 4, 0);
            lv_obj_set_style_border_width(header, 0, 0);
            lv_obj_set_style_bg_opa(header, LV_OPA_20, 0);
            lv_obj_set_style_radius(header, 4, 0);
            lv_obj_add_event_cb(header, _group_expand_cb, LV_EVENT_CLICKED, (void *)(long)gi);
            s_group_headers[gi] = header;

            lv_obj_t *gcb = lv_checkbox_create(header);
            lv_checkbox_set_text(gcb, "");
            lv_obj_set_flex_align(gcb, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_add_state(gcb, LV_STATE_CHECKED);
            lv_obj_add_event_cb(gcb, _group_select_cb, LV_EVENT_CLICKED, (void *)(long)gi);
            s_group_checkboxes[gi] = gcb;

            lv_obj_t *glabel = lv_label_create(header);
            char buf[EOS_TEST_GROUP_NAME_MAX + 20];
            snprintf(buf, sizeof(buf), "%s %s (%d)", LV_SYMBOL_RIGHT, g->name, g->count);
            lv_label_set_text(glabel, buf);
            s_group_header_labels[gi] = glabel;
        }

        /* Individual test checkbox (hidden by default, collapsed) */
        {
            const char *name = s_entries[i].name;
            const char *colon = strchr(name, ':');
            const char *display = colon ? colon + 2 : name;

            lv_obj_t *cb = lv_checkbox_create(s_checklist);
            lv_checkbox_set_text(cb, display);
            lv_obj_set_width(cb, lv_pct(100));
            lv_obj_set_flex_align(cb, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_add_state(cb, LV_STATE_CHECKED);
            lv_obj_set_style_pad_ver(cb, 2, 0);
            lv_obj_set_style_pad_left(cb, 20, 0);
            lv_obj_add_flag(cb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_event_cb(cb, _checkbox_cb, LV_EVENT_CLICKED, (void *)(long)i);
            s_checkboxes[i] = cb;
        }

        if (gi < s_group_count && i == s_groups[gi].start_idx + s_groups[gi].count - 1)
            gi++;
    }
}

void eos_test_fw_page_start(const char *title)
{
    eos_activity_t *activity = eos_activity_create(&s_fw_lifecycle);
    if (!activity) return;
    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view) return;

    eos_activity_set_title(activity, title ? title : "Unit Tests");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    lv_obj_t *cont = lv_obj_create(view);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(cont, 8, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_style(cont, eos_theme_get_view_style(), 0);

    /* Header */
    lv_obj_t *header = lv_obj_create(cont);
    lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(header, 6, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_20, 0);
    lv_obj_set_style_radius(header, 6, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_status_icon = lv_label_create(header);
    lv_label_set_text(s_status_icon, LV_SYMBOL_LIST);

    s_info_label = lv_label_create(header);
    char info_buf[40];
    snprintf(info_buf, sizeof(info_buf), "%d tests loaded", s_count);
    lv_label_set_text(s_info_label, info_buf);

    /* Current */
    s_current_label = lv_label_create(cont);
    lv_label_set_text(s_current_label, "Idle");
    lv_obj_set_style_text_align(s_current_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_current_label, lv_pct(100));
    lv_label_set_long_mode(s_current_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_pad_top(s_current_label, 2, 0);

    /* Progress */
    s_progress_bar = lv_bar_create(cont);
    lv_obj_set_size(s_progress_bar, lv_pct(100), 4);
    lv_bar_set_range(s_progress_bar, 0, s_count > 0 ? s_count : 1);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(s_progress_bar, 2, 0);
    lv_obj_set_style_radius(s_progress_bar, 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0x2196F3), LV_PART_INDICATOR);

    /* Summary */
    s_summary_label = lv_label_create(cont);
    char sum_buf[48];
    snprintf(sum_buf, sizeof(sum_buf), "Pass: 0/%d  Fail: 0", s_count);
    lv_label_set_text(s_summary_label, sum_buf);
    lv_obj_set_style_text_align(s_summary_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_summary_label, lv_pct(100));

    /* Run button */
    s_run_btn = lv_button_create(cont);
    lv_obj_set_size(s_run_btn, lv_pct(100), 36);
    lv_obj_set_style_radius(s_run_btn, 8, 0);
    lv_obj_set_style_bg_color(s_run_btn, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_shadow_width(s_run_btn, 6, 0);
    lv_obj_set_style_shadow_opa(s_run_btn, LV_OPA_30, 0);
    s_run_btn_label = lv_label_create(s_run_btn);
    lv_label_set_text(s_run_btn_label, LV_SYMBOL_PLAY "  Run Tests");
    lv_obj_set_style_text_color(s_run_btn_label, lv_color_white(), 0);
    lv_obj_center(s_run_btn_label);
    lv_obj_add_event_cb(s_run_btn, _run_cb, LV_EVENT_CLICKED, NULL);

    /* Separator */
    lv_obj_t *sep = lv_obj_create(cont);
    lv_obj_set_size(sep, lv_pct(100), 1);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_30, 0);

    _build_checklist_page(cont);

    eos_crown_encoder_set_target_obj(s_checklist ? s_checklist : cont);
    eos_activity_enter(activity);
}

#endif /* EOS_ENABLE_TEST_APP */
