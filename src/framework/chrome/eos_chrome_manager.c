/**
 * @file eos_chrome_manager.c
 * @brief System chrome manager implementation
 */

#include "eos_chrome_manager.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "ChromeMgr"
#include "eos_log.h"
#include "eos_mem.h"
#include "eos_control_center.h"
#include "eos_msg_list.h"
#include "eos_flash_light.h"
#include "eos_activity.h"
#include "eos_app_list.h"
#include "eos_crown.h"
#include "eos_service_pm.h"
#include "eos_service_lock.h"
#include "eos_stack.h"
#include "eos_recent_apps.h"
#include "eos_recent_apps_page.h"
#include "eos_event.h"

/* Macros and Definitions -------------------------------------*/

#define _MAX_OVERLAYS 16

/* Variables --------------------------------------------------*/
static const eos_chrome_overlay_t *_overlays[_MAX_OVERLAYS];
static uint32_t _overlay_count = 0;
static eos_stack_t *_overlay_stack = NULL;
static bool _pending_recents_open = false;

/* Function Implementations -----------------------------------*/

static void _ensure_overlay_on_top(const eos_chrome_overlay_t *overlay)
{
    if (!overlay)
        return;

    lv_obj_t *obj = NULL;

    if (overlay->get_foreground_obj)
    {
        obj = overlay->get_foreground_obj();
    }
    else if (overlay->get_scrollable)
    {
        obj = overlay->get_scrollable();
    }

    if (obj && lv_obj_is_valid(obj))
    {
        lv_obj_move_foreground(obj);
        /* Also raise the overlay's container layer so the entire overlay
         * sits above any direct children of lv_layer_top() */
        lv_obj_t *parent = lv_obj_get_parent(obj);
        if (parent && lv_obj_is_valid(parent))
        {
            lv_obj_move_foreground(parent);
        }
        EOS_LOG_D("Brought overlay to front[%s]", overlay->name ? overlay->name : "unknown");
    }
}

static void _activate_crown_for_overlay(const eos_chrome_overlay_t *overlay)
{
    if (!overlay)
    {
        eos_crown_encoder_set_target_view(eos_view_active());
        return;
    }

    if (overlay->get_scrollable)
    {
        lv_obj_t *scrollable = overlay->get_scrollable();
        if (scrollable && lv_obj_is_valid(scrollable))
        {
            eos_crown_encoder_set_target_obj(scrollable);
            EOS_LOG_D("Crown activated for overlay[%s]", overlay->name ? overlay->name : "unknown");
            return;
        }
    }

    eos_crown_encoder_set_target_view(eos_view_active());
}

static void _focus_top_overlay(void)
{
    if (!_overlay_stack)
        return;

    const eos_chrome_overlay_t *top = (const eos_chrome_overlay_t *)eos_stack_peek(_overlay_stack);
    if (!top)
        return;

    EOS_LOG_I("Focus changed to overlay[%s]", top->name ? top->name : "unknown");

    if (top->on_focus)
    {
        top->on_focus();
    }

    _ensure_overlay_on_top(top);
    _activate_crown_for_overlay(top);
}

void eos_chrome_manager_register_overlay(const eos_chrome_overlay_t *overlay)
{
    if (!overlay || !overlay->pull_back || !overlay->hide)
    {
        EOS_LOG_E("Invalid overlay registration");
        return;
    }

    if (_overlay_count >= _MAX_OVERLAYS)
    {
        EOS_LOG_E("Max overlays reached (%d)", _MAX_OVERLAYS);
        return;
    }

    _overlays[_overlay_count++] = overlay;
    EOS_LOG_I("Overlay registered[%p] (total=%d)", overlay, _overlay_count);
}

bool eos_chrome_manager_any_overlay_open(void)
{
    return eos_stack_get_size(_overlay_stack) > 0;
}

