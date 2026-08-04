/**
 * @file eos_recent_apps_page.c
 * @brief Recent Apps page Activity implementation
 */

#include "eos_recent_apps_page.h"

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

/* Variables --------------------------------------------------*/

static eos_activity_lifecycle_t _page_lifecycle = {
    .on_enter = _page_on_enter,
    .on_destroy = _page_on_destroy,
    .on_pause = NULL,
    .on_resume = NULL,
};

static bool _anim_routes_registered = false;

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

static lv_obj_t *_create_card(lv_obj_t *parent, eos_recent_app_entry_t *entry, int index)
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
    lv_obj_set_user_data(card, app_id_copy);
    lv_obj_add_event_cb(card, _card_free_app_id, LV_EVENT_DELETE, NULL);

    /* App icon as placeholder left of card (screenshot unreliable via lv_image) */
    lv_obj_t *icon = lv_image_create(card);
    /* Use the app's icon if available, otherwise show a colored placeholder */
    lv_obj_set_size(icon, 48, 48);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_set_style_bg_color(icon, lv_color_hex(0x007AFF), 0);
    lv_obj_set_style_bg_opa(icon, LV_OPA_30, 0);
    lv_obj_set_style_radius(icon, 12, 0);

    /* App name label */
    lv_obj_t *name_label = lv_label_create(card);
    lv_label_set_text(name_label, entry->app_id);
    lv_obj_set_style_text_color(name_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(name_label, NULL, 0);
    lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 80, 0);

    /* Tap on card → resume app */
    lv_obj_add_event_cb(card, _card_clicked_cb, LV_EVENT_CLICKED, NULL);

    return card;
}

static void _card_clicked_cb(lv_event_t *e)
{
    EOS_LOG_I("Card clicked!");

    lv_obj_t *target = lv_event_get_target_obj(e);
    EOS_LOG_I("Click target: %p", (void *)target);

    /* Walk up to find the card (which has user_data = app_id) */
    lv_obj_t *obj = target;
    const char *app_id = NULL;
    while (obj)
    {
        app_id = (const char *)lv_obj_get_user_data(obj);
        if (app_id)
            break;
        obj = lv_obj_get_parent(obj);
    }

    if (!app_id)
    {
        EOS_LOG_W("No app_id found in click chain");
        return;
    }

    EOS_LOG_I("App ID from click: '%s'", app_id);

    eos_recent_app_entry_t *entry = eos_recent_apps_find(app_id);
    if (!entry)
    {
        EOS_LOG_W("Entry not found for app_id: '%s'", app_id);
        return;
    }

    EOS_LOG_I("Resuming app: '%s'", app_id);
    eos_result_t ret = eos_recent_apps_resume(entry);
    EOS_LOG_I("Resume returned: %d", ret);

    EOS_LOG_I("Closing recents page...");
    eos_activity_back();
    EOS_LOG_I("Back returned");
}

static void _build_card_list(lv_obj_t *parent)
{
    uint32_t count = eos_recent_apps_count();
    if (count == 0)
    {
        /* Empty state */
        lv_obj_t *empty_label = lv_label_create(parent);
        lv_label_set_text(empty_label, "No recent apps");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x8E8E93), 0);
        lv_obj_set_style_text_font(empty_label, NULL, 0);
        lv_obj_center(empty_label);
        return;
    }

    /* Build a vertical list of cards */
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_size(list, lv_pct(90), lv_pct(85));
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, _RECENT_LIST_PAD, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    /* Title */
    lv_obj_t *title = lv_label_create(list);
    lv_label_set_text(title, "Recent Apps");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, NULL, 0);

    /* Iterate LRU list and create a card for each entry */
    int index = 0;
    eos_recent_app_entry_t *entry = eos_recent_apps_get_head();
    while (entry)
    {
        lv_obj_t *card = _create_card(list, entry, index);
        LV_UNUSED(card);
        index++;
        entry = eos_recent_apps_get_next(entry);
    }
}

static void _page_on_enter(eos_activity_t *a)
{
    lv_obj_t *view = eos_activity_get_view(a);
    if (!view)
        return;

    _build_card_list(view);
}

static void _page_on_destroy(eos_activity_t *a)
{
    LV_UNUSED(a);
    EOS_LOG_I("Recent Apps page destroyed");
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
