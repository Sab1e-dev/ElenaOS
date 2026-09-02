/**
 * @file eos_recent_apps_page.c
 * @brief Recent Apps page Activity implementation
 */

#include "eos_recent_apps_page.h"

#if EOS_RECENT_APPS_ENABLE

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#define EOS_LOG_TAG "RecentPage"
#include "eos_log.h"
#include "eos_mem.h"
#include "eos_activity.h"
#include "eos_crown.h"
#include "eos_recent_apps.h"
#include "eos_app_list.h"
#include "eos_slide_widget.h"
#include "eos_card_stack.h"
#include "eos_image.h"
#include "eos_service_storage.h"
#include "eos_app.h"
#include "eos_theme.h"
#include "eos_lang.h"

/* Macros and Definitions -------------------------------------*/
#define _RECENT_CARD_WIDTH 300
#define _RECENT_CARD_HEIGHT \
    ((_RECENT_CARD_WIDTH * EOS_DISPLAY_HEIGHT) / EOS_DISPLAY_WIDTH) /* 300 → 346，保持 390:450 */
#define _RECENT_STACK_TOP 28
#define _RECENT_STACK_STEP 54
#define _RECENT_SLOT_COUNT 4
#define _RECENT_DELETE_SIZE 84
#define _RECENT_DELETE_WIDTH _RECENT_DELETE_SIZE
#define _RECENT_DELETE_HEIGHT _RECENT_DELETE_SIZE
#define _RECENT_DELETE_RADIUS 8
#define _RECENT_CARD_BASE_X _RECENT_DELETE_WIDTH
#define _RECENT_WRAPPER_WIDTH (_RECENT_CARD_WIDTH + (_RECENT_DELETE_WIDTH * 2))
#define _RECENT_STACK_ANIM_DURATION 220
#define _RECENT_BORDER_WIDTH 2
#define _RECENT_BORDER_COLOR 0x48484A
#define _RECENT_ICON_SIZE 44
#define _RECENT_ICON_MARGIN 12
#define _ITEM_CLICK_THRESHOLD 3

/* Typedefs ---------------------------------------------------*/

/** @brief Per-card data stored in lv_obj user_data (entry may be freed first) */
typedef struct _recent_card_data_t
{
    char app_id[64]; /**< Package ID copy — stable after the entry is evicted */
    lv_obj_t *card; /**< Screenshot card */
    lv_obj_t *thumb_img; /**< Screenshot image, detached before eviction */
    lv_obj_t *delete_btn; /**< Delete button revealed by horizontal sliding */
    eos_card_stack_item_t *stack_item; /**< Generic stack item owning the wrapper */
    eos_slide_widget_t *slide; /**< Horizontal delete-reveal controller */
    lv_coord_t gesture_start_x;
    lv_coord_t gesture_start_y;
    bool suppress_tap;
    bool deleting;
    struct _recent_card_data_t *next;
} _recent_card_data_t;

/* Forward Declarations ---------------------------------------*/
static void _page_on_enter(eos_activity_t *a);
static void _page_on_destroy(eos_activity_t *a);
static void _card_delete_cb(lv_event_t *e);
static void _card_swipe_opened_cb(lv_event_t *e);
static void _card_swipe_closed_cb(lv_event_t *e);
static void _card_swipe_moving_cb(lv_event_t *e);
static void _card_tap_resume_cb(lv_event_t *e);
static void _delete_button_cb(lv_event_t *e);
static void _stack_focus_changed_cb(eos_card_stack_t *stack, uint32_t focus_index, void *user_data);
static void _build_card_list(lv_obj_t *parent);
static void _update_page_visibility(void);
static void _register_anim_routes(void);
static void _deferred_resume_timer_cb(lv_timer_t *t);
static void _size_thumb_timer_cb(lv_timer_t *t);

static const void *_recent_get_icon_src(const char *app_id, char *icon_path, size_t icon_path_size)
{
    if (!app_id || !icon_path || icon_path_size == 0U)
        return EOS_IMG_APP;

    for (uint32_t i = 0; i < EOS_SYS_APP_LAST; i++)
    {
        if (strcmp(app_id, eos_sys_app_id_list[i]) == 0)
            return eos_sys_app_icon_list[i];
    }

    snprintf(icon_path, icon_path_size, EOS_APP_INSTALLED_DIR "%s/" EOS_APP_ICON_FILE_NAME, app_id);
    return eos_storage_is_file(icon_path) ? (const void *)icon_path : (const void *)EOS_IMG_APP;
}

/* Variables --------------------------------------------------*/

