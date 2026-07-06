/**
 * @file eos_test_stress.c
 * @brief SPM stress test - memory leak check via unit test framework
 */

#include "eos_test_stress.h"
#if EOS_ENABLE_TEST_APP

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "eos_log.h"
#include "eos_test_framework.h"
#include "eos_activity.h"
#include "eos_app.h"
#include "eos_service_storage.h"
#include "eos_mem.h"
#include "spm.h"
#include "eos_pkg_mgr.h"
#include "lvgl.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "StressTest"
#define SPM_STRESS_TEST_APP_ID "com.elenixos.test"
#define SPM_STRESS_MAX_CYCLES 30
#define SPM_STRESS_PHASE_CREATE 0
#define SPM_STRESS_PHASE_BACK 1
#define SPM_STRESS_PHASE_WAIT 2
#define SPM_STRESS_DELAY 100

/* Variables --------------------------------------------------*/

typedef struct
{
    lv_timer_t *timer;
    int cycle;
    int phase;
    script_pkg_t pkg;
    eos_activity_t *activity;
    unsigned long prev_alloc;
} stress_ctx_t;

static stress_ctx_t s_stress = {0};
static bool s_stress_passed = true;

/* Function Implementations -----------------------------------*/

static unsigned long _stress_get_alloc(void)
{
    if (!jerry_feature_enabled(JERRY_FEATURE_HEAP_STATS))
        return 0;
    jerry_heap_stats_t stats = {0};
    jerry_heap_stats(&stats);
    return stats.allocated_bytes;
}

static void _stress_report_header(void)
{
    EOS_LOG_I("SPM STRESS TEST -- Memory Leak Check");
    EOS_LOG_I("  App: " SPM_STRESS_TEST_APP_ID);
    EOS_LOG_I("  Cycles: %d", SPM_STRESS_MAX_CYCLES);
}

static void _stress_timer_cb(lv_timer_t *t);

static void _stress_cleanup(void)
{
    if (s_stress.timer)
    {
        lv_timer_delete(s_stress.timer);
        s_stress.timer = NULL;
    }
    eos_pkg_free(&s_stress.pkg);
    memset(&s_stress.pkg, 0, sizeof(s_stress.pkg));
    s_stress.activity = NULL;
}

static bool _stress_load_pkg(void)
{
    char manifest_path[EOS_FS_PATH_MAX];
    snprintf(manifest_path,
             sizeof(manifest_path),
             EOS_APP_INSTALLED_DIR SPM_STRESS_TEST_APP_ID "/" EOS_APP_MANIFEST_FILE_NAME);

    memset(&s_stress.pkg, 0, sizeof(s_stress.pkg));
    s_stress.pkg.type = SCRIPT_TYPE_APPLICATION;
    if (script_engine_get_manifest(manifest_path, &s_stress.pkg) != EOS_OK)
    {
        EOS_LOG_E("[STRESS] Failed to read manifest: %s", manifest_path);
        return false;
    }

    char script_path[EOS_FS_PATH_MAX];
    snprintf(script_path,
             sizeof(script_path),
             EOS_APP_INSTALLED_DIR SPM_STRESS_TEST_APP_ID "/" EOS_APP_SCRIPT_ENTRY_FILE_NAME);

    char base_path[EOS_FS_PATH_MAX];
    snprintf(base_path, sizeof(base_path), EOS_APP_INSTALLED_DIR SPM_STRESS_TEST_APP_ID "/");
    s_stress.pkg.base_path = eos_strdup(base_path);
    s_stress.pkg.script_str = eos_storage_read_file(script_path);

    if (!s_stress.pkg.script_str)
    {
        EOS_LOG_E("[STRESS] Failed to read script: %s", script_path);
        eos_pkg_free(&s_stress.pkg);
        memset(&s_stress.pkg, 0, sizeof(s_stress.pkg));
        return false;
    }

    return true;
}

