/**
 * @file eos_recent_apps.c
 * @brief Recent Apps registry — LRU list, suspend/resume orchestration, eviction
 */

#include "eos_recent_apps.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "lvgl.h"
#define EOS_LOG_TAG "RecentApps"
#include "eos_log.h"
#include "eos_mem.h"
#include "eos_app.h"
#include "eos_app_list.h"
#include "eos_event.h"
#include "eos_dispatcher.h"
#include "spm.h"
#include "sni_context.h"
#include "sni_callback_runtime.h"
#include "eos_config.h"
#include "eos_basic_widgets.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

static eos_recent_app_entry_t *s_head = NULL;
static eos_recent_app_entry_t *s_tail = NULL;
static uint32_t s_count = 0;
static uint32_t s_total_mem_bytes = 0;
static eos_suspend_timer_strategy_t s_timer_strategy = EOS_SUSPEND_TIMER_RUN_ONCE;
static eos_suspend_anim_strategy_t s_anim_strategy = EOS_SUSPEND_ANIM_CONTINUE;
static bool s_initialized = false;

/* Forward Declarations ---------------------------------------*/
static void _lru_link_head(eos_recent_app_entry_t *entry);
static void _lru_unlink(eos_recent_app_entry_t *entry);
static void _lru_eviction_check(void);
static void _app_uninstalled_cb(eos_event_t *e);
static uint32_t _estimate_entry_mem(eos_recent_app_entry_t *entry);

/* Function Implementations -----------------------------------*/

static void _lru_link_head(eos_recent_app_entry_t *entry)
{
    entry->next = s_head;
    entry->prev = NULL;
    if (s_head)
        s_head->prev = entry;
    s_head = entry;
    if (!s_tail)
        s_tail = entry;
    s_count++;
    s_total_mem_bytes += entry->est_mem_bytes;
}

static void _lru_unlink(eos_recent_app_entry_t *entry)
{
    if (entry->prev)
        entry->prev->next = entry->next;
    else
        s_head = entry->next;

    if (entry->next)
        entry->next->prev = entry->prev;
    else
        s_tail = entry->prev;

    if (s_count > 0)
        s_count--;
    if (s_total_mem_bytes >= entry->est_mem_bytes)
        s_total_mem_bytes -= entry->est_mem_bytes;
    else
        s_total_mem_bytes = 0;

    entry->next = NULL;
    entry->prev = NULL;
}

static uint32_t _estimate_entry_mem(eos_recent_app_entry_t *e)
{
    LV_UNUSED(e);
    /* Screenshot: width * height * 2 bytes (RGB565) */
    uint32_t img_bytes = (uint32_t)(390 * 450 * 2);
    /* Rough estimate for realm + sni_ctx + activity structs */
    uint32_t overhead = 64 * 1024;
    return img_bytes + overhead;
}

static void _lru_eviction_check(void)
{
    uint32_t max = (uint32_t)EOS_RECENT_APPS_MAX;
    uint32_t watermark = (uint32_t)EOS_RECENT_APPS_MEM_HIGH_WATERMARK;

    while ((s_count > max || s_total_mem_bytes > watermark) && s_tail)
    {
        EOS_LOG_W("LRU eviction triggered: count=%u (max=%u) mem=%u (limit=%u)",
                  s_count, max, s_total_mem_bytes, watermark);
        eos_recent_apps_evict(s_tail);
    }
}

/* Public API -------------------------------------------------*/

void eos_recent_apps_init(void)
{
    if (s_initialized)
        return;

    /* Load strategy from Kconfig */
#if defined(EOS_RECENT_APPS_TIMER_STRATEGY)
    s_timer_strategy = (eos_suspend_timer_strategy_t)EOS_RECENT_APPS_TIMER_STRATEGY;
#endif
#if defined(EOS_RECENT_APPS_ANIM_STRATEGY)
    s_anim_strategy = (eos_suspend_anim_strategy_t)EOS_RECENT_APPS_ANIM_STRATEGY;
#endif

    /* Subscribe to app uninstall events for cleanup */
    eos_event_subscribe_ex(EOS_EVENT_APP_UNINSTALLED, _app_uninstalled_cb, NULL, NULL);

    s_initialized = true;
    EOS_LOG_I("Recent Apps initialized: max=%d mem_limit=%d timer_strat=%d anim_strat=%d",
              EOS_RECENT_APPS_MAX,
              EOS_RECENT_APPS_MEM_HIGH_WATERMARK,
              (int)s_timer_strategy,
              (int)s_anim_strategy);
}