static eos_activity_lifecycle_t _page_lifecycle = {
    .on_enter = _page_on_enter,
    .on_destroy = _page_on_destroy,
    .on_pause = NULL,
    .on_resume = NULL,
};

static bool _anim_routes_registered = false;
static char s_pending_resume[64] = {0};
static lv_obj_t *s_empty_state = NULL;
static eos_card_stack_t *s_stack = NULL;
static lv_obj_t *s_card_list = NULL;
static _recent_card_data_t *s_open_card = NULL;
static _recent_card_data_t *s_cards = NULL;

/* Function Implementations -----------------------------------*/

static void _unlink_card_data(_recent_card_data_t *data)
{
    if (!data)
        return;

    _recent_card_data_t **cursor = &s_cards;
    while (*cursor)
    {
        if (*cursor == data)
        {
            *cursor = data->next;
            data->next = NULL;
            return;
        }
        cursor = &(*cursor)->next;
    }
}

static void _register_anim_routes(void)
{
    if (_anim_routes_registered)
        return;
    _anim_routes_registered = true;
}

static void _card_delete_cb(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_target(e);
    _recent_card_data_t *data = lv_obj_get_user_data(card);
    if (!data)
        return;

    if (s_open_card == data)
        s_open_card = NULL;
    _unlink_card_data(data);
    lv_obj_set_user_data(card, NULL);
    eos_free(data);
}

/**
 * @brief Left swipe completed — leave the card open so its delete button can be tapped
 */
static void _card_swipe_opened_cb(lv_event_t *e)
{
    _recent_card_data_t *data = lv_event_get_user_data(e);
    if (!data || data->deleting || !s_stack || !eos_card_stack_item_is_focused(s_stack, data->stack_item))
        return;

    data->suppress_tap = true;
    s_open_card = data;
    eos_card_stack_set_vertical_gesture_enabled(s_stack, false);
    eos_card_stack_set_side_reveal(s_stack, data->stack_item, LV_OPA_COVER);
    EOS_LOG_I("Recent card opened for delete: '%s'", data->app_id);
}

static void _card_swipe_closed_cb(lv_event_t *e)
{
    _recent_card_data_t *data = lv_event_get_user_data(e);
    if (!data || data->deleting)
        return;
    eos_card_stack_set_side_reveal(s_stack, NULL, 0);
    eos_card_stack_set_vertical_gesture_enabled(s_stack, true);
}

static void _card_swipe_moving_cb(lv_event_t *e)
{
    _recent_card_data_t *data = lv_event_get_user_data(e);
    eos_slide_widget_t *sw = data ? data->slide : NULL;
    if (!data || !sw || !data->delete_btn || !lv_obj_is_valid(data->delete_btn))
        return;
    if (!s_stack || !eos_card_stack_is_settled(s_stack) || !eos_card_stack_item_is_focused(s_stack, data->stack_item))
    {
        if (s_stack)
            eos_card_stack_set_side_reveal(s_stack, NULL, 0);
        lv_obj_set_style_opa(data->delete_btn, LV_OPA_TRANSP, 0);
        return;
    }

    lv_coord_t range = eos_slide_widget_get_target(sw) - eos_slide_widget_get_base(sw);
    lv_coord_t displacement = eos_slide_widget_get_current_pos(sw) - eos_slide_widget_get_base(sw);
    lv_opa_t opacity = LV_OPA_TRANSP;
    bool moving_left = displacement < 0;
    if (range != 0 && moving_left)
    {
        int32_t progress = abs(displacement) * LV_OPA_COVER / abs(range);
        if (progress > LV_OPA_COVER)
            progress = LV_OPA_COVER;
        opacity = (lv_opa_t)progress;
        eos_card_stack_set_side_reveal(s_stack, data->stack_item, (uint16_t)progress);
    }
    else
    {
        eos_card_stack_set_side_reveal(s_stack, NULL, 0);
    }
    lv_obj_set_style_opa(data->delete_btn, opacity, 0);
}

static void _close_open_card(_recent_card_data_t *except)
{
    if (!s_open_card || s_open_card == except || s_open_card->deleting || !s_open_card->slide)
        return;

    eos_slide_widget_move(s_open_card->slide,
                          eos_slide_widget_get_current_pos(s_open_card->slide),
                          eos_slide_widget_get_base(s_open_card->slide),
                          120);
    s_open_card = NULL;
}

/**
 * @brief Delete a recent app after the user taps the visible × button
 */
