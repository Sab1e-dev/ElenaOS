/**
 * @file eos_card_pager.c
 * @brief Card pager
 */

#include "eos_card_pager.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EOS_LOG_DISABLE
#define EOS_LOG_TAG "CardPager"
#include "eos_log.h"
#include "eos_config.h"
#include "eos_theme.h"
#include "eos_event.h"
#include "eos_port.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/
#define _CP_GET_DIR (cp->dir)
#define _IF_DIR_EQUAL_VER (_CP_GET_DIR == EOS_CARD_PAGER_DIR_VER)
#define _INDICATOR_DOT_WIDTH 12
#define _INDICATOR_DOT_HEIGHT _INDICATOR_DOT_WIDTH
#define _INDICATOR_ACTIVE_COLOR EOS_COLOR_WHITE
#define _INDICATOR_INACTIVE_COLOR EOS_COLOR_DARK_GREY_2
#define _PAGE_MARGIN 20
/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/
static void _page_switch_handler(eos_card_pager_t *cp);
static void _bring_pages_to_front(eos_card_pager_t *cp);
static inline void _set_indicator_active(lv_obj_t *indicator)
{
    lv_obj_set_style_bg_color(indicator, _INDICATOR_ACTIVE_COLOR, 0);
}

static inline void _set_indicator_inactive(lv_obj_t *indicator)
{
    lv_obj_set_style_bg_color(indicator, _INDICATOR_INACTIVE_COLOR, 0);
}

static inline void _notify_page_changed(eos_card_pager_t *cp)
{
    if (cp->page_changed_cb)
        cp->page_changed_cb(cp, cp->current_page_index, cp->page_changed_user_data);
}

static void _update_z_order(eos_card_pager_t *cp)
{
    EOS_CHECK_PTR_RETURN(cp);

    lv_obj_move_foreground(cp->background);
    lv_obj_t *cur_page = eos_card_pager_get_page(cp, cp->current_page_index);
    if (cur_page)
        lv_obj_move_foreground(cur_page);

    if (cp->sw)
    {
        lv_obj_t *touch_obj = eos_slide_widget_get_touch_obj(cp->sw);
        if (touch_obj)
            lv_obj_move_foreground(touch_obj);
    }

    if (cp->indicator_container)
        lv_obj_move_foreground(cp->indicator_container);
}

static void _bring_pages_to_front(eos_card_pager_t *cp)
{
    EOS_CHECK_PTR_RETURN(cp);

    uint8_t cur = cp->current_page_index;
    uint8_t last = cp->page_count > 0 ? cp->page_count - 1 : 0;
    uint8_t prev_index = (cur == 0) ? (cp->loop ? last : cur) : (cur - 1);
    uint8_t next_index = (cur == last) ? (cp->loop ? 0 : cur) : (cur + 1);

    lv_obj_t *prev_page = eos_card_pager_get_page(cp, prev_index);
    lv_obj_t *next_page = eos_card_pager_get_page(cp, next_index);
    lv_obj_t *cur_page = eos_card_pager_get_page(cp, cur);

    if (prev_page && prev_page != cur_page)
        lv_obj_move_foreground(prev_page);
    if (next_page && next_page != cur_page)
        lv_obj_move_foreground(next_page);
    if (cur_page)
        lv_obj_move_foreground(cur_page);

    if (cp->indicator_container)
        lv_obj_move_foreground(cp->indicator_container);
    if (cp->sw)
    {
        lv_obj_t *touch_obj = eos_slide_widget_get_touch_obj(cp->sw);
        if (touch_obj)
            lv_obj_move_foreground(touch_obj);
    }
}