eos_result_t eos_recent_apps_suspend_current(void)
{
    if (!s_initialized)
    {
        EOS_LOG_W("Recent Apps not initialized");
        return EOS_FAILED;
    }

    eos_activity_t *current = eos_activity_get_current();
    if (!current)
    {
        EOS_LOG_W("No current activity to suspend");
        return EOS_FAILED;
    }

    /* Validate: must be an APP activity or a sub-activity belonging to an app */
    if (!eos_recent_apps_is_suspendable(current))
    {
        EOS_LOG_W("Current activity is not a suspendable app");
        return EOS_FAILED;
    }

    if (eos_activity_is_transition_in_progress())
    {
        EOS_LOG_W("Cannot suspend during transition");
        return EOS_FAILED;
    }

    /* Walk to AppRoot and count depth */
    eos_activity_t *app_root = current;
    uint32_t depth = 1;
    while (eos_activity_get_app_substack_next(app_root))
    {
        app_root = eos_activity_get_app_substack_next(app_root);
        depth++;
    }

    if (eos_activity_get_type(app_root) != EOS_ACTIVITY_TYPE_APP)
    {
        EOS_LOG_W("AppRoot not found");
        return EOS_FAILED;
    }

    /* Get the app_id from the title (set from pkg.name during launch) */
    const char *title = eos_activity_get_title(app_root);
    if (!title)
    {
        EOS_LOG_W("AppRoot has no title");
        return EOS_FAILED;
    }

    /* Deduplicate: if this app is already in recents, evict the old entry */
    eos_recent_app_entry_t *existing = eos_recent_apps_find(title);
    if (existing)
    {
        EOS_LOG_I("App '%s' already in recents, evicting old entry", title);
        eos_recent_apps_evict(existing);
    }

    /* Take standalone snapshot of the stack top view (what the user sees) */
    lv_draw_buf_t *screenshot = eos_activity_take_snapshot_standalone(current, false);
    if (!screenshot)
    {
        EOS_LOG_W("Failed to take snapshot for '%s'", title);
        /* Continue without screenshot — resume will use a placeholder */
    }

    /* Allocate entry */
    eos_recent_app_entry_t *entry = eos_malloc_zeroed(sizeof(eos_recent_app_entry_t));
    if (!entry)
    {
        EOS_LOG_E("Failed to allocate recents entry");
        if (screenshot)
            eos_draw_buf_destroy(screenshot);
        return EOS_FAILED;
    }

    snprintf(entry->app_id, sizeof(entry->app_id), "%s", title);
    entry->activity = app_root;
    entry->saved_stack_top = current;
    entry->saved_stack_depth = depth;
    entry->screenshot_buf = screenshot;
    entry->last_used_tick = eos_tick_get();
    entry->est_mem_bytes = _estimate_entry_mem(entry);

    /* Mark sub-stack for suspend park */
    eos_activity_t *node = current;
    while (node && node != eos_activity_get_app_substack_next(app_root))
    {
        eos_activity_set_suspend_on_exit(node, true);
        eos_activity_set_suspended(node, true);
        node = eos_activity_get_app_substack_next(node);
    }

    /* Detach sub-stack from main activity stack (calls on_pause chain) */
    eos_activity_t *detached = eos_activity_detach_app_substack();
    if (!detached)
    {
        EOS_LOG_E("Failed to detach app sub-stack");
        if (entry->screenshot_buf)
            eos_draw_buf_destroy(entry->screenshot_buf);
        eos_free(entry);
        return EOS_FAILED;
    }

    /* Suspend SPM program */
    eos_result_t spm_ret = spm_app_suspend();
    if (spm_ret != EOS_OK)
    {
        EOS_LOG_W("SPM suspend returned %d (may already be suspended)", spm_ret);
    }

    /* Link to LRU head */
    _lru_link_head(entry);

    EOS_LOG_I("App suspended: '%s' depth=%u mem=%u total_count=%u total_mem=%u",
              entry->app_id, depth, entry->est_mem_bytes, s_count, s_total_mem_bytes);

    /* Run LRU eviction check */
    _lru_eviction_check();

    return EOS_OK;
}

eos_result_t eos_recent_apps_resume(eos_recent_app_entry_t *entry)
{
    if (!entry)
        return EOS_FAILED;

    if (eos_activity_is_transition_in_progress())
    {
        EOS_LOG_W("Cannot resume during transition");
        return EOS_FAILED;
    }

    EOS_LOG_I("Resuming app: '%s' depth=%u", entry->app_id, entry->saved_stack_depth);

    /* Unlink from LRU list */
    _lru_unlink(entry);

    /* Re-attach sub-stack to main activity stack (calls on_resume chain bottom-up) */
    eos_activity_reattach_app_substack(entry->saved_stack_top);

    /* Resume SPM program (AppRoot's on_resume handles this) */
    /* Note: The on_resume chain was already called during reattach.
     * For AppRoot, _app_on_resume will call spm_app_resume() with timer/anim strategies.
     * We ensure the strategies are applied by calling sni_context_resume_resources
     * directly on the SPM program's context. */
    script_program_t *prog = spm_get_program_by_id_any_state(entry->app_id);
    if (prog && prog->state == SCRIPT_PROGRAM_STATE_ACTIVE && prog->sni_ctx)
    {
        sni_context_resume_resources(prog->sni_ctx,
                                      (int)s_timer_strategy,
                                      (int)s_anim_strategy);
    }

    /* Free screenshot after resume transition completes */
    if (entry->screenshot_buf)
    {
        eos_draw_buf_destroy(entry->screenshot_buf);
        entry->screenshot_buf = NULL;
    }
    entry->screenshot_img = NULL;
    entry->activity = NULL;
    entry->saved_stack_top = NULL;

    eos_free(entry);

    EOS_LOG_I("App resumed successfully");
    return EOS_OK;
}