const eos_chrome_overlay_t *eos_chrome_manager_get_top_overlay(void)
{
    return (const eos_chrome_overlay_t *)eos_stack_peek(_overlay_stack);
}

void eos_chrome_manager_pull_back_top(void)
{
    const eos_chrome_overlay_t *top = (const eos_chrome_overlay_t *)eos_stack_peek(_overlay_stack);
    if (top)
    {
        EOS_LOG_I("Pulling back top overlay[%p]", top);
        top->pull_back();
    }
}

void eos_chrome_manager_pull_back_all(void)
{
    const eos_chrome_overlay_t *overlay;
    while ((overlay = (const eos_chrome_overlay_t *)eos_stack_pop(_overlay_stack)) != NULL)
    {
        EOS_LOG_I("Pulling back overlay[%p]", overlay);
        overlay->pull_back();
    }
}

void eos_chrome_manager_push_overlay(const eos_chrome_overlay_t *overlay)
{
    if (!overlay)
        return;

    const eos_chrome_overlay_t *top = (const eos_chrome_overlay_t *)eos_stack_peek(_overlay_stack);
    if (top == overlay)
    {
        EOS_LOG_W("Overlay[%p] is already on top of stack", overlay);
        return;
    }

    eos_chrome_manager_remove_overlay(overlay);

    EOS_LOG_I("Pushing overlay[%p] to stack", overlay);
    eos_stack_push(_overlay_stack, (void *)overlay);
    _focus_top_overlay();
}

void eos_chrome_manager_remove_overlay(const eos_chrome_overlay_t *overlay)
{
    if (!overlay || !_overlay_stack)
        return;

    size_t size = eos_stack_get_size(_overlay_stack);
    if (size == 0)
        return;

    const eos_chrome_overlay_t *top_before = (const eos_chrome_overlay_t *)eos_stack_peek(_overlay_stack);
    bool removed = false;

    const eos_chrome_overlay_t **items = eos_malloc_zeroed(size * sizeof(*items));
    EOS_CHECK_PTR_RETURN(items);

    for (size_t i = 0; i < size; i++)
    {
        items[i] = (const eos_chrome_overlay_t *)eos_stack_pop(_overlay_stack);
    }

    for (size_t i = size; i > 0; i--)
    {
        const eos_chrome_overlay_t *item = items[i - 1];
        if (item == overlay)
        {
            removed = true;
            EOS_LOG_I("Removing overlay[%p] from stack", overlay);
            continue;
        }

        eos_stack_push(_overlay_stack, (void *)item);
    }

    eos_free(items);

    if (!removed)
        return;

    if (top_before == overlay)
    {
        _focus_top_overlay();
    }
}

void eos_chrome_manager_handle_crown_click(void)
{
    /* Wake up first if sleeping — always allow crown to wake, even on lock screen */
    if (eos_pm_get_state() == EOS_PM_SLEEP)
    {
        eos_pm_wake_up();
        return;
    }

    /* Don't allow crown click to bypass lock screen */
    if (eos_lock_screen_is_active())
    {
        return;
    }

    eos_pm_reset_timer();

    if (eos_chrome_manager_any_overlay_open())
    {
        eos_chrome_manager_pull_back_top();
        return;
    }

    eos_activity_t *current = eos_activity_get_current();
    eos_activity_t *visible = eos_activity_get_visible();
    eos_activity_t *watchface = eos_activity_get_watchface();
    bool from_watchface = (current == watchface);

    EOS_LOG_I(
        "Crown click: current=%p[type=%d] visible=%p[type=%d] watchface=%p[type=%d] from_watchface=%d transition=%d",
        (void *)current,
        current ? eos_activity_get_type(current) : -1,
        (void *)visible,
        visible ? eos_activity_get_type(visible) : -1,
        (void *)watchface,
        watchface ? eos_activity_get_type(watchface) : -1,
        from_watchface,
        eos_activity_is_transition_in_progress());

    if (from_watchface)
    {
        eos_app_list_enter();
    }
    else
    {
        eos_activity_back();
    }
}

