/**
 * @file eos_basic_widgets.c
 * @brief Basic widgets
 */

#include "eos_basic_widgets.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "eos_core.h"
#include "eos_lang.h"
#define EOS_LOG_TAG "BasicWidgets"
#include "eos_log.h"
#include "eos_image.h"
#include "eos_event.h"
#include "script_engine_core.h"
#include "eos_theme.h"
#include "eos_port.h"
#include "eos_icon.h"
#include "eos_config.h"
#include "eos_anim.h"
#include "eos_font.h"
#include "eos_crown.h"
#include "lvgl_private.h"
#include "eos_app_header.h"
#include "eos_activity.h"
#include "eos_app_list.h"
#include "eos_mem.h"
#include "eos_service_cache.h"
#include "eos_widget_data.h"

/* Macros and Definitions -------------------------------------*/

#define _BACK_BTN_WIDTH 64
#define _BACK_BTN_HEIGHT _BACK_BTN_WIDTH

#define _LIST_SWITCH_WIDTH 38
#define _LIST_SWITCH_HEIGHT 65

#define _LIST_LABEL_MARGIN_VER 8

#define _LIST_HEAD_PLACEHOLDER_HEIGHT 110
#define _LIST_TAIL_PLACEHOLDER_HEIGHT 100

#define _LIST_CONTAINER_MARGIN_BOTTOM 8

#define _LIST_TRANSITION_HALF_SCALE 128
#define _LIST_TRANSITION_NORMAL_SCALE 256
#define _LIST_TRANSITION_DELAY_PCT 0
#define _LIST_TRANSITION_MAX_VISIBLE_ITEMS 64
#define _LIST_TRANSITION_STATE_HISTORY_CAP 16

/* Variables --------------------------------------------------*/

typedef struct
{
    lv_obj_t *list;
    lv_obj_t *button;
    eos_activity_t *activity;
    int32_t button_hidden_x;
    uint32_t sequence;
} eos_list_transition_state_t;

static eos_list_transition_state_t *_list_transition_state = NULL;
static uint32_t _list_transition_sequence = 0U;

static eos_list_transition_state_t *_list_transition_get_state(lv_obj_t *list);
static void _list_transition_clear_state(void);
static void _list_transition_state_dtor(void *data);
static void _list_transition_record_state(lv_obj_t *list, lv_obj_t *button, eos_activity_t *activity);
static bool _list_transition_select_state_for_activity(eos_activity_t *expected_activity);
static void _list_transition_select_state_from_tree(lv_obj_t *root,
                                                    eos_activity_t *expected_activity,
                                                    eos_list_transition_state_t **best_state,
                                                    uint32_t *best_sequence);
static lv_obj_t *_list_transition_resolve_button_target(lv_obj_t *list, lv_obj_t *target);
static void _list_transition_list_clicked_cb(lv_event_t *e);
static void _list_transition_list_delete_cb(lv_event_t *e);
static void _list_transition_cancel_anims_for_list(lv_obj_t *list);
static void _list_transition_set_scale_cb(void *var, int32_t value);
static void _list_transition_set_translate_x_cb(void *var, int32_t value);
static void _list_transition_init_scale_anim(lv_anim_t *anim,
                                             lv_obj_t *obj,
                                             int32_t start,
                                             int32_t end,
                                             uint32_t duration);
static void _list_transition_init_translate_x_anim(lv_anim_t *anim,
                                                   lv_obj_t *obj,
                                                   int32_t start,
                                                   int32_t end,
                                                   uint32_t duration);
static bool _list_transition_is_obj_visible_in_list(lv_obj_t *list, lv_obj_t *obj);
static bool _list_transition_is_descendant_of(lv_obj_t *obj, lv_obj_t *ancestor);
static uint32_t _list_transition_collect_visible_children(lv_obj_t *list, lv_obj_t **out_objs, uint32_t cap);
static uint32_t _list_transition_collect_all_children(lv_obj_t *list, lv_obj_t **out_objs, uint32_t cap);

/* Function Implementations -----------------------------------*/

void eos_obj_get_coord_center(lv_obj_t *obj, lv_coord_t *x, lv_coord_t *y)
{
    if (obj)
    {
        lv_coord_t obj_x, obj_y;
        lv_area_t area;
        lv_obj_get_coords(obj, &area);
        obj_x = area.x1;
        obj_y = area.y1;
        *x = obj_x + lv_obj_get_width(obj) / 2;
        *y = obj_y + lv_obj_get_height(obj) / 2;
    }
    else
    {
        *x = 0;
        *y = 0;
    }
}

lv_obj_t *eos_button_create_ex(lv_obj_t *parent,
                               lv_color_t btn_color,
                               const char *txt,
                               lv_color_t txt_color,
                               lv_event_cb_t clicked_cb,
                               void *event_user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, lv_pct(100), EOS_THEME_BUTTON_HEIGHT);
    lv_obj_set_style_bg_color(btn, btn_color, 0);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(btn, LV_DIR_NONE);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, txt);
    lv_obj_set_style_text_color(label, txt_color, 0);
    lv_obj_center(label);

    if (clicked_cb)
        lv_obj_add_event_cb(btn, clicked_cb, LV_EVENT_CLICKED, event_user_data);

    return btn;
}

lv_obj_t *eos_button_create(lv_obj_t *parent, const char *txt, lv_event_cb_t clicked_cb, void *event_user_data)
{
    return eos_button_create_ex(parent, EOS_THEME_SECONDARY_COLOR, txt, EOS_COLOR_WHITE, clicked_cb, event_user_data);
}