eos_result_t eos_recent_apps_resume_by_id(const char *app_id)
{
    eos_recent_app_entry_t *entry = eos_recent_apps_find(app_id);
    if (!entry)
        return EOS_FAILED;
    return eos_recent_apps_resume(entry);
}

eos_recent_app_entry_t *eos_recent_apps_find(const char *app_id)
{
    if (!app_id)
        return NULL;
    eos_recent_app_entry_t *entry = s_head;
    while (entry)
    {
        if (strcmp(entry->app_id, app_id) == 0)
            return entry;
        entry = entry->next;
    }
    return NULL;
}

eos_result_t eos_recent_apps_evict(eos_recent_app_entry_t *entry)
{
    if (!entry)
        return EOS_FAILED;

    EOS_LOG_I("Evicting app: '%s'", entry->app_id);

    _lru_unlink(entry);

    /* Destroy the entire sub-stack: walk app_substack_next chain from top down.
     * Use eos_activity_destroy() which handles the full teardown including
     * on_destroy lifecycle, view cleanup, and memory free. */
    eos_activity_t *node = entry->saved_stack_top;
    while (node)
    {
        eos_activity_t *next = eos_activity_get_app_substack_next(node);

        /* Clear suspended flag so _activity_run_destroy proceeds */
        eos_activity_set_suspended(node, false);
        eos_activity_set_suspend_on_exit(node, false);

        /* Destroy the activity (calls on_destroy, deletes view, frees memory).
         * For the AppRoot, on_destroy triggers spm_app_stop_by_id(). */
        eos_activity_destroy(node);

        if (node == entry->activity)
            break;
        node = next;
    }

    /* Free screenshot */
    if (entry->screenshot_buf)
    {
        eos_draw_buf_destroy(entry->screenshot_buf);
        entry->screenshot_buf = NULL;
    }

    eos_free(entry);
    return EOS_OK;
}

void eos_recent_apps_clear_all(void)
{
    while (s_head)
    {
        eos_recent_apps_evict(s_head);
    }
    s_count = 0;
    s_total_mem_bytes = 0;
    EOS_LOG_I("All recents entries cleared");
}

void eos_recent_apps_on_engine_reset(void)
{
    EOS_LOG_W("Engine reset detected — clearing recents");
    /* Walk and evict all entries. The programs are already destroyed
     * by spm_handle_engine_reset, so we only need to clean up C structures. */
    eos_recent_app_entry_t *entry = s_head;
    while (entry)
    {
        eos_recent_app_entry_t *next = entry->next;
        /* Mark as not suspended so cleanup proceeds */
        if (entry->activity)
            eos_activity_set_suspended(entry->activity, false);
        if (entry->screenshot_buf)
        {
            eos_draw_buf_destroy(entry->screenshot_buf);
            entry->screenshot_buf = NULL;
        }
        eos_free(entry);
        entry = next;
    }
    s_head = NULL;
    s_tail = NULL;
    s_count = 0;
    s_total_mem_bytes = 0;
}

void eos_recent_apps_set_timer_strategy(eos_suspend_timer_strategy_t strategy)
{
    s_timer_strategy = strategy;
}

void eos_recent_apps_set_anim_strategy(eos_suspend_anim_strategy_t strategy)
{
    s_anim_strategy = strategy;
}

uint32_t eos_recent_apps_count(void)
{
    return s_count;
}

bool eos_recent_apps_is_suspendable(eos_activity_t *activity)
{
    if (!activity)
        return false;
    if (eos_activity_get_type(activity) == EOS_ACTIVITY_TYPE_APP)
        return true;
    /* Check if it's a sub-activity with an app_root set */
    eos_activity_t *root = eos_activity_get_app_root(activity);
    if (root && eos_activity_get_type(root) == EOS_ACTIVITY_TYPE_APP)
        return true;
    return false;
}

/* Event Handlers ---------------------------------------------*/

static void _app_uninstalled_cb(eos_event_t *e)
{
    const char *app_id = (const char *)eos_event_get_param(e);
    if (!app_id)
        return;
    EOS_LOG_I("App uninstalled: '%s' — checking recents", app_id);
    eos_recent_app_entry_t *entry = eos_recent_apps_find(app_id);
    if (entry)
    {
        EOS_LOG_I("Evicting recents entry for uninstalled app: '%s'", app_id);
        eos_recent_apps_evict(entry);
    }
}
