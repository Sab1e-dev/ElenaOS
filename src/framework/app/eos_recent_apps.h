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
#include "eos_config.h"

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
    char app_name[64]; /**< Display name for the recents page UI */
    eos_activity_t *activity; /**< AppRoot activity (parked, kept alive) */
    eos_activity_t *saved_stack_top; /**< Sub-stack top at suspend time */
    uint32_t saved_stack_depth; /**< Sub-stack depth (1 = just AppRoot) */
    uint32_t last_used_tick; /**< LRU timestamp (eos_tick_get) */
    uint32_t est_mem_bytes; /**< Estimated memory footprint */
    lv_draw_buf_t *snap_buf; /**< Screenshot of app view at suspend time, used for resume animation */
    lv_draw_buf_t *thumb_buf; /**< Full-resolution screenshot for Recent Apps page display */
} eos_recent_app_entry_t;

/* Public function prototypes ---------------------------------*/

#if EOS_RECENT_APPS_ENABLE

/**
 * @brief Initialize the Recent Apps registry
 * @note Subscribes to EOS_EVENT_APP_UNINSTALLED for cleanup.
 */
void eos_recent_apps_init(void);

/**
 * @brief Suspend the currently running app and register it in the recents list
 * @return EOS_OK on success
 */
eos_result_t eos_recent_apps_suspend_current(void);

/**
 * @brief Resume a suspended app from the recents list
 * @param entry The recents entry to resume
 * @return EOS_OK on success
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
 * @brief Evict and fully destroy a recents entry
 * @param entry Entry to evict
 * @return EOS_OK on success
 */
eos_result_t eos_recent_apps_evict(eos_recent_app_entry_t *entry);

/**
 * @brief Clear all recents entries (engine reset recovery)
 */
void eos_recent_apps_clear_all(void);

/**
 * @brief Called after spm_handle_engine_reset to purge entries
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
 * @brief Check whether the given activity is a suspendable script app
 * @param activity Activity to check
 * @return true if the activity or its app_root is APP type
 */
bool eos_recent_apps_is_suspendable(eos_activity_t *activity);

/**
 * @brief Get the first (MRU) entry in the recents list for iteration
 * @return eos_recent_app_entry_t* Head entry, or NULL if empty
 */
eos_recent_app_entry_t *eos_recent_apps_get_head(void);

/**
 * @brief Get the next entry in the recents list
 * @param entry Current entry
 * @return eos_recent_app_entry_t* Next entry, or NULL if at end
 */
eos_recent_app_entry_t *eos_recent_apps_get_next(eos_recent_app_entry_t *entry);

/**
 * @brief Register an app activity for suspend
 * @param app_activity The APP-type activity being left
 * @return EOS_OK on success
 */
eos_result_t eos_recent_apps_register_for_suspend(eos_activity_t *app_activity);

#else /* !EOS_RECENT_APPS_ENABLE — stubs */

static inline void eos_recent_apps_init(void)
{
}
static inline eos_result_t eos_recent_apps_suspend_current(void)
{
    return EOS_FAILED;
}
static inline eos_result_t eos_recent_apps_resume(eos_recent_app_entry_t *entry)
{
    LV_UNUSED(entry);
    return EOS_FAILED;
}
static inline eos_result_t eos_recent_apps_resume_by_id(const char *app_id)
{
    LV_UNUSED(app_id);
    return EOS_FAILED;
}
static inline eos_recent_app_entry_t *eos_recent_apps_find(const char *app_id)
{
    LV_UNUSED(app_id);
    return NULL;
}
static inline eos_result_t eos_recent_apps_evict(eos_recent_app_entry_t *entry)
{
    LV_UNUSED(entry);
    return EOS_FAILED;
}
static inline void eos_recent_apps_clear_all(void)
{
}
static inline void eos_recent_apps_on_engine_reset(void)
{
}
static inline void eos_recent_apps_set_timer_strategy(eos_suspend_timer_strategy_t s)
{
    LV_UNUSED(s);
}
static inline void eos_recent_apps_set_anim_strategy(eos_suspend_anim_strategy_t s)
{
    LV_UNUSED(s);
}
static inline uint32_t eos_recent_apps_count(void)
{
    return 0;
}
static inline bool eos_recent_apps_is_suspendable(eos_activity_t *a)
{
    LV_UNUSED(a);
    return false;
}
static inline eos_recent_app_entry_t *eos_recent_apps_get_head(void)
{
    return NULL;
}
static inline eos_recent_app_entry_t *eos_recent_apps_get_next(eos_recent_app_entry_t *e)
{
    LV_UNUSED(e);
    return NULL;
}
static inline eos_result_t eos_recent_apps_register_for_suspend(eos_activity_t *a)
{
    LV_UNUSED(a);
    return EOS_FAILED;
}

#endif /* EOS_RECENT_APPS_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* EOS_RECENT_APPS_H */
