/**
 * @file eos_recent_apps_page.c
 * @brief Recent Apps page Activity implementation
 */

#include "eos_recent_apps_page.h"

#if EOS_RECENT_APPS_ENABLE

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lvgl.h"
#define EOS_LOG_TAG "RecentPage"
#include "eos_log.h"
#include "eos_mem.h"
#include "eos_activity.h"
#include "eos_recent_apps.h"
#include "eos_slide_widget.h"

/* Macros and Definitions -------------------------------------*/
#define _RECENT_CARD_WIDTH 316
#define _RECENT_CARD_HEIGHT 365 /* 316 * 450/390 — keeps the 390:450 screen aspect */
#define _RECENT_CARD_GAP 16
#define _RECENT_LIST_PAD 20
#define _ITEM_CLICK_THRESHOLD 3

/* Typedefs ---------------------------------------------------*/

/** @brief Per-card data stored in lv_obj user_data (entry may be freed first) */
typedef struct
{
    char app_id[64]; /**< Package ID copy — stable after the entry is evicted */
    lv_obj_t *thumb_img; /**< Screenshot image child, detached before evict frees the draw buf */
} _recent_card_data_t;

/* Forward Declarations ---------------------------------------*/
static void _page_on_enter(eos_activity_t *a);
static void _page_on_destroy(eos_activity_t *a);
static void _card_swipe_delete_cb(lv_event_t *e);
static void _card_tap_resume_cb(lv_event_t *e);
static void _card_delete_cb(lv_event_t *e);
static void _build_card_list(lv_obj_t *parent);
static void _update_page_visibility(void);
static void _register_anim_routes(void);
static void _deferred_resume_timer_cb(lv_timer_t *t);

/* Variables --------------------------------------------------*/

static eos_activity_lifecycle_t _page_lifecycle = {
    .on_enter = _page_on_enter,
    .on_destroy = _page_on_destroy,
    .on_pause = NULL,
    .on_resume = NULL,
};

static bool _anim_routes_registered = false;

/** @brief Pending app_id to resume after the recents page closes */
static char s_pending_resume[64] = {0};

/** @brief Empty-state container (hidden when apps exist) */
static lv_obj_t *s_empty_state = NULL;

/** @brief Card list container (hidden when no apps) */
static lv_obj_t *s_card_list = NULL;

/* Function Implementations -----------------------------------*/

static void _register_anim_routes(void)
{
    if (_anim_routes_registered)
        return;
    _anim_routes_registered = true;
    /* No custom animations needed — resume suppresses the app-list zoom
     * directly in eos_activity_reattach_app_substack(). */
}

static void _card_delete_cb(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_target(e);
    _recent_card_data_t *data = lv_obj_get_user_data(card);
    if (data)
    {
        eos_free(data);
        lv_obj_set_user_data(card, NULL);
    }
}

/**
 * @brief Horizontal swipe past threshold — evict the app and remove the card
 *
 * Mirrors eos_msg_list's swipe-to-delete: the slide widget's own animation has
 * already moved the card off-screen when this fires.  The screenshot image is
 * detached first so eos_recent_apps_evict() can free the draw buffer safely.
 */
static void _card_swipe_delete_cb(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_target(e);
    _recent_card_data_t *data = lv_obj_get_user_data(card);
    if (!data)
    {
        EOS_LOG_W("Swipe delete: no card data");
        return;
    }

    EOS_LOG_I("Swipe delete recent app: '%s'", data->app_id);

    if (data->thumb_img)
    {
        lv_image_set_src(data->thumb_img, NULL);
    }

    eos_recent_app_entry_t *entry = eos_recent_apps_find(data->app_id);
    if (entry)
    {
        eos_recent_apps_evict(entry);
    }

    lv_obj_delete_async(card);
    _update_page_visibility();
}

/**
 * @brief Tap (small displacement that reverted) — schedule a resume
 *
 * A swipe has a displacement > _ITEM_CLICK_THRESHOLD, so only genuine taps
 * (or near-taps) reach the resume path.  Same disambiguation as eos_msg_list.
 */