lv_draw_buf_t *eos_draw_buf_create(uint32_t w, uint32_t h, lv_color_format_t cf, uint32_t stride)
{
    if (w == 0 || h == 0)
    {
        return NULL;
    }

    uint32_t data_size;
    if (stride == 0)
    {
        data_size = w * h * lv_color_format_get_size(cf);
    }
    else
    {
        data_size = h * stride;
    }
    if (data_size == 0)
        return NULL;
    void *data_buf = eos_cache_buf_alloc(data_size);
    if (!data_buf)
    {
        EOS_LOG_E("NULL pointer at %s:%d, at function: %s", __FILE__, __LINE__, __func__);
        return NULL;
    }
    memset(data_buf, 0, data_size);
    lv_draw_buf_t *draw_buf = eos_malloc_zeroed(sizeof(lv_draw_buf_t));
    if (!draw_buf)
    {
        EOS_LOG_E("NULL pointer at %s:%d, at function: %s", __FILE__, __LINE__, __func__);
        eos_cache_buf_free(data_buf);
        return NULL;
    }

    if (lv_draw_buf_init(draw_buf, w, h, cf, stride, data_buf, data_size) != LV_RESULT_OK)
    {
        EOS_LOG_E("Init draw buf failed");
        eos_cache_buf_free(data_buf);
        eos_free(draw_buf);
        return NULL;
    }
    return draw_buf;
}

void eos_draw_buf_destroy(lv_draw_buf_t *draw_buf)
{
    EOS_CHECK_PTR_RETURN(draw_buf);
    if (draw_buf->data)
        eos_cache_buf_free(draw_buf->data);
    eos_free(draw_buf);
}

lv_obj_t *eos_back_btn_create(lv_obj_t *parent, bool show_text)
{
    static lv_style_t style_pressed;
    static bool style_pressed_inited = false;

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_add_event_cb(btn, eos_activity_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *btn_label = lv_label_create(btn);
    if (show_text)
    {
        lv_label_set_text_fmt(btn_label, "%s", eos_lang_get_text(STR_ID_BACK));
    }
    else
    {
        lv_label_set_text(btn_label, RI_ARROW_LEFT_S_LINE_LARGE);
    }
    lv_obj_set_style_text_color(btn_label, EOS_COLOR_WHITE, 0);
    lv_obj_align(btn_label, LV_ALIGN_CENTER, -2, 2);
    lv_obj_set_size(btn, _BACK_BTN_WIDTH, _BACK_BTN_HEIGHT);

    lv_obj_set_style_transform_pivot_x(btn, _BACK_BTN_WIDTH / 2, 0);
    lv_obj_set_style_transform_pivot_y(btn, _BACK_BTN_HEIGHT / 2, 0);

    if (!style_pressed_inited)
    {
        /* Shared styles must be initialized only once, otherwise LVGL will leak the
         * previously allocated property list on every back button recreation. */
        lv_style_init(&style_pressed);
        lv_style_set_transform_scale(&style_pressed, 350);
        lv_style_set_bg_color(&style_pressed, lv_color_lighten(EOS_THEME_BUTTON_COLOR, 64));
        style_pressed_inited = true;
    }

    lv_obj_add_style(btn, &style_pressed, LV_STATE_PRESSED);

    return btn;
}

void eos_switch_set_state(lv_obj_t *sw, bool checked)
{
    if (checked)
    {
        lv_obj_set_state(sw, LV_STATE_CHECKED, checked);
        lv_obj_send_event(sw, LV_EVENT_VALUE_CHANGED, NULL);
    }
}

/************************** List Related **************************/

lv_obj_t *eos_list_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);
    lv_obj_t *list = lv_list_create(parent);
    eos_list_transition_state_t *state = eos_malloc_zeroed(sizeof(eos_list_transition_state_t));
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_ver(list, 0, 0);
    lv_obj_set_style_pad_hor(list, 10, 0);
    lv_obj_center(list);
    lv_obj_set_style_pad_top(list, _LIST_HEAD_PLACEHOLDER_HEIGHT, 0);
    lv_obj_set_style_pad_bottom(list, _LIST_TAIL_PLACEHOLDER_HEIGHT, 0);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (state)
    {
        state->list = list;
        eos_wdata_set(list, EOS_WDATA_LIST_TRANSITION, state, _list_transition_state_dtor);
    }
    else
    {
        EOS_LOG_E("Failed to allocate list transition state for list %p", list);
    }
    lv_obj_add_event_cb(list, _list_transition_list_clicked_cb, LV_EVENT_PRESSED, list);
    lv_obj_add_event_cb(list, _list_transition_list_delete_cb, LV_EVENT_DELETE, list);
    return list;
}

static eos_list_transition_state_t *_list_transition_get_state(lv_obj_t *list)
{
    if (!(list && lv_obj_is_valid(list) && lv_obj_check_type(list, &lv_list_class)))
    {
        return NULL;
    }

    eos_list_transition_state_t *state = (eos_list_transition_state_t *)eos_wdata_get(list, EOS_WDATA_LIST_TRANSITION);
    if (state)
    {
        state->list = list;
    }

    return state;
}

static void _list_transition_clear_state(void)
{
    _list_transition_state = NULL;
}

static void _list_transition_state_dtor(void *data)
{
    eos_list_transition_state_t *state = (eos_list_transition_state_t *)data;
    if (_list_transition_state == state)
        _list_transition_state = NULL;
    eos_free(state);
}