static void _on_threshold_reached_cb(lv_event_t *e)
{
    eos_card_pager_t *cp = (eos_card_pager_t *)lv_event_get_user_data(e);
    eos_slide_widget_state_t state = eos_slide_widget_get_state(cp->sw);

    if (state == EOS_SLIDE_WIDGET_STATE_THRESHOLD)
    {
        uint8_t prev_index = cp->current_page_index;
        lv_coord_t displacement = eos_slide_widget_get_displacement(cp->sw);

        if (displacement > 0)
        {
            if (cp->current_page_index == 0)
                cp->current_page_index = cp->loop ? cp->page_count - 1 : 0;
            else
                cp->current_page_index--;
        }
        else if (displacement < 0)
        {
            if (cp->current_page_index == cp->page_count - 1)
                cp->current_page_index = cp->loop ? 0 : cp->page_count - 1;
            else
                cp->current_page_index++;
        }
        else
        {
            cp->current_page_index = prev_index;
        }

        if (cp->current_page_index == prev_index)
        {
            lv_obj_t *cur_page = eos_card_pager_get_page(cp, cp->current_page_index);
            if (cur_page)
                lv_obj_set_pos(cur_page, 0, 0);
            _page_switch_handler(cp);
            return;
        }

        lv_obj_t *indicator = eos_card_pager_get_indicator(cp, prev_index);
        _set_indicator_inactive(indicator);
        indicator = eos_card_pager_get_indicator(cp, cp->current_page_index);
        _set_indicator_active(indicator);

        _page_switch_handler(cp);
        _update_z_order(cp);
        _notify_page_changed(cp);
    }
}

static void _page_switch_handler(eos_card_pager_t *cp)
{
    EOS_LOG_I("Curent page index: %d / %d (loop=%d)", cp->current_page_index, cp->page_count, cp->loop);

    if (cp->page_count == 0)
        return;

    if (cp->page_count == 1)
    {
        eos_slide_widget_set_target_obj(cp->sw, eos_card_pager_get_page(cp, 0));
        eos_slide_widget_set_threshold(cp->sw, EOS_THRESHOLD_INFINITE);
        eos_slide_widget_set_range(cp->sw, 0, 0);
        return;
    }

    bool vertical = (cp->dir == EOS_CARD_PAGER_DIR_VER);
    lv_coord_t base_offset = vertical ? EOS_DISPLAY_HEIGHT : EOS_DISPLAY_WIDTH;

    uint8_t cur = cp->current_page_index;
    uint8_t last = cp->page_count - 1;
    uint8_t prev_index = (cur == 0) ? (cp->loop ? last : cur) : (cur - 1);
    uint8_t next_index = (cur == last) ? (cp->loop ? 0 : cur) : (cur + 1);

    lv_obj_t *cur_page = eos_card_pager_get_page(cp, cur);
    lv_obj_t *prev_page = eos_card_pager_get_page(cp, prev_index);
    lv_obj_t *next_page = eos_card_pager_get_page(cp, next_index);

    eos_slide_widget_set_target_obj(cp->sw, cur_page);
    eos_slide_widget_set_range(cp->sw, 0, base_offset);
    eos_slide_widget_set_threshold(cp->sw, EOS_THRESHOLD_30);

    if (cur_page)
        lv_obj_set_pos(cur_page, 0, 0);

    if (prev_page && prev_page != cur_page)
    {
        if (vertical)
            lv_obj_set_pos(prev_page, 0, -base_offset - _PAGE_MARGIN);
        else
            lv_obj_set_pos(prev_page, -base_offset - _PAGE_MARGIN, 0);
    }

    if (next_page && next_page != cur_page)
    {
        if (vertical)
            lv_obj_set_pos(next_page, 0, base_offset + _PAGE_MARGIN);
        else
            lv_obj_set_pos(next_page, base_offset + _PAGE_MARGIN, 0);
    }

    _bring_pages_to_front(cp);

    EOS_LOG_I("Slide config: current=%p, prev=%p, next=%p", cur_page, prev_page, next_page);
}

eos_card_pager_node_t *eos_card_pager_get_node(eos_card_pager_t *cp, uint8_t page_index)
{
    EOS_CHECK_PTR_RETURN_VAL(cp && cp->page_list_head, NULL);

    if (page_index >= cp->page_count)
        return NULL;

    eos_card_pager_node_t *cur = cp->page_list_head;
    for (uint8_t i = 0; i < page_index; i++)
    {
        cur = cur->next;
    }
    return cur ? cur : NULL;
}