static void _delete_recent_card(_recent_card_data_t *data)
{
    if (!data || data->deleting)
        return;

    data->deleting = true;
    if (s_open_card == data)
        s_open_card = NULL;

    EOS_LOG_I("Delete recent app: '%s'", data->app_id);
    if (data->thumb_img)
        lv_image_set_src(data->thumb_img, NULL);

    eos_recent_app_entry_t *entry = eos_recent_apps_find(data->app_id);
    if (entry)
        eos_recent_apps_evict(entry);

    eos_card_stack_set_side_reveal(s_stack, NULL, 0);
    if (data->stack_item)
        eos_card_stack_remove(data->stack_item);
    _update_page_visibility();
}

static void _delete_button_cb(lv_event_t *e)
{
    _delete_recent_card(lv_event_get_user_data(e));
}

/**
 * @brief Tap (small displacement that reverted) — schedule a resume
 */
static void _card_tap_resume_cb(lv_event_t *e)
{
    eos_slide_widget_t *sw = (eos_slide_widget_t *)lv_event_get_param(e);
    _recent_card_data_t *data = lv_event_get_user_data(e);
    if (!data || data->deleting || data->suppress_tap || !sw || !s_stack
        || !eos_card_stack_item_is_focused(s_stack, data->stack_item))
        return;

    if (abs(eos_slide_widget_get_displacement(sw)) > _ITEM_CLICK_THRESHOLD)
        return;

    EOS_LOG_I("Recent card tapped: '%s' — closing page then resuming", data->app_id);
    snprintf(s_pending_resume, sizeof(s_pending_resume), "%s", data->app_id);
    eos_activity_back();
}

static void _card_pressed_cb(lv_event_t *e)
{
    _recent_card_data_t *data = lv_event_get_user_data(e);
    if (!data || data->deleting)
        return;
    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);
    data->gesture_start_x = point.x;
    data->gesture_start_y = point.y;
    data->suppress_tap = false;
    bool focused =
        s_stack && eos_card_stack_is_settled(s_stack) && eos_card_stack_item_is_focused(s_stack, data->stack_item);
    if (data->slide)
        eos_slide_widget_set_enabled(data->slide, focused);
    if (!focused)
    {
        data->suppress_tap = true;
        return;
    }
    _close_open_card(data);
}

static void _card_pressing_cb(lv_event_t *e)
{
    _recent_card_data_t *data = lv_event_get_user_data(e);
    if (!data || data->deleting)
        return;
    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);
    lv_coord_t dx = point.x - data->gesture_start_x;
    lv_coord_t dy = point.y - data->gesture_start_y;
    if (abs(dx) + abs(dy) >= 8 && abs(dy) > abs(dx))
        data->suppress_tap = true;
}

static void _card_released_cb(lv_event_t *e)
{
    _recent_card_data_t *data = lv_event_get_user_data(e);
    if (!data || data->deleting)
        return;
    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);
    lv_coord_t dx = point.x - data->gesture_start_x;
    lv_coord_t dy = point.y - data->gesture_start_y;
    if (abs(dx) + abs(dy) >= 8)
        data->suppress_tap = true;
}

static void _stack_focus_changed_cb(eos_card_stack_t *stack, uint32_t focus_index, void *user_data)
{
    LV_UNUSED(focus_index);
    LV_UNUSED(user_data);
    if (!stack)
        return;

    for (_recent_card_data_t *data = s_cards; data; data = data->next)
    {
        bool focused = !data->deleting && eos_card_stack_is_settled(stack)
                       && eos_card_stack_item_is_focused(stack, data->stack_item);
        if (data->slide)
            eos_slide_widget_set_enabled(data->slide, focused);
        if (!focused)
            data->suppress_tap = true;
    }
}

static void _size_thumb_timer_cb(lv_timer_t *t)
{
    lv_obj_t *img = (lv_obj_t *)lv_timer_get_user_data(t);
    if (!img || !lv_obj_is_valid(img))
        return;

    lv_obj_t *card = lv_obj_get_parent(img);
    if (!card || !lv_obj_is_valid(card))
        return;

    lv_obj_update_layout(card);
    eos_img_set_size(img, (uint32_t)lv_obj_get_content_width(card), (uint32_t)lv_obj_get_content_height(card));
}