/**
 * @brief Event callback: transition completed, open recents if deferred.
 */
static void _recents_deferred_open_cb(eos_event_t *e)
{
    LV_UNUSED(e);
    if (!_pending_recents_open)
        return;
    _pending_recents_open = false;

    eos_activity_t *current = eos_activity_get_current();
    if (current && eos_activity_get_type(current) == EOS_ACTIVITY_TYPE_RECENT_APPS)
    {
        /* Recents page is already open (race) — nothing to do */
        return;
    }

    if (current && eos_recent_apps_is_suspendable(current))
    {
        eos_recent_apps_suspend_current();
    }
    eos_recent_apps_page_enter();
}

void eos_chrome_manager_handle_crown_double_click(void)
{
    /* Wake up first if sleeping */
    if (eos_pm_get_state() == EOS_PM_SLEEP)
    {
        eos_pm_wake_up();
        return;
    }

    /* Don't allow crown double-click to bypass lock screen */
    if (eos_lock_screen_is_active())
    {
        return;
    }

    eos_pm_reset_timer();

    /* If any overlay is open, pull back topmost */
    if (eos_chrome_manager_any_overlay_open())
    {
        eos_chrome_manager_pull_back_top();
        return;
    }

    eos_activity_t *current = eos_activity_get_current();

    /* Toggle: if already on recents page, close it; otherwise open */
    if (current && eos_activity_get_type(current) == EOS_ACTIVITY_TYPE_RECENT_APPS)
    {
        eos_activity_back();
        return;
    }

    /* If a transition is in progress (e.g. the single-click back() from
     * the first crown press is still animating), defer the recents open
     * until the transition completes. The event callback will open it. */
    if (eos_activity_is_transition_in_progress())
    {
        EOS_LOG_I("Crown double-click during transition — deferring recents open");
        _pending_recents_open = true;
        return;
    }

    /* If currently inside a suspendable script app, suspend it first
     * so it appears in the recents list when the page opens. */
    if (current && eos_recent_apps_is_suspendable(current))
    {
        eos_recent_apps_suspend_current();
    }
    eos_recent_apps_page_enter();
}

void eos_chrome_manager_handle_activity_switch(void)
{
    eos_chrome_manager_pull_back_all();

    for (uint32_t i = 0; i < _overlay_count; i++)
    {
        _overlays[i]->hide();
    }
}

void eos_chrome_manager_init(void)
{
    _overlay_count = 0;

    if (_overlay_stack)
        eos_stack_destroy(_overlay_stack);
    _overlay_stack = eos_stack_create(4);

    eos_chrome_manager_register_overlay(eos_control_center_get_overlay_descriptor());
    eos_chrome_manager_register_overlay(eos_msg_list_get_overlay_descriptor());
    eos_chrome_manager_register_overlay(eos_flash_light_get_overlay_descriptor());

    /* Subscribe to screen-switched events for deferred recents-open.
     * When crown double-click arrives during a transition (from the
     * immediate-back first click), we defer opening recents until the
     * transition completes and this event fires. */
    eos_event_subscribe_ex(EOS_EVENT_ACTIVITY_SCREEN_SWITCHED, _recents_deferred_open_cb, NULL, NULL);

    EOS_LOG_I("Chrome manager initialized");
}

/* Helper functions for overlays to notify opening */
void eos_chrome_manager_notify_overlay_opened(const eos_chrome_overlay_t *overlay)
{
    if (!overlay)
    {
        EOS_LOG_W("Overlay notify opened with null pointer");
        return;
    }

    eos_chrome_manager_push_overlay(overlay);
}

void eos_chrome_manager_notify_overlay_closed(const eos_chrome_overlay_t *overlay)
{
    if (!overlay)
    {
        EOS_LOG_W("Overlay notify closed with null pointer");
        return;
    }

    eos_chrome_manager_remove_overlay(overlay);
}
