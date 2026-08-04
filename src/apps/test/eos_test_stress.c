/**
 * @file eos_test_stress.c
 * @brief SPM stress test - memory leak check via unit test framework
 *
 * Synchronously runs 30 app launch/back cycles and checks whether the
 * JerryScript heap returns to the previous cycle's post-cleanup level.
 * Each cycle loads the test app, creates an activity, runs the JS
 * script, goes back, waits for the transition to settle and activity
 * destruction to complete, then compares the post-cleanup heap against
 * the previous cycle's post-cleanup value.  Any positive delta means
 * the app-launch-to-close cycle leaked memory.
 *
 * The test also checks total growth from the initial baseline after
 * all cycles complete as a second line of defense.
 *
 * The test aborts early (returns false) on the first package-load
 * failure or spm_app_run error so the framework sees the real result
 * and the remaining cycles don't hammer a broken engine.
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
#define SPM_STRESS_TIMEOUT_MS 5000 /* per-phase hard timeout */

/* Variables --------------------------------------------------*/

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

/**
 * @brief Pump LVGL timer handler until @p cond returns true or timeout.
 * @return true if condition was met, false on timeout.
 */
static bool _stress_pump_until(bool (*cond)(void), uint32_t timeout_ms)
{
    uint32_t start = lv_tick_get();
    while (!cond())
    {
        lv_timer_handler();
        uint32_t now = lv_tick_get();
        if (now - start >= timeout_ms)
            return false;
    }
    return true;
}

/* Conditions for _stress_pump_until --------------------------*/

static bool _stress_transition_done(void)
{
    return !eos_activity_is_transition_in_progress();
}

static eos_activity_t *s_expected_activity = NULL;

static bool _stress_activity_changed(void)
{
    return eos_activity_get_current() != s_expected_activity;
}

/* Package loading --------------------------------------------*/

static bool _stress_load_pkg(script_pkg_t *pkg)
{
    char manifest_path[EOS_FS_PATH_MAX];
    snprintf(manifest_path,
             sizeof(manifest_path),
             EOS_APP_INSTALLED_DIR SPM_STRESS_TEST_APP_ID "/" EOS_APP_MANIFEST_FILE_NAME);

    memset(pkg, 0, sizeof(*pkg));
    pkg->type = SCRIPT_TYPE_APPLICATION;
    if (script_engine_get_manifest(manifest_path, pkg) != EOS_OK)
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
    pkg->base_path = eos_strdup(base_path);
    pkg->script_str = eos_storage_read_file(script_path);

    if (!pkg->script_str)
    {
        EOS_LOG_E("[STRESS] Failed to read script: %s", script_path);
        eos_pkg_free(pkg);
        memset(pkg, 0, sizeof(*pkg));
        return false;
    }

    return true;
}

/* Lifecycle callbacks ----------------------------------------*/

static void _stress_on_enter(eos_activity_t *a)
{
    (void)a;
    /* The script is started synchronously from _test_spm_stress below,
     * not from this callback.  We just need the activity to exist. */
}

static void _stress_on_destroy(eos_activity_t *a)
{
    (void)a;
    spm_app_stop();
}

static const eos_activity_lifecycle_t _stress_lifecycle = {
    .on_enter = _stress_on_enter,
    .on_destroy = _stress_on_destroy,
};

/* Main test function (synchronous) ---------------------------*/

static bool _test_spm_stress(void)
{
    _stress_report_header();

    s_stress_passed = true;
    unsigned long baseline = _stress_get_alloc();
    unsigned long prev_after = baseline;

    for (int cycle = 0; cycle < SPM_STRESS_MAX_CYCLES; cycle++)
    {
        /* Load package */
        script_pkg_t pkg;
        if (!_stress_load_pkg(&pkg))
        {
            EOS_LOG_E("[STRESS] Cycle %d: package load failed — aborting", cycle);
            s_stress_passed = false;
            break;
        }

        /* Create and enter activity */
        eos_activity_t *activity = eos_activity_create(&_stress_lifecycle);
        if (!activity)
        {
            EOS_LOG_E("[STRESS] Cycle %d: activity create failed — aborting", cycle);
            eos_pkg_free(&pkg);
            s_stress_passed = false;
            break;
        }

        lv_obj_t *view = eos_activity_get_view(activity);
        lv_obj_set_size(view, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
        eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);
        eos_activity_set_title(activity, SPM_STRESS_TEST_APP_ID);

        eos_activity_enter(activity);

        /* Run the JS app */
        eos_result_t ret = spm_app_run(&pkg);
        eos_pkg_free(&pkg);
        memset(&pkg, 0, sizeof(pkg));

        if (ret != EOS_OK)
        {
            EOS_LOG_E("[STRESS] Cycle %d: spm_app_run failed ret=%d — aborting", cycle, ret);
            s_stress_passed = false;
            break;
        }

        /* Wait for enter transition to finish */
        if (!_stress_pump_until(_stress_transition_done, SPM_STRESS_TIMEOUT_MS))
        {
            EOS_LOG_W("[STRESS] Cycle %d: enter transition timed out", cycle);
        }

        /* Go back (pops the app's activity + our stress activity) */
        s_expected_activity = activity;
        eos_activity_back();

        if (!_stress_pump_until(_stress_transition_done, SPM_STRESS_TIMEOUT_MS))
        {
            EOS_LOG_W("[STRESS] Cycle %d: back transition timed out", cycle);
        }

        /* Wait for activity to actually change (destroy callbacks fired) */
        if (!_stress_pump_until(_stress_activity_changed, SPM_STRESS_TIMEOUT_MS))
        {
            EOS_LOG_W("[STRESS] Cycle %d: activity change timed out", cycle);
        }

        /* Compare post-cleanup heap against previous cycle's post-cleanup level.
         * This avoids the package/activity memory offset that would mask small
         * leaks when comparing against a mid-cycle "before" snapshot. */
        unsigned long after = _stress_get_alloc();
        long delta = (long)after - (long)prev_after;

        EOS_LOG_I("[STRESS] Cycle %2d: prev=%lu after=%lu delta=%ld %s",
                  cycle,
                  prev_after,
                  after,
                  delta,
                  delta > 0 ? "(LEAK?)" : "");

        if (delta > 0)
        {
            s_stress_passed = false;
        }

        prev_after = after;
    }

    /* Final report */
    unsigned long final = _stress_get_alloc();
    long total_delta = (long) final - (long)baseline;

    EOS_LOG_I("[STRESS] Test complete.  %d cycles, baseline=%lu final=%lu delta=%ld",
              SPM_STRESS_MAX_CYCLES,
              baseline,
              final,
              total_delta);

    /* Second line of defense: absolute growth from the initial baseline must
     * also be zero.  This catches leaks that the per-cycle check might miss
     * (e.g. a one-time leak during setup before the first cycle). */
    if (total_delta > 0)
    {
        EOS_LOG_E("[STRESS] Total heap growth detected: %ld bytes", total_delta);
        s_stress_passed = false;
    }

    eos_test_record("SPM Stress: memory leak check",
                    s_stress_passed,
                    s_stress_passed ? "No heap growth detected" : "Heap grew");

    return s_stress_passed;
}

void eos_test_stress_register_tests(void)
{
    eos_test_register("SPM Stress: memory leak check", _test_spm_stress);
}

#endif /* EOS_ENABLE_TEST_APP */