static _recent_card_data_t *_create_card(eos_card_stack_t *stack, eos_recent_app_entry_t *entry)
{
    lv_obj_t *stack_container = eos_card_stack_get_container(stack);
    lv_obj_t *card = lv_obj_create(stack_container);
    if (!card)
        return NULL;

    _recent_card_data_t *data = eos_malloc_zeroed(sizeof(_recent_card_data_t));
    if (!data)
    {
        lv_obj_delete(card);
        return NULL;
    }
    snprintf(data->app_id, sizeof(data->app_id), "%s", entry->app_id);
    data->card = card;
    lv_obj_set_size(card, _RECENT_CARD_WIDTH, _RECENT_CARD_HEIGHT);
    lv_obj_set_style_radius(card, EOS_DISPLAY_RADIUS, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_border_width(card, _RECENT_BORDER_WIDTH, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(_RECENT_BORDER_COLOR), 0);
    lv_obj_set_style_pad_all(card, _RECENT_BORDER_WIDTH, 0);
    lv_obj_set_style_clip_corner(card, true, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(card, data);
    lv_obj_add_event_cb(card, _card_delete_cb, LV_EVENT_DELETE, NULL);

    if (entry->thumb_buf)
    {
        data->thumb_img = lv_image_create(card);
        lv_image_set_src(data->thumb_img, entry->thumb_buf);
        lv_obj_remove_flag(data->thumb_img, LV_OBJ_FLAG_CLICKABLE);
        lv_timer_t *t = lv_timer_create(_size_thumb_timer_cb, 0, data->thumb_img);
        if (t)
            lv_timer_set_repeat_count(t, 1);
    }

    {
        char icon_path[EOS_FS_PATH_MAX];
        const void *icon_src = _recent_get_icon_src(entry->app_id, icon_path, sizeof(icon_path));
        lv_obj_t *icon = eos_circle_image_create(card, icon_src, _RECENT_ICON_SIZE);
        if (icon)
        {
            lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(icon, EOS_COLOR_ICON_BG, 0);
            lv_obj_align(icon, LV_ALIGN_TOP_LEFT, _RECENT_ICON_MARGIN, _RECENT_ICON_MARGIN);
        }
    }

    data->slide = eos_slide_widget_create(card, card, EOS_SLIDE_DIR_HOR, -_RECENT_DELETE_WIDTH, EOS_THRESHOLD_40);
    if (!data->slide)
    {
        lv_obj_set_user_data(card, NULL);
        lv_obj_delete(card);
        eos_free(data);
        return NULL;
    }
    eos_slide_widget_set_close_on_reverse(data->slide, true);
    eos_slide_widget_set_drag_factor(data->slide, 160);
    lv_obj_t *touch = eos_slide_widget_get_touch_obj(data->slide);
    lv_obj_set_size(touch, _RECENT_CARD_WIDTH, _RECENT_CARD_HEIGHT);
    lv_obj_set_pos(touch, 0, 0);
    lv_obj_add_flag(touch, LV_OBJ_FLAG_CLICKABLE);
    eos_slide_widget_add_event_cb_reached_threshold(data->slide, _card_swipe_opened_cb, data);
    eos_slide_widget_add_event_cb_closed(data->slide, _card_swipe_closed_cb, data);
    eos_slide_widget_add_event_cb_moving(data->slide, _card_swipe_moving_cb, data);
    eos_slide_widget_add_event_cb_reverted(data->slide, _card_tap_resume_cb, data);
    lv_obj_add_event_cb(touch, _card_pressed_cb, LV_EVENT_PRESSED, data);
    lv_obj_add_event_cb(touch, _card_pressing_cb, LV_EVENT_PRESSING, data);
    lv_obj_add_event_cb(touch, _card_released_cb, LV_EVENT_RELEASED, data);

    data->stack_item = eos_card_stack_add(stack, card, touch);
    if (!data->stack_item)
    {
        lv_obj_set_user_data(card, NULL);
        lv_obj_delete(card);
        eos_free(data);
        return NULL;
    }
    eos_slide_widget_set_range(data->slide, _RECENT_CARD_BASE_X, 0);
    eos_slide_widget_set_enabled(data->slide, eos_card_stack_item_is_focused(stack, data->stack_item));

    lv_obj_t *wrapper = eos_card_stack_item_get_container(data->stack_item);
    lv_obj_t *delete_btn = lv_button_create(wrapper);
    if (delete_btn)
    {
        data->delete_btn = delete_btn;
        lv_obj_set_size(delete_btn, _RECENT_DELETE_WIDTH, _RECENT_DELETE_HEIGHT);
        lv_obj_set_pos(delete_btn,
                       _RECENT_CARD_BASE_X + _RECENT_CARD_WIDTH - _RECENT_DELETE_WIDTH,
                       (_RECENT_CARD_HEIGHT - _RECENT_DELETE_HEIGHT) / 2);
        lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0xFF3B30), 0);
        lv_obj_set_style_bg_opa(delete_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_opa(delete_btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(delete_btn, _RECENT_DELETE_RADIUS, 0);
        lv_obj_set_style_border_width(delete_btn, 0, 0);
        lv_obj_set_style_pad_all(delete_btn, 0, 0);
        lv_obj_t *delete_label = lv_label_create(delete_btn);
        lv_label_set_text(delete_label, LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(delete_label, lv_color_white(), 0);
        lv_obj_center(delete_label);
        lv_obj_move_to_index(delete_btn, 0);
        lv_obj_add_event_cb(delete_btn, _delete_button_cb, LV_EVENT_CLICKED, data);
    }
    data->next = s_cards;
    s_cards = data;
    return data;
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
    eos_label_set_text_id(empty_label, STR_ID_RECENT_APPS_EMPTY);
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
    s_stack = NULL;
    s_card_list = NULL;
    s_open_card = NULL;
    _create_empty_state(parent);

    eos_card_stack_config_t config = {
        .card_width = _RECENT_CARD_WIDTH,
        .card_height = _RECENT_CARD_HEIGHT,
        .wrapper_width = _RECENT_WRAPPER_WIDTH,
        .card_x = _RECENT_CARD_BASE_X,
        .top = _RECENT_STACK_TOP,
        .step = _RECENT_STACK_STEP,
        .slot_count = _RECENT_SLOT_COUNT,
        .slots =
            {
                {.y = 104, .scale = 256, .opacity = LV_OPA_COVER},
                {.y = 62, .scale = 228, .opacity = LV_OPA_COVER},
                {.y = 30, .scale = 204, .opacity = LV_OPA_90},
                {.y = 4, .scale = 184, .opacity = LV_OPA_70},
            },
        .previous_slot = {.y = EOS_DISPLAY_HEIGHT - 64, .scale = 256, .opacity = LV_OPA_COVER},
        .exit_slot = {.y = EOS_DISPLAY_HEIGHT + 40, .scale = 256, .opacity = LV_OPA_COVER},
        /* Require a larger finger travel before the stack reaches the next
         * slot; crown scrolling keeps its independent one-step mapping. */
        .vertical_drag_factor = 128,
        .animation_duration = _RECENT_STACK_ANIM_DURATION,
    };
    s_stack = eos_card_stack_create(parent, &config);
    if (!s_stack)
    {
        EOS_LOG_E("Failed to create recent card stack");
        _update_page_visibility();
        return;
    }
    eos_card_stack_set_focus_changed_cb(s_stack, _stack_focus_changed_cb, NULL);
    s_card_list = eos_card_stack_get_container(s_stack);

    eos_recent_app_entry_t *entry = eos_recent_apps_get_head();
    while (entry)
    {
        _create_card(s_stack, entry);
        entry = eos_recent_apps_get_next(entry);
    }
    _update_page_visibility();
}

static void _page_on_enter(eos_activity_t *a)
{
    lv_obj_t *view = eos_activity_get_view(a);
    if (!view)
        return;
    s_pending_resume[0] = '\0';
    _build_card_list(view);
}

static void _deferred_resume_timer_cb(lv_timer_t *t)
{
    char *app_id = (char *)lv_timer_get_user_data(t);
    if (app_id)
    {
        EOS_LOG_I("Deferred resume for: '%s'", app_id);
        eos_result_t ret = eos_recent_apps_resume_by_id(app_id);
        if (ret != EOS_OK)
            EOS_LOG_W("Deferred resume failed for '%s': %d", app_id, ret);
        eos_free(app_id);
    }
}

static void _page_on_destroy(eos_activity_t *a)
{
    LV_UNUSED(a);
    EOS_LOG_I("Recent Apps page destroyed");
    if (s_stack)
        eos_card_stack_delete(s_stack);
    s_stack = NULL;
    s_cards = NULL;
    s_card_list = NULL;
    s_empty_state = NULL;
    s_open_card = NULL;

    if (s_pending_resume[0] != '\0')
    {
        char *app_id_copy = eos_strdup(s_pending_resume);
        if (app_id_copy)
        {
            lv_timer_t *t = lv_timer_create(_deferred_resume_timer_cb, 0, app_id_copy);
            if (!t)
                eos_free(app_id_copy);
            else
                lv_timer_set_repeat_count(t, 1);
        }
        s_pending_resume[0] = '\0';
    }
}

/* Public API -------------------------------------------------*/

void eos_recent_apps_page_enter(void)
{
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
    lv_obj_add_flag(view, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    eos_activity_set_type(a, EOS_ACTIVITY_TYPE_RECENT_APPS);
    eos_activity_set_app_header_visible(a, false);
    eos_activity_enter(a);
    if (s_stack)
        eos_crown_encoder_set_target_obj(eos_card_stack_get_crown_target(s_stack));
}

#endif /* EOS_RECENT_APPS_ENABLE */