static void _list_transition_select_state_from_tree(lv_obj_t *root,
                                                    eos_activity_t *expected_activity,
                                                    eos_list_transition_state_t **best_state,
                                                    uint32_t *best_sequence)
{
    if (!(root && expected_activity && best_state && best_sequence))
    {
        return;
    }

    if (lv_obj_check_type(root, &lv_list_class))
    {
        eos_list_transition_state_t *state = _list_transition_get_state(root);
        if (state && state->button && lv_obj_is_valid(state->button)
            && _list_transition_is_descendant_of(state->button, state->list) && state->sequence >= *best_sequence)
        {
            *best_sequence = state->sequence;
            *best_state = state;
        }
    }

    uint32_t child_cnt = lv_obj_get_child_count(root);
    for (uint32_t i = 0U; i < child_cnt; i++)
    {
        lv_obj_t *child = lv_obj_get_child(root, i);
        if (child)
        {
            _list_transition_select_state_from_tree(child, expected_activity, best_state, best_sequence);
        }
    }
}

static void _list_transition_record_state(lv_obj_t *list, lv_obj_t *button, eos_activity_t *activity)
{
    if (!(list && button && activity))
    {
        return;
    }

    eos_list_transition_state_t *state = _list_transition_get_state(list);
    if (!state)
    {
        return;
    }

    state->list = list;
    state->button = button;
    state->activity = activity;
    state->sequence = ++_list_transition_sequence;
    _list_transition_state = state;
}

static bool _list_transition_select_state_for_activity(eos_activity_t *expected_activity)
{
    if (!expected_activity)
    {
        return false;
    }

    lv_obj_t *expected_view = eos_activity_get_view(expected_activity);
    if (!(expected_view && lv_obj_is_valid(expected_view)))
    {
        return false;
    }

    eos_list_transition_state_t *best_state = NULL;
    uint32_t best_sequence = 0U;
    _list_transition_select_state_from_tree(expected_view, expected_activity, &best_state, &best_sequence);
    _list_transition_state = best_state;

    return best_state != NULL;
}

static void _list_transition_list_delete_cb(lv_event_t *e)
{
    lv_obj_t *list = lv_event_get_user_data(e);
    if (!list)
    {
        return;
    }

    _list_transition_cancel_anims_for_list(list);

    eos_list_transition_state_t *state = _list_transition_get_state(list);
    if (state && _list_transition_state == state)
    {
        _list_transition_clear_state();
    }
}

static void _list_transition_cancel_anims_for_list(lv_obj_t *list)
{
    if (!(list && lv_obj_is_valid(list)))
    {
        return;
    }

    lv_obj_t *children[_LIST_TRANSITION_MAX_VISIBLE_ITEMS] = {0};
    uint32_t child_cnt = _list_transition_collect_all_children(list, children, _LIST_TRANSITION_MAX_VISIBLE_ITEMS);
    for (uint32_t i = 0U; i < child_cnt; i++)
    {
        if (children[i] && lv_obj_is_valid(children[i]))
        {
            lv_anim_delete(children[i], NULL);
        }
    }

    lv_anim_delete(list, NULL);

    if (_list_transition_state && _list_transition_state->button && lv_obj_is_valid(_list_transition_state->button))
    {
        lv_anim_delete(_list_transition_state->button, NULL);
    }
}

static lv_obj_t *_list_transition_resolve_button_target(lv_obj_t *list, lv_obj_t *target)
{
    if (!(list && target))
    {
        EOS_LOG_D("resolve_button: list=%p, target=%p -> NULL", list, target);
        return NULL;
    }

    lv_obj_t *obj = target;
    int depth = 0;
    while (obj && obj != list)
    {
        if (lv_obj_has_flag(obj, LV_OBJ_FLAG_USER_1))
        {
            EOS_LOG_D("resolve_button: found button at depth %d, obj=%p", depth, obj);
            return obj;
        }
        obj = lv_obj_get_parent(obj);
        depth++;
    }

    EOS_LOG_D("resolve_button: no USER_1 flag found, depth=%d", depth);
    return NULL;
}

static void _list_transition_list_clicked_cb(lv_event_t *e)
{
    lv_obj_t *list = lv_event_get_user_data(e);
    lv_obj_t *target = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED)
    {
        return;
    }

    EOS_LOG_D("list_clicked: code=%d, list=%p, target=%p", code, list, target);
    if (!(list && target))
    {
        EOS_LOG_D("list_clicked: list or target is NULL, skip");
        return;
    }

    lv_obj_t *button = _list_transition_resolve_button_target(list, target);
    if (!button)
    {
        EOS_LOG_D("list_clicked: button is NULL after resolve, skip");
        return;
    }

    eos_activity_t *click_activity = eos_activity_get_previous();
    if (!click_activity)
    {
        click_activity = eos_activity_get_current();
    }
    if (!click_activity)
    {
        EOS_LOG_D("list_clicked: previous/current activity is NULL, fallback to visible_activity");
        click_activity = eos_activity_get_visible();
    }
    EOS_LOG_D("list_clicked: saving state list=%p, button=%p, click_activity=%p", list, button, click_activity);
    _list_transition_record_state(list, button, click_activity);
}

bool eos_list_transition_should_animate(eos_activity_t *from, eos_activity_t *to, bool back)
{
    eos_activity_t *expected_activity = back ? to : from;
    EOS_LOG_D("should_animate: from=%p, to=%p, back=%d, expected=%p", from, to, back, expected_activity);

    if (!expected_activity)
    {
        EOS_LOG_D("should_animate: FAIL gate 1: expected activity is NULL");
        return false;
    }

    if (!_list_transition_select_state_for_activity(expected_activity))
    {
        EOS_LOG_D("should_animate: FAIL gate 2: no matching list transition state for expected activity");
        return false;
    }

    if (_list_transition_state->activity != expected_activity)
    {
        EOS_LOG_D("should_animate: activity mismatch tolerated, saved=%p, expected=%p",
                  _list_transition_state->activity,
                  expected_activity);
    }

    EOS_LOG_D("should_animate: PASS - will animate");
    return true;
}

