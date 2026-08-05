/**
 * @file eos_recent_apps_page.c
 * @brief Recent Apps page Activity implementation
 */

#include "eos_recent_apps_page.h"

#if EOS_RECENT_APPS_ENABLE

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#define EOS_LOG_TAG "RecentPage"
#include "eos_log.h"
#include "eos_mem.h"
#include "eos_activity.h"
#include "eos_recent_apps.h"
#include "eos_app_header.h"
#include "eos_theme.h"
#include "eos_anim.h"
#include "eos_overlay_layer.h"
#include "eos_crown.h"
#include "eos_image.h"
#include "eos_service_storage.h"
#include "eos_app.h"

/* Macros and Definitions -------------------------------------*/
#define _RECENT_CARD_WIDTH 340
#define _RECENT_CARD_HEIGHT 160
#define _RECENT_CARD_GAP 12
#define _RECENT_LIST_PAD 20

/* Forward Declarations ---------------------------------------*/
static void _page_on_enter(eos_activity_t *a);
static void _page_on_destroy(eos_activity_t *a);
static void _card_clicked_cb(lv_event_t *e);
static void _build_card_list(lv_obj_t *parent);
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

/* Function Implementations -----------------------------------*/

static void _register_anim_routes(void)
{
    if (_anim_routes_registered)
        return;
    _anim_routes_registered = true;
    /* No custom animations needed — default activity switch handles it. */
}

static void _card_free_app_id(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    void *user_data = lv_obj_get_user_data(obj);
    if (user_data)
    {
        eos_free(user_data);
        lv_obj_set_user_data(obj, NULL);
    }
}

static lv_obj_t *_create_card(lv_obj_t *parent, eos_recent_app_entry_t *entry)
{
    /* Card container */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, _RECENT_CARD_WIDTH, _RECENT_CARD_HEIGHT);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2C2C2E), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    /* Store a COPY of the app_id in user_data (NOT the entry pointer —
     * the entry may be freed before the card is destroyed). */
    char *app_id_copy = eos_strdup(entry->app_id);
    if (!app_id_copy)
    {
        EOS_LOG_E("Failed to copy app_id for card");
        lv_obj_del(card);
        return NULL;
    }
    lv_obj_set_user_data(card, app_id_copy);
    lv_obj_add_event_cb(card, _card_free_app_id, LV_EVENT_DELETE, NULL);

    /* App icon — load from the app's installed icon path, fall back to generic */
    char icon_path[EOS_FS_PATH_MAX];
    snprintf(icon_path, sizeof(icon_path), EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME, entry->app_id);
    const void *icon_src = eos_storage_is_file(icon_path) ? (const void *)icon_path : (const void *)EOS_IMG_APP;
    lv_obj_t *icon = eos_circle_image_create(card, icon_src, 48);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 16, 0);

    /* App name label (display name, not the package ID) */
    lv_obj_t *name_label = lv_label_create(card);
    lv_label_set_text(name_label, entry->app_name);
    lv_obj_set_style_text_color(name_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(name_label, NULL, 0);
    lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 80, -8);

    /* App ID as dimmed sub-label */
    lv_obj_t *id_label = lv_label_create(card);
    lv_label_set_text(id_label, entry->app_id);
    lv_obj_set_style_text_color(id_label, lv_color_hex(0x8E8E93), 0);
    lv_obj_set_style_text_font(id_label, NULL, 0);
    lv_obj_align(id_label, LV_ALIGN_LEFT_MID, 80, 14);

    /* Tap on card → resume app */
    lv_obj_add_event_cb(card, _card_clicked_cb, LV_EVENT_CLICKED, NULL);

    return card;
}

static void _card_clicked_cb(lv_event_t *e)
{
    /* Get the card object directly (the one the event handler is attached to),
     * not the child that was actually clicked. The card always has user_data. */
    lv_obj_t *card = lv_event_get_current_target_obj(e);
    const char *app_id = (const char *)lv_obj_get_user_data(card);

    if (!app_id)
    {
        EOS_LOG_W("No app_id on card");
        return;
    }

    eos_recent_app_entry_t *entry = eos_recent_apps_find(app_id);
    if (!entry)
    {
        EOS_LOG_W("Entry not found for app_id: '%s'", app_id);
        return;
    }

    EOS_LOG_I("Recent card tapped: '%s' — closing page then resuming", app_id);

    /* Store the app_id for deferred resume after the page closes.
     * We must NOT resume here because that re-attaches the app on top of
     * the still-open recents page — back() would then pop the wrong activity. */
    snprintf(s_pending_resume, sizeof(s_pending_resume), "%s", app_id);
    eos_activity_back();
}

static void _build_card_list(lv_obj_t *parent)
{
    uint32_t count = eos_recent_apps_count();
    if (count == 0)
    {
        /* Empty state with icon and hint text */
        lv_obj_t *empty_icon = lv_label_create(parent);
        lv_label_set_text(empty_icon, LV_SYMBOL_LIST);
        lv_obj_set_style_text_color(empty_icon, lv_color_hex(0x8E8E93), 0);
        lv_obj_set_style_text_font(empty_icon, NULL, 0);
        lv_obj_align(empty_icon, LV_ALIGN_CENTER, 0, -24);

        lv_obj_t *empty_label = lv_label_create(parent);
        lv_label_set_text(empty_label, "No recent apps");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x8E8E93), 0);
        lv_obj_set_style_text_font(empty_label, NULL, 0);
        lv_obj_align(empty_label, LV_ALIGN_CENTER, 0, 12);

        lv_obj_t *hint_label = lv_label_create(parent);
        lv_label_set_text(hint_label, "Open apps from the app list\nto see them here");
        lv_obj_set_style_text_color(hint_label, lv_color_hex(0x636366), 0);
        lv_obj_set_style_text_font(hint_label, NULL, 0);
        lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(hint_label, LV_ALIGN_CENTER, 0, 44);
        return;
    }

    /* Build a vertical list of cards */
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_size(list, lv_pct(92), lv_pct(82));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, _RECENT_LIST_PAD, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_gap(list, _RECENT_CARD_GAP, 0);

    /* Iterate LRU list and create a card for each entry */
    eos_recent_app_entry_t *entry = eos_recent_apps_get_head();
    while (entry)
    {
        _create_card(list, entry);
        entry = eos_recent_apps_get_next(entry);
    }
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