static void _card_tap_resume_cb(lv_event_t *e)
{
    eos_slide_widget_t *sw = (eos_slide_widget_t *)lv_event_get_param(e);
    lv_obj_t *card = lv_event_get_current_target(e);
    _recent_card_data_t *data = lv_obj_get_user_data(card);

    if (!data)
    {
        EOS_LOG_W("Tap resume: no card data");
        return;
    }

    if (abs(eos_slide_widget_get_displacement(sw)) > _ITEM_CLICK_THRESHOLD)
    {
        return;
    }

    EOS_LOG_I("Recent card tapped: '%s' — closing page then resuming", data->app_id);

    /* Store the app_id for deferred resume after the page closes.
     * We must NOT resume here because that re-attaches the app on top of
     * the still-open recents page — back() would then pop the wrong activity. */
    snprintf(s_pending_resume, sizeof(s_pending_resume), "%s", data->app_id);
    eos_activity_back();
}

static lv_obj_t *_create_card(lv_obj_t *parent, eos_recent_app_entry_t *entry)
{
    /* Card container — portrait, screenshot fills the entire card */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, _RECENT_CARD_WIDTH, _RECENT_CARD_HEIGHT);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_clip_corner(card, true, 0);
    lv_obj_set_style_margin_bottom(card, _RECENT_CARD_GAP, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    /* Per-card data: app_id copy (entry may be freed first) + image pointer */
    _recent_card_data_t *data = eos_malloc_zeroed(sizeof(_recent_card_data_t));
    if (!data)
    {
        EOS_LOG_E("Failed to allocate card data");
        lv_obj_del(card);
        return NULL;
    }
    snprintf(data->app_id, sizeof(data->app_id), "%s", entry->app_id);
    lv_obj_set_user_data(card, data);
    lv_obj_add_event_cb(card, _card_delete_cb, LV_EVENT_DELETE, NULL);

    /* App screenshot filling the entire card (no icon/text overlay) */
    if (entry->thumb_buf)
    {
        data->thumb_img = lv_image_create(card);
        lv_image_set_src(data->thumb_img, entry->thumb_buf);
        lv_obj_set_size(data->thumb_img, _RECENT_CARD_WIDTH, _RECENT_CARD_HEIGHT);
        lv_obj_set_style_radius(data->thumb_img, 16, 0);
        lv_obj_remove_flag(data->thumb_img, LV_OBJ_FLAG_CLICKABLE);

        /* Scale the full-resolution screenshot down proportionally to fit the
         * portrait card without cropping.  fit = min(width_scale, height_scale),
         * centered via LV_IMAGE_ALIGN_CENTER so no edge is clipped. */
        uint32_t src_w = entry->thumb_buf->header.w;
        uint32_t src_h = entry->thumb_buf->header.h;
        uint32_t scale_w = (_RECENT_CARD_WIDTH * 256) / src_w;
        uint32_t scale_h = (_RECENT_CARD_HEIGHT * 256) / src_h;
        uint32_t scale = (scale_w < scale_h) ? scale_w : scale_h;
        lv_image_set_scale(data->thumb_img, scale);
        lv_image_set_inner_align(data->thumb_img, LV_IMAGE_ALIGN_CENTER);
    }

    /* Horizontal swipe-to-delete + tap-to-resume (msg_list pattern).
     * The card is a child of an lv_list, so eos_slide_widget uses translate_x
     * and does not fight the flex layout. */
    eos_slide_widget_t *sw = eos_slide_widget_create_with_touch(card,
                                                                card,
                                                                EOS_SLIDE_DIR_HOR,
                                                                EOS_DISPLAY_WIDTH,
                                                                EOS_THRESHOLD_40);
    if (sw)
    {
        eos_slide_widget_set_bidirectional(sw, true);
        eos_slide_widget_add_event_cb_reached_threshold(sw, _card_swipe_delete_cb, NULL);
        eos_slide_widget_add_event_cb_reverted(sw, _card_tap_resume_cb, NULL);
    }

    return card;
}