static bool _list_transition_is_obj_visible_in_list(lv_obj_t *list, lv_obj_t *obj)
{
    if (!(list && obj && lv_obj_is_valid(list) && lv_obj_is_valid(obj)))
    {
        return false;
    }

    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN))
    {
        return false;
    }

    lv_area_t list_area;
    lv_area_t obj_area;
    lv_obj_get_coords(list, &list_area);
    lv_obj_get_coords(obj, &obj_area);

    if (obj_area.x2 < list_area.x1 || obj_area.x1 > list_area.x2 || obj_area.y2 < list_area.y1
        || obj_area.y1 > list_area.y2)
    {
        return false;
    }

    return true;
}

static bool _list_transition_is_descendant_of(lv_obj_t *obj, lv_obj_t *ancestor)
{
    if (!(obj && ancestor && lv_obj_is_valid(obj) && lv_obj_is_valid(ancestor)))
    {
        return false;
    }

    lv_obj_t *cur = obj;
    while (cur)
    {
        if (cur == ancestor)
        {
            return true;
        }
        cur = lv_obj_get_parent(cur);
    }

    return false;
}

static uint32_t _list_transition_collect_visible_children(lv_obj_t *list, lv_obj_t **out_objs, uint32_t cap)
{
    if (!(list && out_objs && cap > 0U))
    {
        return 0U;
    }

    uint32_t cnt = 0U;
    uint32_t child_cnt = lv_obj_get_child_count(list);
    for (uint32_t i = 0U; i < child_cnt && cnt < cap; i++)
    {
        lv_obj_t *child = lv_obj_get_child(list, i);
        if (_list_transition_is_obj_visible_in_list(list, child))
        {
            out_objs[cnt++] = child;
        }
    }

    return cnt;
}

static uint32_t _list_transition_collect_all_children(lv_obj_t *list, lv_obj_t **out_objs, uint32_t cap)
{
    if (!(list && out_objs && cap > 0U))
    {
        return 0U;
    }

    uint32_t cnt = 0U;
    uint32_t child_cnt = lv_obj_get_child_count(list);
    for (uint32_t i = 0U; i < child_cnt && cnt < cap; i++)
    {
        lv_obj_t *child = lv_obj_get_child(list, i);
        if (child && !lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN))
        {
            out_objs[cnt++] = child;
        }
    }

    return cnt;
}

void eos_list_transition_setup(lv_obj_t *list, lv_obj_t *button, eos_activity_t *activity)
{
    if (!list || !lv_obj_is_valid(list))
        return;
    eos_list_transition_state_t *state = _list_transition_get_state(list);
    if (!state || state->list != list)
        return;
    state->button = button;
    state->activity = activity;
    state->sequence++;
}

