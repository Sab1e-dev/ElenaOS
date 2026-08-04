/**
 * @file eos_recent_apps.h
 * @brief Recent Apps registry — LRU list of suspended apps for instant resume
 */

#ifndef EOS_RECENT_APPS_H
#define EOS_RECENT_APPS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_core.h"
#include "eos_activity.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/**
 * @brief Timer resume strategy (mirrors sni_timer_resume_strategy_t)
 */
typedef enum
{
    EOS_SUSPEND_TIMER_RUN_ONCE = 0, /**< Fire callback once if elapsed >= period */
    EOS_SUSPEND_TIMER_RUN_ALL, /**< Fire N = min(elapsed/period, 10) callbacks */
    EOS_SUSPEND_TIMER_SKIP, /**< Reset timer, drop all missed */
} eos_suspend_timer_strategy_t;

/**
 * @brief Animation resume strategy (mirrors sni_anim_resume_strategy_t)
 */
typedef enum
{
    EOS_SUSPEND_ANIM_CONTINUE = 0, /**< Continue from paused position */
    EOS_SUSPEND_ANIM_JUMP_TO_END, /**< Jump to end state, fire completed */
} eos_suspend_anim_strategy_t;

/**
 * @brief A single entry in the Recent Apps LRU registry
 */
typedef struct eos_recent_app_entry
{
    struct eos_recent_app_entry *next; /**< LRU: more recently used */
    struct eos_recent_app_entry *prev; /**< LRU: less recently used */

    char app_id[64]; /**< Script package ID, stable key */
    eos_activity_t *activity; /**< AppRoot activity (parked, kept alive) */
    eos_activity_t *saved_stack_top; /**< Sub-stack top at suspend time */
    uint32_t saved_stack_depth; /**< Sub-stack depth (1 = just AppRoot) */
    lv_draw_buf_t *screenshot_buf; /**< Owned RGB565 draw buf */
    lv_obj_t *screenshot_img; /**< lv_image wrapping screenshot_buf */
    uint32_t last_used_tick; /**< LRU timestamp (eos_tick_get) */
    uint32_t est_mem_bytes; /**< Estimated memory footprint */
} eos_recent_app_entry_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize the Recent Apps registry
 * @note Subscribes to EOS_EVENT_APP_UNINSTALLED for cleanup.
 */
void eos_recent_apps_init(void);

/**
 * @brief Suspend the currently running app and register it in the recents list
 * @return EOS_OK on success
 *
 * Flow:
 *  1. Walk app_substack_next to find AppRoot and count depth
 *  2. Take standalone snapshot of stack top view
 *  3. Allocate entry, link into LRU head (MRU)
 *  4. Detach sub-stack from main activity stack
 *  5. Suspend SPM program (pause sni_ctx, state = SUSPENDED)
 *  6. Run LRU eviction check
 */
eos_result_t eos_recent_apps_suspend_current(void);

/**
 * @brief Resume a suspended app from the recents list
 * @param entry The recents entry to resume
 * @return EOS_OK on success
 *
 * Flow:
 *  1. Unlink from LRU list
 *  2. Re-attach sub-stack to main activity stack (on_resume chain)
 *  3. Resume SPM program with timer/animation strategies
 *  4. Transition animation using stored screenshot
 */
eos_result_t eos_recent_apps_resume(eos_recent_app_entry_t *entry);

/**
 * @brief Resume a suspended app by its package ID
 * @param app_id Application package ID
 * @return EOS_OK on success, EOS_FAILED if not found
 */
eos_result_t eos_recent_apps_resume_by_id(const char *app_id);

/**
 * @brief Find a recents entry by app ID
 * @param app_id Application package ID
 * @return Entry pointer, or NULL if not found
 */
eos_recent_app_entry_t *eos_recent_apps_find(const char *app_id);

/**
 * @brief Evict and fully destroy a recents entry (close button, LRU, uninstall)
 * @param entry Entry to evict
 * @return EOS_OK on success
 *
 * Destroys the entire sub-stack (on_destroy chain) and the SPM program
 * (full 6-phase teardown including realm destruction).
 */
eos_result_t eos_recent_apps_evict(eos_recent_app_entry_t *entry);

/**
 * @brief Clear all recents entries (engine reset recovery)
 */
void eos_recent_apps_clear_all(void);

/**
 * @brief Called after spm_handle_engine_reset to purge entries with destroyed programs
 */
void eos_recent_apps_on_engine_reset(void);

/**
 * @brief Set the timer resume strategy at runtime
 * @param strategy Strategy to use
 */
void eos_recent_apps_set_timer_strategy(eos_suspend_timer_strategy_t strategy);

/**
 * @brief Set the animation resume strategy at runtime
 * @param strategy Strategy to use
 */
void eos_recent_apps_set_anim_strategy(eos_suspend_anim_strategy_t strategy);

/**
 * @brief Get the number of entries in the recents list
 * @return uint32_t Entry count
 */
uint32_t eos_recent_apps_count(void);

/**
 * @brief Check whether the given activity is an app activity or a sub-activity
 *        that belongs to a script app (and thus is suspendable)
 * @param activity Activity to check
 * @return true if the activity or its app_root is APP type
 */
bool eos_recent_apps_is_suspendable(eos_activity_t *activity);

/**
 * @brief Register an app activity for suspend (take snapshot, create entry, suspend SPM)
 *
 * Called from eos_activity_back() when leaving an APP-type activity.
 * Does NOT detach the sub-stack — the caller handles that via the normal
 * back() / transition / suspend_on_exit flow.
 *
 * @param app_activity The APP-type activity being left
 * @return EOS_OK on success
 */
eos_result_t eos_recent_apps_register_for_suspend(eos_activity_t *app_activity);

#ifdef __cplusplus
}
#endif

#endif /* EOS_RECENT_APPS_H */