static void _stress_app_on_enter(eos_activity_t *a)
{
    (void)a;
    unsigned long before = _stress_get_alloc();

    eos_result_t ret = spm_app_run(&s_stress.pkg);
    if (ret != EOS_OK)
    {
        EOS_LOG_E("[STRESS] Cycle %d: run failed ret=%d", s_stress.cycle, ret);
    }

    s_stress.prev_alloc = before;
    eos_pkg_free(&s_stress.pkg);
    memset(&s_stress.pkg, 0, sizeof(s_stress.pkg));
    s_stress.phase = SPM_STRESS_PHASE_BACK;
}

static void _stress_app_on_destroy(eos_activity_t *a)
{
    (void)a;
    spm_app_stop();
}

static const eos_activity_lifecycle_t _stress_lifecycle = {
    .on_enter = _stress_app_on_enter,
    .on_destroy = _stress_app_on_destroy,
};

static void _stress_timer_cb(lv_timer_t *t)
{
    (void)t;

    switch (s_stress.phase)
    {
        case SPM_STRESS_PHASE_CREATE:
        {
            if (!_stress_load_pkg())
            {
                _stress_cleanup();
                return;
            }

            s_stress.activity = eos_activity_create(&_stress_lifecycle);
            if (!s_stress.activity)
            {
                EOS_LOG_E("[STRESS] Failed to create activity");
                _stress_cleanup();
                return;
            }

            lv_obj_t *view = eos_activity_get_view(s_stress.activity);
            lv_obj_set_size(view, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
            eos_activity_set_type(s_stress.activity, EOS_ACTIVITY_TYPE_APP);
            eos_activity_set_title(s_stress.activity, SPM_STRESS_TEST_APP_ID);
            eos_activity_enter(s_stress.activity);
            break;
        }

        case SPM_STRESS_PHASE_BACK:
        {
            if (eos_activity_is_transition_in_progress())
                return;
            eos_activity_back();
            s_stress.phase = SPM_STRESS_PHASE_WAIT;
            break;
        }

        case SPM_STRESS_PHASE_WAIT:
        {
            if (eos_activity_is_transition_in_progress())
                return;
            if (eos_activity_get_current() == s_stress.activity)
                return;

            unsigned long after = _stress_get_alloc();
            long delta = (long)after - (long)s_stress.prev_alloc;

            EOS_LOG_I("[STRESS] Cycle %2d: alloc=%lu delta=%ld %s",
                      s_stress.cycle,
                      after,
                      delta,
                      delta > 0 ? "(LEAK?)" : "");

            if (delta > 0)
            {
                s_stress_passed = false;
            }

            s_stress.cycle++;
            s_stress.activity = NULL;

            if (s_stress.cycle >= SPM_STRESS_MAX_CYCLES)
            {
                EOS_LOG_I("[STRESS] Test complete. %d cycles.", SPM_STRESS_MAX_CYCLES);
                EOS_LOG_I("[STRESS] Final alloc=%lu", after);
                eos_test_record("SPM Stress: memory leak check",
                                s_stress_passed,
                                s_stress_passed ? "No heap growth detected" : "Heap grew");
                _stress_cleanup();
                return;
            }

            s_stress.phase = SPM_STRESS_PHASE_CREATE;
            break;
        }
    }
}

static bool _test_spm_stress(void)
{
    _stress_report_header();
    memset(&s_stress, 0, sizeof(s_stress));
    s_stress_passed = true;
    s_stress.phase = SPM_STRESS_PHASE_CREATE;
    s_stress.timer = lv_timer_create(_stress_timer_cb, SPM_STRESS_DELAY, NULL);
    if (!s_stress.timer)
    {
        EOS_LOG_E("[STRESS] Failed to create timer");
        eos_test_record("SPM Stress: memory leak check", false, "Timer creation failed");
        return false;
    }
    return true;
}

void eos_test_stress_register_tests(void)
{
    eos_test_register("SPM Stress: memory leak check", _test_spm_stress);
}

#endif /* EOS_ENABLE_TEST_APP */