void eos_list_transition_play(eos_anim_group_t *group, eos_activity_t *from, eos_activity_t *to, bool back)
{
    if (!(group && from && to))
    {
        return;
    }

    if (!eos_list_transition_should_animate(from, to, back))
    {
        return;
    }

    eos_activity_t *list_activity = back ? to : from;
    eos_activity_t *page_activity = back ? from : to;
    lv_obj_t *list_view = eos_activity_get_view(list_activity);
    eos_list_transition_state_t *state = _list_transition_state;
    lv_obj_t *list = state ? state->list : NULL;
    lv_obj_t *button = state ? state->button : NULL;
    if (!(list_view && list && button))
    {
        EOS_LOG_W("list_transition_play: invalid objects detected, skipping animation");
        return;
    }

    if (lv_obj_has_state(button, LV_STATE_PRESSED))
    {
        lv_obj_remove_state(button, LV_STATE_PRESSED);
    }

    lv_obj_t *page_obj = eos_activity_take_snapshot(page_activity, false);
    if (!page_obj)
    {
        return;
    }

    if (back)
    {
        lv_obj_remove_flag(list_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(list, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_update_layout(list_view);
    lv_obj_update_layout(list);
    lv_coord_t btn_w = lv_obj_get_width(button);
    lv_area_t btn_area;
    lv_obj_get_coords(button, &btn_area);

    lv_obj_t *visible_items[_LIST_TRANSITION_MAX_VISIBLE_ITEMS] = {0};
    uint32_t visible_item_cnt =
        _list_transition_collect_visible_children(list, visible_items, _LIST_TRANSITION_MAX_VISIBLE_ITEMS);
    lv_obj_t *all_items[_LIST_TRANSITION_MAX_VISIBLE_ITEMS] = {0};
    uint32_t all_item_cnt = 0U;
    if (back && visible_item_cnt == 0U)
    {
        visible_item_cnt =
            _list_transition_collect_all_children(list, visible_items, _LIST_TRANSITION_MAX_VISIBLE_ITEMS);
    }
    if (back)
    {
        all_item_cnt = _list_transition_collect_all_children(list, all_items, _LIST_TRANSITION_MAX_VISIBLE_ITEMS);
    }

    uint32_t total_duration = EOS_VIEW_SWITCH_DURATION;
    if (total_duration == 0U)
    {
        total_duration = 1U;
    }

    uint32_t page_delay = (total_duration * _LIST_TRANSITION_DELAY_PCT) / 100U;
    if (page_delay >= total_duration)
    {
        page_delay = total_duration > 1U ? total_duration - 1U : 0U;
    }

    uint32_t page_duration = total_duration - page_delay;
    if (page_duration == 0U)
    {
        page_duration = 1U;
    }

    int32_t computed_hidden_x = -(btn_area.x1 + btn_w + 16);
    int32_t style_translate_x = lv_obj_get_style_translate_x(button, 0);
    int32_t button_hidden_x = computed_hidden_x;
    if (state->button_hidden_x < 0)
    {
        button_hidden_x = state->button_hidden_x;
    }

    int32_t button_start_x;
    int32_t button_end_x;
    if (back)
    {
        button_start_x = (style_translate_x < 0) ? style_translate_x : button_hidden_x;
        button_end_x = 0;
        /* Ensure the real button is at the hidden position before snapshot capture,
         * in case its translate_x was reset to 0 between forward and reverse transitions.
         * Without this, the button would be visible at its normal position when the list
         * is unhidden, appearing to "pop in" instead of smoothly sliding back. */
        lv_obj_set_style_translate_x(button, button_start_x, 0);
    }
    else
    {
        button_start_x = 0;
        button_end_x = button_hidden_x;
        state->button_hidden_x = button_hidden_x;
    }
    int32_t page_start_x = back ? 0 : EOS_DISPLAY_WIDTH;
    int32_t page_end_x = back ? EOS_DISPLAY_WIDTH : 0;

    if (back)
    {
        for (uint32_t i = 0U; i < all_item_cnt; i++)
        {
            lv_obj_t *obj = all_items[i];
            bool in_visible = false;
            for (uint32_t j = 0U; j < visible_item_cnt; j++)
            {
                if (visible_items[j] == obj)
                {
                    in_visible = true;
                    break;
                }
            }

            if (!in_visible)
            {
                lv_obj_set_style_transform_scale(obj, _LIST_TRANSITION_NORMAL_SCALE, 0);
            }
        }

        /* Ensure the real button starts at the hidden off-screen position before
         * the list becomes visible, so it does not "pop in" at its normal spot.
         * The button animation runs directly (no snapshot backend) so the real
         * widget slides back into view instead of a hidden snapshot image. */
        lv_obj_set_style_translate_x(button, button_start_x, 0);

        /* Snapshot-batch other list items (scale grow animation) */
        eos_anim_snapshot_batch_begin();

        for (uint32_t i = 0U; i < visible_item_cnt; i++)
        {
            lv_obj_t *obj = visible_items[i];
            if (obj == button)
            {
                continue;
            }
            lv_obj_set_style_transform_pivot_x(obj, lv_obj_get_width(obj) / 2, 0);
            lv_obj_set_style_transform_pivot_y(obj, lv_obj_get_height(obj) / 2, 0);
            eos_anim_t *anim = eos_anim_transform_scale_create(obj,
                                                               _LIST_TRANSITION_HALF_SCALE,
                                                               _LIST_TRANSITION_NORMAL_SCALE,
                                                               total_duration,
                                                               false);
            if (anim)
            {
                eos_anim_set_backend(anim, EOS_ANIM_BACKEND_SNAPSHOT);
                eos_anim_set_preserve_layout(anim, true);
                eos_anim_group_attach(anim, group);
                eos_anim_start(anim);
            }
        }

        lv_obj_set_style_translate_x(page_obj, page_start_x, 0);

        eos_anim_snapshot_batch_flush();

        /* Button slides back in directly (no snapshot backend) so it is
         * visible above the snapshot layer while the page slides out. */
        eos_anim_t *btn_anim = eos_anim_move_create(button, button_start_x, 0, button_end_x, 0, page_duration, false);
        if (btn_anim)
        {
            eos_anim_set_path(btn_anim, lv_anim_path_ease_in_out);
            eos_anim_set_delay(btn_anim, page_delay);
            eos_anim_group_attach(btn_anim, group);
            eos_anim_start(btn_anim);
        }
        else
        {
            /* Fallback: if animation creation fails, at least place the button
             * at its final position so it doesn't stay off-screen. */
            lv_obj_set_style_translate_x(button, button_end_x, 0);
        }

        eos_anim_t *page_anim = eos_anim_move_create(page_obj, page_start_x, 0, page_end_x, 0, total_duration, false);
        if (page_anim)
        {
            eos_anim_set_path(page_anim, lv_anim_path_ease_in_out);
            eos_anim_group_attach(page_anim, group);
            eos_anim_start(page_anim);
        }
    }
    else
    {
        eos_anim_snapshot_batch_begin();

        for (uint32_t i = 0U; i < visible_item_cnt; i++)
        {
            lv_obj_t *obj = visible_items[i];
            if (obj == button)
            {
                continue;
            }
            lv_obj_set_style_transform_pivot_x(obj, lv_obj_get_width(obj) / 2, 0);
            lv_obj_set_style_transform_pivot_y(obj, lv_obj_get_height(obj) / 2, 0);
            eos_anim_t *anim = eos_anim_transform_scale_create(obj,
                                                               _LIST_TRANSITION_NORMAL_SCALE,
                                                               _LIST_TRANSITION_HALF_SCALE,
                                                               total_duration,
                                                               false);
            if (anim)
            {
                eos_anim_set_backend(anim, EOS_ANIM_BACKEND_SNAPSHOT);
                eos_anim_set_preserve_layout(anim, true);
                eos_anim_group_attach(anim, group);
                eos_anim_start(anim);
            }
        }

        lv_obj_set_style_translate_x(page_obj, page_start_x, 0);

        eos_anim_t *btn_anim = eos_anim_move_create(button, button_start_x, 0, button_end_x, 0, total_duration, false);
        if (btn_anim)
        {
            eos_anim_set_backend(btn_anim, EOS_ANIM_BACKEND_SNAPSHOT);
            eos_anim_set_preserve_layout(btn_anim, true);
            eos_anim_set_path(btn_anim, lv_anim_path_ease_in_out);
            eos_anim_group_attach(btn_anim, group);
            eos_anim_start(btn_anim);
        }

        eos_anim_snapshot_batch_flush();

        eos_anim_t *page_anim = eos_anim_move_create(page_obj, page_start_x, 0, page_end_x, 0, page_duration, false);
        if (page_anim)
        {
            eos_anim_set_path(page_anim, lv_anim_path_ease_in_out);
            eos_anim_set_delay(page_anim, page_delay);
            eos_anim_group_attach(page_anim, group);
            eos_anim_start(page_anim);
        }
    }

    /* page_obj is in its own activity's snap_container which was moved
       to foreground by _activity_show — no manual z-order needed. */
}

static void _list_container_common_style(lv_obj_t *container)
{
    lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(container, LV_DIR_NONE); // Disable scrolling
    lv_obj_set_style_bg_color(container, EOS_THEME_SECONDARY_COLOR, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, EOS_LIST_CONTAINER_PAD_ALL, 0);
    lv_obj_set_style_margin_bottom(container, _LIST_CONTAINER_MARGIN_BOTTOM, 0);
    lv_obj_set_style_align(container, LV_ALIGN_CENTER, 0);
    lv_obj_set_style_radius(container, EOS_LIST_OBJ_RADIUS, 0);
    lv_obj_set_style_shadow_width(container, 0, 0);
    lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
}

lv_obj_t *_list_btn_container_create(lv_obj_t *list)
{
    lv_obj_t *btn = lv_button_create(list);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(btn, lv_pct(100), EOS_LIST_CONTAINER_HEIGHT);
    _list_container_common_style(btn);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW); // Horizontal layout
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_update_layout(btn);
    lv_obj_set_style_transform_pivot_x(btn, lv_obj_get_width(btn) / 2, 0);
    lv_obj_set_style_transform_pivot_y(btn, lv_obj_get_height(btn) / 2, 0);
    lv_obj_set_style_transform_scale(btn, 230, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, EOS_COLOR_DARK_GREY_1, LV_STATE_PRESSED);
    return btn;
}

lv_obj_t *eos_list_add_button(lv_obj_t *list, const void *icon, const char *txt)
{
    lv_obj_t *obj = _list_btn_container_create(list);
    lv_obj_t *img, *label;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_USER_1);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    if (icon)
    {
        img = lv_image_create(obj);
        lv_image_set_src(img, icon);
        eos_img_set_size(img, 64, 64);
    }

    if (txt)
    {
        label = lv_label_create(obj);
        lv_label_set_text(label, txt);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_flex_grow(label, 1);
        lv_obj_set_style_margin_left(label, 10, 0);
    }

    return obj;
}

lv_obj_t *eos_list_add_placeholder(lv_obj_t *list, uint32_t height)
{
    lv_obj_t *ph = lv_obj_create(list);
    lv_obj_remove_style_all(ph);
    lv_obj_set_size(ph, lv_pct(100), height);
    return ph;
}

lv_obj_t *eos_round_icon_create(lv_obj_t *parent, lv_color_t bg_color, const void *icon_src)
{
    // Draw circular background
    lv_obj_t *round = lv_obj_create(parent);
    lv_obj_set_style_border_width(round, 0, 0);
    lv_obj_remove_flag(round, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(round, 50, 50);
    lv_obj_set_style_pad_all(round, 0, 0);
    lv_obj_set_style_radius(round, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(round, bg_color, 0);
    // Draw image
    lv_obj_t *icon = lv_label_create(round);
    lv_label_set_text(icon, icon_src);
    lv_obj_set_style_translate_y(icon, 2, 0);
    lv_obj_center(icon);

    lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(round, LV_OBJ_FLAG_EVENT_BUBBLE);
    return round;
}

lv_obj_t *eos_list_add_round_icon_button(lv_obj_t *list, lv_color_t bg_color, const void *icon_src, const char *txt)
{
    // Create button
    lv_obj_t *btn = _list_btn_container_create(list);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_USER_1);
    eos_round_icon_create(btn, bg_color, icon_src);
    // Text
    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_margin_left(label, 14, 0);
    lv_label_set_text(label, txt);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_style_margin_right(label, 36, 0);
    return btn;
}

lv_obj_t *eos_list_add_round_icon_button_str_id(lv_obj_t *list,
                                                lv_color_t bg_color,
                                                const void *icon_src,
                                                lang_string_id_t id)
{
    // Create button
    lv_obj_t *btn = _list_btn_container_create(list);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_USER_1);
    eos_round_icon_create(btn, bg_color, icon_src);
    // Text
    lv_obj_t *label = lv_label_create(btn);
    eos_label_set_text_id(label, id);
    lv_obj_set_style_margin_left(label, 14, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_style_margin_right(label, 36, 0);
    return btn;
}

lv_obj_t *eos_list_add_entry_button(lv_obj_t *list, const char *txt)
{
    // Create button
    lv_obj_t *btn = _list_btn_container_create(list);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_USER_1);
    // Text
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, txt);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_flex_grow(label, 1);
    // Text
    label = lv_label_create(btn);
    lv_label_set_text(label, RI_ARROW_RIGHT_S_LINE);
    return btn;
}