static void _create_empty_state(lv_obj_t *parent)
{
    s_empty_state = lv_obj_create(parent);
    lv_obj_remove_style_all(s_empty_state);
    lv_obj_set_size(s_empty_state, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(s_empty_state, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(s_empty_state, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *empty_icon = lv_label_create(s_empty_state);
    lv_label_set_text(empty_icon, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(empty_icon, lv_color_hex(0x8E8E93), 0);
    lv_obj_align(empty_icon, LV_ALIGN_CENTER, 0, -24);

    lv_obj_t *empty_label = lv_label_create(s_empty_state);
    lv_label_set_text(empty_label, "No recent apps");
    lv_obj_set_style_text_color(empty_label, lv_color_hex(0x8E8E93), 0);
    lv_obj_align(empty_label, LV_ALIGN_CENTER, 0, 12);
}

static void _update_page_visibility(void)
{
    bool has_apps = eos_recent_apps_count() > 0;

    if (s_empty_state)
    {
        if (has_apps)
            lv_obj_add_flag(s_empty_state, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_remove_flag(s_empty_state, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_card_list)
    {
        if (has_apps)
            lv_obj_remove_flag(s_card_list, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(s_card_list, LV_OBJ_FLAG_HIDDEN);
    }
}

static void _build_card_list(lv_obj_t *parent)
{
    s_empty_state = NULL;
    s_card_list = NULL;

    _create_empty_state(parent);

    /* Vertical lv_list of portrait cards — lv_list_class lets eos_slide_widget
     * slide each card via translate_x (see eos_msg_list.c for the pattern). */
    s_card_list = lv_list_create(parent);
    lv_obj_set_size(s_card_list, lv_pct(92), lv_pct(82));
    lv_obj_align(s_card_list, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_opa(s_card_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_card_list, 0, 0);
    lv_obj_set_style_pad_all(s_card_list, _RECENT_LIST_PAD, 0);
    lv_obj_set_flex_align(s_card_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(s_card_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_card_list, LV_SCROLLBAR_MODE_OFF);

    /* Iterate LRU list and create a card for each entry */
    eos_recent_app_entry_t *entry = eos_recent_apps_get_head();
    while (entry)
    {
        _create_card(s_card_list, entry);
        entry = eos_recent_apps_get_next(entry);
    }

    _update_page_visibility();
}

static void _page_on_enter(eos_activity_t *a)
{
    lv_obj_t *view = eos_activity_get_view(a);
    if (!view)
        return;

    /* Clear any stale pending resume from a previous page instance */
    s_pending_resume[0] = '\0';

    _build_card_list(view);
}

/**
 * @brief Deferred resume timer callback — fires after the activity switch
 *        from closing the recents page has fully completed, so the resume
 *        is not clobbered by _activity_switch_to()'s post-destroy cleanup.
 */
static void _deferred_resume_timer_cb(lv_timer_t *t)
{
    char *app_id = (char *)lv_timer_get_user_data(t);
    if (app_id)
    {
        EOS_LOG_I("Deferred resume for: '%s'", app_id);
        eos_result_t ret = eos_recent_apps_resume_by_id(app_id);
        if (ret != EOS_OK)
        {
            EOS_LOG_W("Deferred resume failed for '%s': %d", app_id, ret);
        }
        eos_free(app_id);
    }
}

static void _page_on_destroy(eos_activity_t *a)
{
    LV_UNUSED(a);
    EOS_LOG_I("Recent Apps page destroyed");

    s_empty_state = NULL;
    s_card_list = NULL;

    /* If a card was tapped, schedule the resume to run on the next LVGL tick.
     * We cannot call eos_recent_apps_resume_by_id() directly here because
     * _page_on_destroy is called from within _activity_switch_to(), which
     * continues executing after we return and would clobber the resume by
     * calling _activity_show()/_activity_mark_visible() on the previous
     * activity (app list), burying the just-resumed app's view. */
    if (s_pending_resume[0] != '\0')
    {
        char *app_id_copy = eos_strdup(s_pending_resume);
        if (app_id_copy)
        {
            lv_timer_t *t = lv_timer_create(_deferred_resume_timer_cb, 0, app_id_copy);
            lv_timer_set_repeat_count(t, 1);
        }
        s_pending_resume[0] = '\0';
    }
}

/* Public API -------------------------------------------------*/

void eos_recent_apps_page_enter(void)
{
    /* Init if not already called (always safe to call — has guard) */
    eos_recent_apps_init();

    if (eos_activity_is_transition_in_progress())
    {
        EOS_LOG_W("Cannot enter recents during transition");
        return;
    }

    _register_anim_routes();

    eos_activity_t *a = eos_activity_create(&_page_lifecycle);
    if (!a)
    {
        EOS_LOG_E("Failed to create recents page activity");
        return;
    }

    lv_obj_t *view = eos_activity_get_view(a);
    lv_obj_set_size(view, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(view, lv_color_black(), 0);
    eos_activity_set_type(a, EOS_ACTIVITY_TYPE_RECENT_APPS);
    eos_activity_set_title(a, "Recent Apps");
    eos_activity_set_app_header_visible(a, false);

    eos_activity_enter(a);
}

#endif /* EOS_RECENT_APPS_ENABLE */