lv_obj_t *eos_card_pager_get_indicator(eos_card_pager_t *cp, uint8_t page_index)
{
    eos_card_pager_node_t *cur = eos_card_pager_get_node(cp, page_index);
    return cur ? cur->indicator : NULL;
}

lv_obj_t *eos_card_pager_get_page(eos_card_pager_t *cp, uint8_t page_index)
{
    eos_card_pager_node_t *cur = eos_card_pager_get_node(cp, page_index);
    return cur ? cur->page : NULL;
}

static void _page_init(lv_obj_t *page)
{
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(page, EOS_COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(page, EOS_DISPLAY_RADIUS, 0);
}

lv_obj_t *eos_card_pager_create_page(eos_card_pager_t *cp)
{
    EOS_CHECK_PTR_RETURN_VAL(cp, NULL);

    eos_card_pager_node_t *node = eos_malloc_zeroed(sizeof(eos_card_pager_node_t));
    EOS_CHECK_PTR_RETURN_VAL(node, NULL);

    lv_obj_t *page = lv_obj_create(cp->container);
    _page_init(page);
    node->page = page;

    lv_obj_t *indicator = lv_obj_create(cp->indicator_container);
    lv_obj_remove_style_all(indicator);
    lv_obj_set_style_radius(indicator, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(indicator, _INDICATOR_DOT_WIDTH, _INDICATOR_DOT_HEIGHT);
    lv_obj_set_style_bg_color(indicator, _INDICATOR_INACTIVE_COLOR, 0);
    lv_obj_set_style_bg_opa(indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_margin_all(indicator, 10, 0);
    node->indicator = indicator;

    if (cp->page_list_head == NULL)
    {
        cp->page_list_head = node;
        cp->current_page_index = 0;
        _set_indicator_active(indicator);
        cp->page_count++;
    }
    else
    {
        eos_card_pager_node_t *cur = cp->page_list_head;
        while (cur->next)
            cur = cur->next;
        cur->next = node;
        node->prev = cur;
        if (cp->dir == EOS_CARD_PAGER_DIR_VER)
        {
            lv_obj_set_pos(page, 0, EOS_DISPLAY_HEIGHT);
        }
        else
        {
            lv_obj_set_pos(page, EOS_DISPLAY_WIDTH, 0);
        }
        cp->page_count++;
        _page_switch_handler(cp);
    }

    EOS_LOG_I("Page created: [%p]\nPage count: %d", page, cp->page_count);
    lv_obj_move_foreground(cp->indicator_container);
    return page;
}

bool eos_card_pager_remove_page(eos_card_pager_t *cp, uint8_t page_index)
{
    EOS_CHECK_PTR_RETURN_VAL(cp && cp->page_list_head, false);
    if (page_index >= cp->page_count)
        return false;

    eos_card_pager_node_t *cur = cp->page_list_head;
    for (uint8_t i = 0; i < page_index; i++)
        cur = cur->next;

    if (cur->prev)
        cur->prev->next = cur->next;
    else
        cp->page_list_head = cur->next;

    if (cur->next)
        cur->next->prev = cur->prev;

    if (cp->current_page_index == page_index)
        cp->current_page_index = 0;

    if (cur->page)
        lv_obj_delete_async(cur->page);
    if (cur->indicator)
        lv_obj_delete_async(cur->indicator);
    eos_free(cur);

    cp->page_count--;
    return true;
}

void eos_card_pager_set_loop(eos_card_pager_t *cp, bool loop)
{
    EOS_CHECK_PTR_RETURN(cp);
    cp->loop = loop;
    _page_switch_handler(cp);
}

bool eos_card_pager_move_node(eos_card_pager_t *cp, uint8_t from_index, uint8_t to_index)
{
    EOS_LOG_I("Move Node");
    EOS_CHECK_PTR_RETURN_VAL(cp && cp->page_list_head, false);
    if (from_index >= cp->page_count || to_index >= cp->page_count)
    {
        EOS_LOG_W("Invalid index: from=%d, to=%d (count=%d)", from_index, to_index, cp->page_count);
        return false;
    }

    if (from_index == to_index)
    {
        EOS_LOG_I("Node already in position %d", from_index);
        return true;
    }

    eos_card_pager_node_t *from = eos_card_pager_get_node(cp, from_index);
    if (!from)
        return false;

    eos_card_pager_node_t *to = eos_card_pager_get_node(cp, to_index);
    EOS_LOG_D("To color: 0x%06X", lv_obj_get_style_bg_color(to->page, 0));
    if (!to)
        return false;

    if (from->prev)
        from->prev->next = from->next;
    else
        cp->page_list_head = from->next;

    if (from->next)
        from->next->prev = from->prev;

    from->next = to->next;
    from->prev = to;

    if (to->next)
        to->next->prev = from;

    to->next = from;

    if (cp->current_page_index == from_index)
        cp->current_page_index = to_index;
    else if (from_index < cp->current_page_index && to_index >= cp->current_page_index)
        cp->current_page_index--;
    else if (from_index > cp->current_page_index && to_index <= cp->current_page_index)
        cp->current_page_index++;

    EOS_LOG_D("Current page index: %d", cp->current_page_index);

    uint8_t idx = 0;
    for (eos_card_pager_node_t *it = cp->page_list_head; it; it = it->next, idx++)
    {
        if (it->indicator && cp->indicator_container)
        {
            lv_obj_move_to_index(it->indicator, idx);
        }
    }

    _page_switch_handler(cp);

    lv_obj_t *cur_page_obj = eos_card_pager_get_page(cp, cp->current_page_index);
    lv_obj_set_pos(cur_page_obj, 0, 0);
    _update_z_order(cp);

    _notify_page_changed(cp);

    EOS_LOG_I("Moved node: from %d -> %d", from_index, to_index);
    return true;
}

void eos_card_pager_move_page(eos_card_pager_t *cp, uint8_t page_index)
{
    EOS_CHECK_PTR_RETURN(cp);
    if (page_index >= cp->page_count)
    {
        EOS_LOG_W("Invalid page index: %d (max=%d)", page_index, cp->page_count - 1);
        return;
    }

    if (cp->current_page_index == page_index)
    {
        EOS_LOG_I("Already at page %d", page_index);
        return;
    }

    uint8_t prev_index = cp->current_page_index;
    cp->current_page_index = page_index;

    lv_obj_t *indicator = eos_card_pager_get_indicator(cp, prev_index);
    if (indicator)
        lv_obj_set_style_bg_color(indicator, _INDICATOR_INACTIVE_COLOR, 0);

    indicator = eos_card_pager_get_indicator(cp, page_index);
    if (indicator)
        lv_obj_set_style_bg_color(indicator, _INDICATOR_ACTIVE_COLOR, 0);

    _page_switch_handler(cp);

    lv_obj_t *cur_page_obj = eos_card_pager_get_page(cp, page_index);
    lv_obj_set_pos(cur_page_obj, 0, 0);
    _update_z_order(cp);

    _notify_page_changed(cp);

    EOS_LOG_I("Page moved to %d / %d", page_index + 1, cp->page_count);
}

void eos_card_pager_set_page_changed_cb(eos_card_pager_t *cp, eos_card_pager_page_changed_cb_t cb, void *user_data)
{
    EOS_CHECK_PTR_RETURN(cp);

    cp->page_changed_cb = cb;
    cp->page_changed_user_data = user_data;
}

static void _on_slide_pressed_cb(lv_event_t *e)
{
    eos_card_pager_t *cp = (eos_card_pager_t *)lv_event_get_user_data(e);
    lv_obj_move_foreground(cp->indicator_container);
    lv_obj_move_foreground(eos_slide_widget_get_touch_obj(cp->sw));
}

static void _on_slide_moving_cb(lv_event_t *e)
{
    eos_card_pager_t *cp = (eos_card_pager_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(cp && cp->sw);

    eos_card_pager_node_t *cur_node = eos_card_pager_get_node(cp, cp->current_page_index);
    EOS_CHECK_PTR_RETURN(cur_node && cur_node->page);

    lv_obj_t *cur_obj = cur_node->page;
    lv_coord_t cur_pos = (lv_coord_t)(intptr_t)lv_event_get_param(e);

    bool vertical = (cp->dir == EOS_CARD_PAGER_DIR_VER);
    lv_coord_t base_offset = vertical ? EOS_DISPLAY_HEIGHT : EOS_DISPLAY_WIDTH;

    uint8_t cur = cp->current_page_index;
    uint8_t last = cp->page_count - 1;
    bool has_prev = cp->loop || (cur > 0);
    bool has_next = cp->loop || (cur < last);

    uint8_t prev_index = (cur == 0) ? last : (cur - 1);
    uint8_t next_index = (cur == last) ? 0 : (cur + 1);

    lv_obj_t *prev_page = has_prev ? eos_card_pager_get_page(cp, prev_index) : NULL;
    lv_obj_t *next_page = has_next ? eos_card_pager_get_page(cp, next_index) : NULL;

    if (cur_pos == 0)
    {
        eos_slide_widget_set_threshold(cp->sw, EOS_THRESHOLD_30);
        if (prev_page && prev_page != cur_obj)
        {
            if (vertical)
                lv_obj_set_pos(prev_page, 0, cur_pos - base_offset - _PAGE_MARGIN);
            else
                lv_obj_set_pos(prev_page, cur_pos - base_offset - _PAGE_MARGIN, 0);
        }

        if (next_page && next_page != cur_obj)
        {
            if (vertical)
                lv_obj_set_pos(next_page, 0, cur_pos + base_offset + _PAGE_MARGIN);
            else
                lv_obj_set_pos(next_page, cur_pos + base_offset + _PAGE_MARGIN, 0);
        }
        return;
    }

    if (cur_pos > 0)
    {
        if (!has_prev)
        {
            eos_slide_widget_set_threshold(cp->sw, EOS_THRESHOLD_INFINITE);
            lv_coord_t damped = cur_pos / 3;
            if (vertical)
                lv_obj_set_y(cur_obj, damped);
            else
                lv_obj_set_x(cur_obj, damped);
            return;
        }

        eos_slide_widget_set_threshold(cp->sw, EOS_THRESHOLD_30);
        if (prev_page && prev_page != cur_obj)
        {
            if (vertical)
                lv_obj_set_pos(prev_page, 0, cur_pos - base_offset - _PAGE_MARGIN);
            else
                lv_obj_set_pos(prev_page, cur_pos - base_offset - _PAGE_MARGIN, 0);
        }
    }
    else
    {
        if (!has_next)
        {
            eos_slide_widget_set_threshold(cp->sw, EOS_THRESHOLD_INFINITE);
            lv_coord_t damped = cur_pos / 3;
            if (vertical)
                lv_obj_set_y(cur_obj, damped);
            else
                lv_obj_set_x(cur_obj, damped);
            return;
        }

        eos_slide_widget_set_threshold(cp->sw, EOS_THRESHOLD_30);
        if (next_page && next_page != cur_obj)
        {
            if (vertical)
                lv_obj_set_pos(next_page, 0, cur_pos + base_offset + _PAGE_MARGIN);
            else
                lv_obj_set_pos(next_page, cur_pos + base_offset + _PAGE_MARGIN, 0);
        }
    }

    if (vertical)
    {
        lv_obj_set_y(cur_obj, cur_pos);
    }
    else
    {
        lv_obj_set_x(cur_obj, cur_pos);
    }

    _bring_pages_to_front(cp);
}

static void _on_slide_reverted_cb(lv_event_t *e)
{
    eos_card_pager_t *cp = (eos_card_pager_t *)lv_event_get_user_data(e);
    lv_obj_t *cur_page = eos_card_pager_get_page(cp, cp->current_page_index);
    lv_obj_set_pos(cur_page, 0, 0);
    _page_switch_handler(cp);
}

static void _container_delete_cb(lv_event_t *e)
{
    eos_card_pager_t *cp = (eos_card_pager_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(cp);
    cp->sw = NULL;

    eos_card_pager_node_t *node = cp->page_list_head;
    while (node)
    {
        eos_card_pager_node_t *next = node->next;
        eos_free(node);
        node = next;
    }
    cp->page_list_head = NULL;

    eos_free(cp);
}

eos_card_pager_t *eos_card_pager_create(lv_obj_t *parent, eos_card_pager_dir_t dir)
{
    eos_card_pager_t *cp = eos_malloc_zeroed(sizeof(eos_card_pager_t));
    EOS_CHECK_PTR_RETURN_VAL(cp && parent, NULL);

    cp->container = lv_obj_create(parent);
    lv_obj_remove_style_all(cp->container);
    lv_obj_set_size(cp->container, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_remove_flag(cp->container, LV_OBJ_FLAG_SCROLLABLE);
    cp->dir = dir;
    cp->loop = false;
    cp->background = lv_obj_create(cp->container);
    _page_init(cp->background);
    lv_obj_set_size(cp->background, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_set_pos(cp->background, 0, 0);
    lv_obj_set_style_bg_opa(cp->background, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cp->background, EOS_COLOR_BLACK, 0);

    lv_obj_t *indicator_container = lv_obj_create(cp->container);
    lv_obj_remove_style_all(indicator_container);
    cp->indicator_container = indicator_container;
    lv_obj_t *page = eos_card_pager_create_page(cp);

    lv_obj_t *touch_area = lv_obj_create(cp->container);
    lv_obj_remove_style_all(touch_area);
    lv_obj_set_size(touch_area, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_set_pos(touch_area, 0, 0);
    lv_obj_set_style_bg_opa(touch_area, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(touch_area, LV_OBJ_FLAG_SCROLLABLE);

    switch (dir)
    {
        case EOS_CARD_PAGER_DIR_VER:
        {
            lv_obj_set_size(indicator_container, LV_SIZE_CONTENT, EOS_DISPLAY_HEIGHT);
            lv_obj_align(indicator_container, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_obj_set_style_bg_opa(indicator_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(indicator_container, 0, 0);
            lv_obj_set_flex_flow(indicator_container, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(indicator_container,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_remove_flag(indicator_container, LV_OBJ_FLAG_SCROLLABLE);

            cp->sw = eos_slide_widget_create_with_touch(touch_area,
                                                        page,
                                                        EOS_SLIDE_DIR_VER,
                                                        EOS_DISPLAY_HEIGHT,
                                                        EOS_THRESHOLD_30);
            eos_slide_widget_set_bidirectional(cp->sw, true);
            break;
        }
        case EOS_CARD_PAGER_DIR_HOR:
        {
            lv_obj_set_size(indicator_container, EOS_DISPLAY_WIDTH, LV_SIZE_CONTENT);
            lv_obj_align(indicator_container, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_bg_opa(indicator_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(indicator_container, 0, 0);
            lv_obj_set_flex_flow(indicator_container, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(indicator_container,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_remove_flag(indicator_container, LV_OBJ_FLAG_SCROLLABLE);

            cp->sw = eos_slide_widget_create_with_touch(touch_area,
                                                        page,
                                                        EOS_SLIDE_DIR_HOR,
                                                        EOS_DISPLAY_WIDTH,
                                                        EOS_THRESHOLD_30);
            eos_slide_widget_set_bidirectional(cp->sw, true);
            break;
        }
    }

    eos_slide_widget_set_range(cp->sw, 0, (dir == EOS_CARD_PAGER_DIR_VER) ? EOS_DISPLAY_HEIGHT : EOS_DISPLAY_WIDTH);

    eos_slide_widget_add_event_cb_reached_threshold(cp->sw, _on_threshold_reached_cb, cp);
    eos_slide_widget_add_event_cb_moving(cp->sw, _on_slide_moving_cb, cp);
    eos_slide_widget_add_event_cb_reverted(cp->sw, _on_slide_reverted_cb, cp);

    lv_obj_add_event_cb(eos_slide_widget_get_touch_obj(cp->sw), _on_slide_pressed_cb, LV_EVENT_PRESSED, cp);

    lv_obj_add_event_cb(cp->container, _container_delete_cb, LV_EVENT_DELETE, cp);

    _page_switch_handler(cp);

    return cp;
}