lv_obj_t *eos_list_add_entry_button_str_id(lv_obj_t *list, lang_string_id_t id)
{
    // Create button
    lv_obj_t *btn = _list_btn_container_create(list);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_USER_1);
    // Text
    lv_obj_t *label = lv_label_create(btn);
    eos_label_set_text_id(label, id);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_flex_grow(label, 1);
    // Text
    label = lv_label_create(btn);
    lv_label_set_text(label, RI_ARROW_RIGHT_S_LINE);
    return btn;
}

lv_obj_t *eos_list_add_container(lv_obj_t *list)
{
    lv_obj_t *container = lv_obj_create(list);
    _list_container_common_style(container);
    return container;
}

static void _list_switch_container_clicked_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(sw);

    if (lv_obj_has_state(sw, LV_STATE_CHECKED))
    {
        lv_obj_remove_state(sw, LV_STATE_CHECKED);
    }
    else
    {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_send_event(sw, LV_EVENT_VALUE_CHANGED, NULL);
}

lv_obj_t *eos_list_add_switch(lv_obj_t *list, const char *txt)
{
    // Create container
    lv_obj_t *container = _list_btn_container_create(list);
    lv_obj_set_size(container, lv_pct(100), EOS_LIST_CONTAINER_HEIGHT);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW); // Horizontal layout
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Text
    lv_obj_t *label = lv_label_create(container);
    lv_label_set_text(label, txt);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_flex_grow(label, 1);

    // Switch
    lv_obj_t *sw = lv_switch_create(container);
    lv_obj_set_height(sw, _LIST_SWITCH_WIDTH);
    lv_obj_update_layout(sw);
    lv_obj_set_width(sw, _LIST_SWITCH_HEIGHT);
    lv_obj_remove_flag(sw, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(container, _list_switch_container_clicked_cb, LV_EVENT_CLICKED, sw);

    return sw;
}

static void _list_label_common_style(lv_obj_t *label)
{
    eos_label_set_font_size(label, EOS_FONT_SIZE_SMALL);
    lv_obj_set_align(label, LV_ALIGN_LEFT_MID);
    lv_obj_set_size(label, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_margin_hor(label, EOS_LIST_CONTAINER_PAD_ALL, 0);
    lv_obj_set_style_margin_ver(label, _LIST_LABEL_MARGIN_VER, 0);
}

lv_obj_t *eos_list_add_title(lv_obj_t *list, const char *txt)
{
    lv_obj_t *label = lv_label_create(list);
    _list_label_common_style(label);
    lv_label_set_text(label, txt);
    lv_obj_set_style_text_color(label, EOS_COLOR_WHITE, 0);
    return label;
}

lv_obj_t *eos_list_add_comment(lv_obj_t *list, const char *txt)
{
    lv_obj_t *label = lv_label_create(list);
    _list_label_common_style(label);
    lv_label_set_text(label, txt);
    lv_obj_set_style_text_color(label, EOS_COLOR_GREY_1, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}

lv_obj_t *_split_line_create(lv_obj_t *parent)
{
    lv_obj_t *sl = lv_obj_create(parent);
    lv_obj_remove_style_all(sl);
    lv_obj_set_size(sl, lv_pct(90), 2);
    lv_obj_set_style_bg_color(sl, EOS_COLOR_DARK_GREY_2, 0);
    return sl;
}

static void _list_slider_delete_cb(lv_event_t *e)
{
    eos_list_slider_t *list_slider = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(list_slider);
    eos_free(list_slider);
}

lv_obj_t *eos_list_add_title_container(lv_obj_t *list, const char *title)
{
    // Create outer transparent container
    lv_obj_t *outer_container = lv_obj_create(list);
    lv_obj_remove_style_all(outer_container); // Remove default style
    lv_obj_set_size(outer_container, lv_pct(100), LV_SIZE_CONTENT); // Height adaptive
    lv_obj_set_style_bg_opa(outer_container, LV_OPA_TRANSP, 0); // Set transparent background
    lv_obj_set_flex_flow(outer_container, LV_FLEX_FLOW_COLUMN); // Vertical layout

    // Add top-left label
    lv_obj_t *txt_label = eos_list_add_title(outer_container, title);

    // Create inner container (horizontally centered)
    lv_obj_t *inner_container = eos_list_add_container(outer_container);
    lv_obj_set_style_align(inner_container, LV_ALIGN_CENTER, 0); // Horizontally centered
    lv_obj_set_style_margin_hor(inner_container, 0, 0);

    return inner_container;
}

static void _slider_button_clicked_cb(lv_event_t *e)
{
    const uint8_t scale_diff = 50;
    const uint8_t duration = 120;
    lv_obj_t *target = lv_event_get_target(e);
    eos_list_slider_t *ls = (eos_list_slider_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(ls);
    if (ls->minus_btn == target)
    {
        eos_anim_transform_scale_start_ex(ls->minus_label,
                                          ls->minus_label_scale,
                                          ls->minus_label_scale - scale_diff,
                                          duration,
                                          duration,
                                          1,
                                          false);
    }
    else if (ls->plus_btn == target)
    {
        eos_anim_transform_scale_start_ex(ls->plus_label,
                                          ls->plus_label_scale,
                                          ls->plus_label_scale - scale_diff,
                                          duration,
                                          duration,
                                          1,
                                          false);
    }
}

void eos_list_slider_set_minus_label_scale(eos_list_slider_t *ls, uint16_t scale)
{
    EOS_CHECK_PTR_RETURN(ls);
    ls->minus_label_scale = scale;
    lv_obj_set_style_transform_scale(ls->minus_label, scale, 0);
}

void eos_list_slider_set_plus_label_scale(eos_list_slider_t *ls, uint16_t scale)
{
    EOS_CHECK_PTR_RETURN(ls);
    ls->plus_label_scale = scale;
    lv_obj_set_style_transform_scale(ls->plus_label, scale, 0);
}

eos_list_slider_t *eos_list_add_slider(lv_obj_t *list, const char *txt)
{
    eos_list_slider_t *list_slider = eos_malloc_zeroed(sizeof(eos_list_slider_t));
    EOS_CHECK_PTR_RETURN_VAL_FREE(list_slider, NULL, list_slider);

    list_slider->minus_label_scale = 255;
    list_slider->plus_label_scale = 255;

    // Create outer transparent container
    lv_obj_t *inner_container = eos_list_add_title_container(list, txt);
    lv_obj_set_style_pad_all(inner_container, 0, 0);
    lv_obj_set_size(inner_container, lv_pct(100), EOS_LIST_CONTAINER_HEIGHT);
    lv_obj_add_event_cb(inner_container, _list_slider_delete_cb, LV_EVENT_DELETE, (void *)list_slider);

    const uint8_t margin = 25;

    const uint8_t pct_slider = 50;
    const uint8_t pct_btn = (100 - pct_slider) / 2;

    /************************** Left Side **************************/
    list_slider->minus_btn = lv_obj_create(inner_container);
    lv_obj_set_size(list_slider->minus_btn, lv_pct(pct_btn), EOS_LIST_CONTAINER_HEIGHT);
    lv_obj_set_style_margin_all(list_slider->minus_btn, 0, 0);
    lv_obj_set_style_pad_all(list_slider->minus_btn, 0, 0);
    lv_obj_set_style_bg_opa(list_slider->minus_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(list_slider->minus_btn, EOS_THEME_BUTTON_COLOR, 0);
    lv_obj_set_style_radius(list_slider->minus_btn, EOS_ITEM_RADIUS, 0);
    lv_obj_set_style_border_width(list_slider->minus_btn, 0, 0);
    lv_obj_align_to(list_slider->minus_btn, inner_container, LV_ALIGN_LEFT_MID, 0, 0);

    list_slider->minus_label = lv_label_create(list_slider->minus_btn);
    lv_label_set_text(list_slider->minus_label, RI_SUBTRACT_FILL);
    lv_obj_center(list_slider->minus_label);
    lv_obj_add_event_cb(list_slider->minus_btn, _slider_button_clicked_cb, LV_EVENT_CLICKED, list_slider);
    lv_obj_update_layout(list_slider->minus_label);
    lv_obj_set_style_transform_pivot_x(list_slider->minus_label, lv_obj_get_width(list_slider->minus_label) / 2, 0);
    lv_obj_set_style_transform_pivot_y(list_slider->minus_label, lv_obj_get_height(list_slider->minus_label) / 2, 0);

    /************************** Right Side **************************/
    list_slider->plus_btn = lv_obj_create(inner_container);
    lv_obj_set_size(list_slider->plus_btn, lv_pct(pct_btn), EOS_LIST_CONTAINER_HEIGHT);
    lv_obj_set_style_margin_all(list_slider->plus_btn, 0, 0);
    lv_obj_set_style_pad_all(list_slider->plus_btn, 0, 0);
    lv_obj_set_style_bg_opa(list_slider->plus_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(list_slider->plus_btn, EOS_THEME_BUTTON_COLOR, 0);
    lv_obj_set_style_border_width(list_slider->plus_btn, 0, 0);
    lv_obj_set_style_radius(list_slider->plus_btn, EOS_ITEM_RADIUS, 0);
    lv_obj_align_to(list_slider->plus_btn, inner_container, LV_ALIGN_RIGHT_MID, 0, 0);

    list_slider->plus_label = lv_label_create(list_slider->plus_btn);
    lv_label_set_text(list_slider->plus_label, RI_ADD_FILL);
    lv_obj_center(list_slider->plus_label);
    lv_obj_add_event_cb(list_slider->plus_btn, _slider_button_clicked_cb, LV_EVENT_CLICKED, list_slider);
    lv_obj_update_layout(list_slider->plus_label);
    lv_obj_set_style_transform_pivot_x(list_slider->plus_label, lv_obj_get_width(list_slider->plus_label) / 2, 0);
    lv_obj_set_style_transform_pivot_y(list_slider->plus_label, lv_obj_get_height(list_slider->plus_label) / 2, 0);

    const uint8_t split_width = 2;
    lv_obj_t *obj = lv_obj_create(inner_container);
    lv_obj_set_style_bg_color(obj, EOS_THEME_SECONDARY_COLOR, 0);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_size(obj, lv_pct(pct_slider), EOS_LIST_CONTAINER_HEIGHT + split_width * 2);
    lv_obj_set_style_border_width(obj, split_width, 0);
    lv_obj_set_style_border_color(obj, EOS_COLOR_BLACK, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_center(obj);

    // Slider
    const uint8_t slider_height = 5;
    const uint8_t slider_main_bg_darken_lvl = 180;
    list_slider->slider = lv_slider_create(obj);
    lv_obj_set_style_margin_left(list_slider->slider, 18, 0);
    lv_obj_set_size(list_slider->slider, lv_pct(100), slider_height);
    lv_obj_set_style_margin_top(list_slider->slider, 12, 0);
    lv_obj_center(list_slider->slider);
    lv_obj_set_style_bg_opa(list_slider->slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(list_slider->slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_bg_color(list_slider->slider,
                              lv_color_darken(EOS_COLOR_GREEN, slider_main_bg_darken_lvl),
                              LV_PART_MAIN);

    return list_slider;
}

lv_obj_t *eos_row_create(lv_obj_t *parent,
                         const char *left_text,
                         const char *right_text,
                         const char *left_img_path,
                         int icon_w,
                         int icon_h)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
                          LV_FLEX_ALIGN_SPACE_BETWEEN, // Main axis space between
                          LV_FLEX_ALIGN_CENTER, // Cross axis center
                          LV_FLEX_ALIGN_CENTER);

    // Left image
    if (left_img_path)
    {
        lv_obj_t *icon = lv_image_create(row);
        lv_image_set_src(icon, left_img_path);
        eos_img_set_size(icon, icon_w, icon_h);
    }

    // Left text
    if (left_text)
    {
        lv_obj_t *left_label = lv_label_create(row);
        lv_label_set_text(left_label, left_text);
    }

    // Right text
    if (right_text)
    {
        lv_obj_t *right_label = lv_label_create(row);
        lv_label_set_text(right_label, right_text);
        lv_obj_set_flex_grow(right_label, 1); // Fill middle space
        lv_obj_set_style_text_align(right_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(right_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    }

    return row;
}
