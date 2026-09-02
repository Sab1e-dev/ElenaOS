/**
 * @file eos_card_stack.c
 * @brief Reusable vertically stacked card pager
 */

#include "eos_card_stack.h"

/* Includes ---------------------------------------------------*/
#include <stdlib.h>
#define EOS_LOG_TAG "CardStack"
#include "eos_log.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/
#define _GESTURE_LOCK_DISTANCE 8
#define _STACK_SWITCH_RATIO 3
#define _STACK_FLICK_VELOCITY 8
#define _STACK_PREVIOUS_REVEAL 32
#define _CROWN_PROXY_STEP 50

/* Private Structures -----------------------------------------*/

typedef enum
{
    _STACK_GESTURE_UNDECIDED = 0,
    _STACK_GESTURE_VERTICAL,
    _STACK_GESTURE_HORIZONTAL,
} _stack_gesture_axis_t;

struct eos_card_stack_item_t
{
    eos_card_stack_t *stack;
    eos_card_stack_item_t *prev;
    eos_card_stack_item_t *next;
    lv_obj_t *wrapper;
    lv_obj_t *card;
    lv_obj_t *touch_obj;
    uint32_t index;
    lv_coord_t gesture_start_x;
    lv_coord_t gesture_start_y;
    lv_coord_t gesture_start_drag;
    _stack_gesture_axis_t gesture_axis;
    lv_coord_t anim_start_y;
    int32_t anim_start_scale;
    lv_opa_t anim_start_opa;
    lv_coord_t anim_target_y;
    int32_t anim_target_scale;
    lv_opa_t anim_target_opa;
    lv_coord_t rendered_x;
    lv_coord_t rendered_y;
    int32_t rendered_scale;
    lv_opa_t rendered_opa;
    bool rendered_state_valid;
    bool reflow_anim_active;
    bool removed;
};

struct eos_card_stack_t
{
    lv_obj_t *container;
    lv_obj_t *crown_target;
    lv_obj_t *crown_content;
    eos_card_stack_config_t config;
    eos_card_stack_item_t *head;
    eos_card_stack_item_t *tail;
    uint32_t count;
    uint32_t focus;
    lv_coord_t drag;
    uint32_t transition_focus;
    uint32_t settle_focus;
    bool settling_focus;
    bool gesture_active;
    bool vertical_gesture_enabled;
    bool crown_syncing;
    uint16_t side_reveal_progress;
    eos_card_stack_item_t *side_reveal_item;
    eos_card_stack_item_t *gesture_item;
    bool destroying;
    eos_card_stack_focus_changed_cb_t focus_changed_cb;
    void *focus_changed_user_data;
};

/* Forward Declarations ---------------------------------------*/
static void _container_delete_cb(lv_event_t *e);
static void _item_delete_cb(lv_event_t *e);
static void _item_pressed_cb(lv_event_t *e);
static void _item_pressing_cb(lv_event_t *e);
static void _item_released_cb(lv_event_t *e);
static void _stack_anim_exec_cb(void *var, int32_t value);
static void _stack_anim_done_cb(lv_anim_t *a);
static void _item_reflow_anim_exec_cb(void *var, int32_t value);
static void _item_reflow_anim_done_cb(lv_anim_t *a);
static void _crown_scroll_cb(lv_event_t *e);
static void _crown_scroll_end_cb(lv_event_t *e);
static void _layout(eos_card_stack_t *stack, bool animate);
static void _apply_fixed_z_order(eos_card_stack_t *stack);
static void _update_crown_scroll_range(eos_card_stack_t *stack);
static void _sync_crown_scroll(eos_card_stack_t *stack);
static void _start_settle(eos_card_stack_t *stack, uint32_t target_focus, lv_coord_t target_drag);

/* Function Implementations -----------------------------------*/

static int32_t _lerp(int32_t a, int32_t b, int32_t progress)
{
    return a + ((b - a) * progress + 128) / 256;
}

static eos_card_stack_slot_t _slot_for_depth(const eos_card_stack_t *stack, uint32_t depth)
{
    eos_card_stack_slot_t slot = {
        .y = stack->config.top - (lv_coord_t)(depth * stack->config.step),
        .scale = 256,
        .opacity = LV_OPA_COVER,
    };

    if (stack->config.slot_count > 0)
    {
        uint32_t slot_index = depth < stack->config.slot_count ? depth : stack->config.slot_count - 1;
        slot = stack->config.slots[slot_index];
    }
    return slot;
}

static eos_card_stack_slot_t _previous_slot(const eos_card_stack_t *stack)
{
    eos_card_stack_slot_t slot = stack->config.previous_slot;
    if (slot.scale == 0)
    {
        eos_card_stack_slot_t front_slot = _slot_for_depth(stack, 0);
        slot.y = front_slot.y + stack->config.card_height - _STACK_PREVIOUS_REVEAL;
        slot.scale = 256;
        slot.opacity = LV_OPA_COVER;
    }
    return slot;
}

static eos_card_stack_slot_t _slot_for_index(const eos_card_stack_t *stack, uint32_t index, uint32_t focus)
{
    if (index < focus)
    {
        if (index + 1U == focus)
            return _previous_slot(stack);

        eos_card_stack_slot_t exit_slot = stack->config.exit_slot;
        if (exit_slot.scale == 0)
        {
            exit_slot.y = stack->config.top + stack->config.card_height + stack->config.step;
            exit_slot.scale = 256;
            exit_slot.opacity = LV_OPA_TRANSP;
        }
        return exit_slot;
    }
    return _slot_for_depth(stack, index - focus);
}

static uint32_t _depth_for_index(const eos_card_stack_t *stack, uint32_t index, uint32_t focus)
{
    return index < focus ? stack->count + 1U : index - focus;
}

static uint32_t _transition_distance(const eos_card_stack_t *stack)
{
    if (stack->config.slot_count > 1)
    {
        lv_coord_t distance = stack->config.slots[0].y - stack->config.slots[1].y;
        if (distance < 0)
            distance = -distance;
        if (distance > 0)
            return (uint32_t)distance;
    }
    return (uint32_t)stack->config.step;
}

static int32_t _get_position_q(const eos_card_stack_t *stack, lv_coord_t drag, uint32_t distance)
{
    if (!stack || stack->count == 0)
        return 0;

    int32_t position_q = (int32_t)stack->focus * 256;
    if (distance > 0)
        position_q += ((int32_t)drag * 256) / (int32_t)distance;

    int32_t max_position_q = (int32_t)(stack->count - 1) * 256;
    if (position_q < 0)
        return 0;
    if (position_q > max_position_q)
        return max_position_q;
    return position_q;
}

static void _set_card_visual_state(eos_card_stack_item_t *item, lv_coord_t y, int32_t scale, lv_opa_t opacity)
{
    if (!item || !item->wrapper || !item->card || !lv_obj_is_valid(item->wrapper) || !lv_obj_is_valid(item->card))
        return;

    if (!item->rendered_state_valid || item->rendered_y != y)
    {
        lv_obj_set_y(item->wrapper, y);
        item->rendered_y = y;
    }
    if (!item->rendered_state_valid || item->rendered_scale != scale)
    {
        lv_obj_set_style_transform_scale(item->card, scale, 0);
        item->rendered_scale = scale;
    }
    if (!item->rendered_state_valid || item->rendered_opa != opacity)
    {
        lv_obj_set_style_opa(item->card, opacity, 0);
        item->rendered_opa = opacity;
    }
    item->rendered_state_valid = true;
}

static void _item_reflow_anim_exec_cb(void *var, int32_t value)
{
    eos_card_stack_item_t *item = (eos_card_stack_item_t *)var;
    if (!item)
        return;

    _set_card_visual_state(item,
                           _lerp(item->anim_start_y, item->anim_target_y, value),
                           _lerp(item->anim_start_scale, item->anim_target_scale, value),
                           (lv_opa_t)_lerp(item->anim_start_opa, item->anim_target_opa, value));
}

static void _item_reflow_anim_done_cb(lv_anim_t *a)
{
    eos_card_stack_item_t *item = lv_anim_get_user_data(a);
    if (item)
        item->reflow_anim_active = false;
}

static void _crown_scroll_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    eos_card_stack_t *stack = target ? lv_obj_get_user_data(target) : NULL;
    if (!stack || stack->destroying || stack->crown_syncing || stack->count == 0)
        return;

    /* LVGL reports a negative scroll value while the pointer is stretching
     * the top edge. Never let that signed value wrap into a huge slot index. */
    lv_coord_t scroll_y = lv_obj_get_scroll_y(target);
    lv_coord_t scroll_max = lv_obj_get_scroll_top(target) + lv_obj_get_scroll_bottom(target);
    if (scroll_max < 0)
        scroll_max = 0;
    if (scroll_y < 0)
        scroll_y = 0;
    else if (scroll_y > scroll_max)
        scroll_y = scroll_max;
    uint32_t focus = (uint32_t)(scroll_y / _CROWN_PROXY_STEP);
    lv_coord_t remainder = scroll_y % _CROWN_PROXY_STEP;
    if (focus >= stack->count)
    {
        focus = stack->count - 1U;
        remainder = 0;
    }

    uint32_t old_focus = stack->focus;
    lv_coord_t distance = (lv_coord_t)_transition_distance(stack);
    stack->gesture_active = false;
    stack->gesture_item = NULL;
    stack->settling_focus = false;
    lv_anim_delete(stack->container, (lv_anim_exec_xcb_t)_stack_anim_exec_cb);
    stack->focus = focus;
    stack->drag = (lv_coord_t)((int32_t)remainder * distance / _CROWN_PROXY_STEP);
    _layout(stack, false);

    if (stack->focus_changed_cb && stack->focus != old_focus)
        stack->focus_changed_cb(stack, stack->focus, stack->focus_changed_user_data);
}

static void _crown_scroll_end_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    eos_card_stack_t *stack = target ? lv_obj_get_user_data(target) : NULL;
    if (!stack || stack->destroying || stack->crown_syncing || stack->count == 0)
        return;

    lv_coord_t scroll_y = lv_obj_get_scroll_y(target);
    lv_coord_t scroll_max = lv_obj_get_scroll_top(target) + lv_obj_get_scroll_bottom(target);
    if (scroll_max < 0)
        scroll_max = 0;
    if (scroll_y < 0)
        scroll_y = 0;
    else if (scroll_y > scroll_max)
        scroll_y = scroll_max;

    uint32_t slot = (uint32_t)(scroll_y / _CROWN_PROXY_STEP);
    lv_coord_t remainder = scroll_y % _CROWN_PROXY_STEP;
    if (slot >= stack->count)
    {
        slot = stack->count - 1U;
        remainder = 0;
    }

    /* One crown step represents one card transition. Snap to whichever slot
     * is closest after a touch throw or a crown animation. */
    uint32_t target_focus = slot;
    if (remainder >= _CROWN_PROXY_STEP / 2 && target_focus + 1U < stack->count)
        target_focus++;

    lv_coord_t distance = (lv_coord_t)_transition_distance(stack);
    lv_coord_t target_drag = (lv_coord_t)((int32_t)target_focus - (int32_t)stack->focus) * distance;
    _start_settle(stack, target_focus, target_drag);
}

static void _update_crown_scroll_range(eos_card_stack_t *stack)
{
    if (!stack || !stack->crown_target || !stack->crown_content)
        return;

    lv_obj_update_layout(stack->container);
    lv_coord_t view_height = lv_obj_get_height(stack->crown_target);
    if (view_height <= 0)
        return;

    lv_coord_t content_height = view_height;
    if (stack->count > 0)
        content_height += (lv_coord_t)(stack->count - 1U) * _CROWN_PROXY_STEP;
    lv_obj_set_size(stack->crown_content, 1, content_height);
}

static void _sync_crown_scroll(eos_card_stack_t *stack)
{
    if (!stack || !stack->crown_target || !stack->crown_content || stack->crown_syncing || stack->count == 0)
        return;

    uint32_t distance = _transition_distance(stack);
    lv_coord_t scroll_y = (lv_coord_t)(stack->focus * _CROWN_PROXY_STEP);
    if (distance > 0)
        scroll_y += (lv_coord_t)((int32_t)stack->drag * _CROWN_PROXY_STEP / (int32_t)distance);
    if (scroll_y < 0)
        scroll_y = 0;

    stack->crown_syncing = true;
    lv_obj_scroll_to(stack->crown_target, 0, scroll_y, LV_ANIM_OFF);
    stack->crown_syncing = false;
    _layout(stack, false);
}

static void _apply_fixed_z_order(eos_card_stack_t *stack)
{
    if (!stack || stack->count == 0)
        return;

    /* LVGL draws later siblings above earlier siblings. Move cards from the
     * oldest to the newest so the sequence order is the sole z-order rule:
     * card 1 is always above card 2, card 2 is always above card 3, etc. */
    for (eos_card_stack_item_t *item = stack->tail; item; item = item->prev)
        lv_obj_move_foreground(item->wrapper);
}

static void _layout(eos_card_stack_t *stack, bool animate)
{
    if (!stack || !stack->container)
        return;

    uint32_t distance = _transition_distance(stack);
    int32_t position_q = _get_position_q(stack, stack->drag, distance);
    uint32_t transition_focus = (uint32_t)(position_q / 256);
    int32_t progress = position_q % 256;
    if (stack->drag < 0 && progress != 0)
    {
        transition_focus++;
        progress = 256 - progress;
    }
    uint32_t from_focus = transition_focus;
    uint32_t to_focus = transition_focus;
    if (progress > 0)
    {
        if (stack->drag < 0 && transition_focus > 0)
            to_focus--;
        else if (stack->drag >= 0 && transition_focus + 1U < stack->count)
            to_focus++;
    }
    stack->transition_focus = transition_focus;

    /* The focus is intentionally not updated until release, so checking only
     * focus == first/last misses overscroll during a single gesture that
     * crosses several slots. Compare against the full virtual drag bounds
     * instead; once the clamped stack position reaches either end, the
     * excess drag becomes the continuous rubber-band translation. */
    lv_coord_t edge_overscroll = 0;
    if (stack->count > 0)
    {
        lv_coord_t min_drag = -(lv_coord_t)stack->focus * (lv_coord_t)distance;
        lv_coord_t max_drag = (lv_coord_t)(stack->count - 1U - stack->focus) * (lv_coord_t)distance;
        if (stack->drag < min_drag)
            edge_overscroll = stack->drag - min_drag;
        else if (stack->drag > max_drag)
            edge_overscroll = stack->drag - max_drag;
    }

    uint32_t side_reveal_index = stack->count;
    if (stack->side_reveal_item && stack->side_reveal_item->stack == stack && !stack->side_reveal_item->removed)
        side_reveal_index = stack->side_reveal_item->index;

    lv_coord_t target_x = (lv_obj_get_width(stack->container) - stack->config.wrapper_width) / 2;
    uint32_t i = 0;
    for (eos_card_stack_item_t *item = stack->head; item; item = item->next, i++)
    {
        if (item->removed)
            continue;

        item->index = i;
        eos_card_stack_slot_t from_slot = _slot_for_index(stack, i, from_focus);
        eos_card_stack_slot_t to_slot = _slot_for_index(stack, i, to_focus);
        lv_coord_t target_y = _lerp(from_slot.y, to_slot.y, progress);
        int32_t target_scale = _lerp(from_slot.scale, to_slot.scale, progress);
        lv_opa_t target_opa = (lv_opa_t)_lerp(from_slot.opacity, to_slot.opacity, progress);

        if (stack->side_reveal_progress > 0 && stack->drag == 0 && side_reveal_index < stack->count)
        {
            if (i < side_reveal_index)
            {
                eos_card_stack_slot_t exit_slot = stack->config.exit_slot;
                if (exit_slot.scale == 0)
                {
                    exit_slot.y = stack->config.top + stack->config.card_height + stack->config.step;
                    exit_slot.scale = 256;
                    exit_slot.opacity = LV_OPA_COVER;
                }
                target_y = _lerp(target_y, exit_slot.y, stack->side_reveal_progress);
                target_scale = _lerp(target_scale, exit_slot.scale, stack->side_reveal_progress);
                target_opa = (lv_opa_t)_lerp(target_opa, exit_slot.opacity, stack->side_reveal_progress);
            }
            else if (i > side_reveal_index)
            {
                uint32_t depth = _depth_for_index(stack, i, stack->focus);
                eos_card_stack_slot_t reveal_slot = _slot_for_depth(stack, depth + 1U);
                reveal_slot.opacity = LV_OPA_TRANSP;
                target_y = _lerp(target_y, reveal_slot.y, stack->side_reveal_progress);
                target_scale = _lerp(target_scale, reveal_slot.scale, stack->side_reveal_progress);
                target_opa = (lv_opa_t)_lerp(target_opa, LV_OPA_TRANSP, stack->side_reveal_progress);
            }
        }

        lv_coord_t scaled_height = (lv_coord_t)((int32_t)stack->config.card_height * target_scale / 256);
        lv_coord_t wrapper_y = target_y - (stack->config.card_height - scaled_height) / 2;
        wrapper_y += edge_overscroll;
        if (!item->rendered_state_valid || item->rendered_x != target_x)
        {
            lv_obj_set_x(item->wrapper, target_x);
            item->rendered_x = target_x;
        }
        item->anim_target_y = wrapper_y;
        item->anim_target_scale = target_scale;
        item->anim_target_opa = target_opa;

        if (!animate)
        {
            if (item->reflow_anim_active)
            {
                lv_anim_delete(item, _item_reflow_anim_exec_cb);
                item->reflow_anim_active = false;
            }
            _set_card_visual_state(item, wrapper_y, target_scale, target_opa);
            continue;
        }

        item->anim_start_y = lv_obj_get_y(item->wrapper);
        item->anim_start_scale = lv_obj_get_style_transform_scale_x(item->card, 0);
        item->anim_start_opa = lv_obj_get_style_opa(item->card, 0);
        if (item->reflow_anim_active)
        {
            lv_anim_delete(item, _item_reflow_anim_exec_cb);
            item->reflow_anim_active = false;
        }
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, item);
        lv_anim_set_values(&a, 0, 256);
        lv_anim_set_exec_cb(&a, _item_reflow_anim_exec_cb);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_time(&a, stack->config.animation_duration);
        lv_anim_set_user_data(&a, item);
        lv_anim_set_completed_cb(&a, _item_reflow_anim_done_cb);
        item->reflow_anim_active = true;
        lv_anim_start(&a);
    }
}

static lv_coord_t _damped_drag(eos_card_stack_t *stack, lv_coord_t drag)
{
    if (!stack)
        return drag;
    if (stack->count == 0)
        return 0;

    lv_coord_t distance = (lv_coord_t)_transition_distance(stack);
    lv_coord_t min_drag = -(lv_coord_t)stack->focus * distance;
    lv_coord_t max_drag = (lv_coord_t)(stack->count - 1 - stack->focus) * distance;
    if (drag < min_drag)
        return min_drag + (drag - min_drag) / 3;
    if (drag > max_drag)
        return max_drag + (drag - max_drag) / 3;
    return drag;
}

static void _start_settle(eos_card_stack_t *stack, uint32_t target_focus, lv_coord_t target_drag)
{
    if (!stack || !stack->container)
        return;

    lv_anim_delete(stack->container, (lv_anim_exec_xcb_t)_stack_anim_exec_cb);
    if (stack->drag == target_drag || stack->config.animation_duration == 0)
    {
        uint32_t old_focus = stack->focus;
        stack->focus = target_focus;
        stack->drag = 0;
        stack->transition_focus = stack->focus;
        stack->settling_focus = false;
        if (stack->focus_changed_cb && stack->focus != old_focus)
            stack->focus_changed_cb(stack, stack->focus, stack->focus_changed_user_data);
        _layout(stack, false);
        _sync_crown_scroll(stack);
        return;
    }
    stack->settle_focus = target_focus;
    stack->settling_focus = target_focus != stack->focus;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, stack->container);
    lv_anim_set_values(&a, stack->drag, target_drag);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)_stack_anim_exec_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_time(&a, stack->config.animation_duration);
    lv_anim_set_user_data(&a, stack);
    lv_anim_set_completed_cb(&a, _stack_anim_done_cb);
    lv_anim_start(&a);
}

static void _stack_anim_exec_cb(void *var, int32_t value)
{
    eos_card_stack_t *stack = NULL;
    if (var)
    {
        lv_obj_t *container = (lv_obj_t *)var;
        stack = lv_obj_get_user_data(container);
    }
    if (!stack || stack->destroying)
        return;
    stack->drag = (lv_coord_t)value;
    _layout(stack, false);
    _sync_crown_scroll(stack);
}

static void _stack_anim_done_cb(lv_anim_t *a)
{
    eos_card_stack_t *stack = lv_anim_get_user_data(a);
    if (!stack || stack->destroying)
        return;
    uint32_t old_focus = stack->focus;
    bool focus_changed = false;
    if (stack->settling_focus)
    {
        stack->focus = stack->settle_focus;
        stack->settling_focus = false;
        focus_changed = stack->focus != old_focus;
    }
    stack->drag = 0;
    stack->transition_focus = stack->focus;
    if (focus_changed && stack->focus_changed_cb)
        stack->focus_changed_cb(stack, stack->focus, stack->focus_changed_user_data);
    _layout(stack, false);
    _sync_crown_scroll(stack);
}

static void _unlink_item(eos_card_stack_item_t *item)
{
    eos_card_stack_t *stack = item ? item->stack : NULL;
    if (!stack)
        return;

    if (stack->side_reveal_item == item)
    {
        stack->side_reveal_item = NULL;
        stack->side_reveal_progress = 0;
    }
    uint32_t removed_index = item->index;
    if (item->prev)
        item->prev->next = item->next;
    else
        stack->head = item->next;
    if (item->next)
        item->next->prev = item->prev;
    else
        stack->tail = item->prev;

    if (removed_index < stack->focus && stack->focus > 0)
        stack->focus--;
    else if (stack->count > 1 && removed_index == stack->focus && stack->focus >= stack->count - 1)
        stack->focus--;
    if (stack->count > 0)
        stack->count--;
    if (stack->count == 0)
        stack->focus = 0;
    else if (stack->focus >= stack->count)
        stack->focus = stack->count - 1;
    item->removed = true;
    item->stack = NULL;
    item->prev = NULL;
    item->next = NULL;
}

static void _item_delete_cb(lv_event_t *e)
{
    eos_card_stack_item_t *item = lv_event_get_user_data(e);
    if (!item)
        return;
    if (item->stack && !item->removed)
        _unlink_item(item);
    eos_free(item);
}

static void _container_delete_cb(lv_event_t *e)
{
    eos_card_stack_t *stack = lv_event_get_user_data(e);
    if (!stack)
        return;

    stack->destroying = true;
    lv_anim_delete(stack->container, (lv_anim_exec_xcb_t)_stack_anim_exec_cb);
    if (stack->crown_target)
    {
        lv_obj_remove_event_cb(stack->crown_target, _crown_scroll_cb);
        lv_obj_remove_event_cb(stack->crown_target, _crown_scroll_end_cb);
        lv_obj_set_user_data(stack->crown_target, NULL);
    }
    uint32_t child_count = lv_obj_get_child_count(stack->container);
    for (uint32_t i = 0; i < child_count; i++)
    {
        lv_obj_t *wrapper = lv_obj_get_child(stack->container, i);
        if (wrapper == stack->crown_target)
            continue;
        eos_card_stack_item_t *item = lv_obj_get_user_data(wrapper);
        if (item)
        {
            lv_anim_delete(item, _item_reflow_anim_exec_cb);
            lv_obj_remove_event_cb(wrapper, _item_delete_cb);
            eos_free(item);
            lv_obj_set_user_data(wrapper, NULL);
        }
    }
    eos_free(stack);
    lv_obj_set_user_data(lv_event_get_target(e), NULL);
}

static void _item_pressed_cb(lv_event_t *e)
{
    eos_card_stack_item_t *item = lv_event_get_user_data(e);
    if (!item || !item->stack || item->removed)
        return;

    eos_card_stack_t *stack = item->stack;
    if (item->index != stack->focus)
    {
        item->gesture_axis = _STACK_GESTURE_HORIZONTAL;
        return;
    }
    if (!stack->vertical_gesture_enabled || stack->settling_focus || stack->drag != 0 || stack->gesture_active)
    {
        item->gesture_axis = _STACK_GESTURE_HORIZONTAL;
        return;
    }

    lv_anim_delete(item->stack->container, (lv_anim_exec_xcb_t)_stack_anim_exec_cb);
    stack->settling_focus = false;
    stack->gesture_active = true;
    stack->gesture_item = item;
    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);
    item->gesture_start_x = point.x;
    item->gesture_start_y = point.y;
    item->gesture_start_drag = item->stack->drag;
    item->gesture_axis = _STACK_GESTURE_UNDECIDED;
}

static void _item_pressing_cb(lv_event_t *e)
{
    eos_card_stack_item_t *item = lv_event_get_user_data(e);
    if (!item || !item->stack || item->removed)
        return;
    if (!item->stack->gesture_active || item->stack->gesture_item != item)
        return;

    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);
    lv_coord_t dx = point.x - item->gesture_start_x;
    lv_coord_t dy = point.y - item->gesture_start_y;
    if (item->gesture_axis == _STACK_GESTURE_UNDECIDED && abs(dx) + abs(dy) >= _GESTURE_LOCK_DISTANCE)
    {
        item->gesture_axis = abs(dy) > abs(dx) ? _STACK_GESTURE_VERTICAL : _STACK_GESTURE_HORIZONTAL;
    }
    if (item->gesture_axis != _STACK_GESTURE_VERTICAL)
        return;

    /* Touch scrolling is implemented by the stack itself. Keep the crown
     * scrollbar visible while the finger remains on the card. */
    if (item->stack->crown_target && lv_obj_is_valid(item->stack->crown_target))
        lv_obj_send_event(item->stack->crown_target, LV_EVENT_SCROLL_BEGIN, NULL);
    _sync_crown_scroll(item->stack);

    lv_coord_t damped_dy = (lv_coord_t)((int32_t)dy * item->stack->config.vertical_drag_factor / 256);
    /* A finger moving down moves the card stack toward the next card. */
    lv_coord_t drag = _damped_drag(item->stack, item->gesture_start_drag + damped_dy);
    if (drag == item->stack->drag)
        return;
    item->stack->drag = drag;
    _layout(item->stack, false);
    _sync_crown_scroll(item->stack);
}

static void _item_released_cb(lv_event_t *e)
{
    eos_card_stack_item_t *item = lv_event_get_user_data(e);
    if (!item || !item->stack || item->removed)
        return;

    eos_card_stack_t *stack = item->stack;
    if (!stack->gesture_active || stack->gesture_item != item)
        return;
    stack->gesture_active = false;
    stack->gesture_item = NULL;

    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);
    lv_coord_t dx = point.x - item->gesture_start_x;
    lv_coord_t dy = point.y - item->gesture_start_y;
    if (item->gesture_axis == _STACK_GESTURE_UNDECIDED && abs(dx) + abs(dy) >= _GESTURE_LOCK_DISTANCE)
    {
        item->gesture_axis = abs(dy) > abs(dx) ? _STACK_GESTURE_VERTICAL : _STACK_GESTURE_HORIZONTAL;
    }
    if (item->gesture_axis != _STACK_GESTURE_VERTICAL)
        return;

    uint32_t old_focus = stack->focus;
    uint32_t new_focus = old_focus;
    lv_point_t vector = {0, 0};
    lv_indev_get_vect(lv_indev_active(), &vector);
    lv_coord_t distance = (lv_coord_t)_transition_distance(stack);
    bool fast_flick = abs(vector.y) >= _STACK_FLICK_VELOCITY && abs(vector.y) > abs(vector.x);
    lv_coord_t drag_abs = abs(stack->drag);
    if (drag_abs >= distance / _STACK_SWITCH_RATIO || fast_flick)
    {
        lv_coord_t direction = stack->drag != 0 ? stack->drag : vector.y;
        int32_t steps = drag_abs >= distance / _STACK_SWITCH_RATIO ? (drag_abs + (distance * 2) / 3) / distance : 1;
        int32_t signed_steps = direction > 0 ? steps : -steps;
        int32_t target_focus = (int32_t)old_focus + signed_steps;
        if (target_focus < 0)
            target_focus = 0;
        if ((uint32_t)target_focus >= stack->count)
            target_focus = (int32_t)stack->count - 1;
        new_focus = (uint32_t)target_focus;
    }

    lv_coord_t target_drag = (lv_coord_t)((int32_t)new_focus - (int32_t)old_focus) * distance;

    _layout(stack, false);
    _start_settle(stack, new_focus, target_drag);
}

eos_card_stack_t *eos_card_stack_create(lv_obj_t *parent, const eos_card_stack_config_t *config)
{
    if (!parent || !config || config->card_width <= 0 || config->card_height <= 0 || config->step <= 0)
        return NULL;

    eos_card_stack_t *stack = eos_malloc_zeroed(sizeof(eos_card_stack_t));
    if (!stack)
        return NULL;
    stack->container = lv_obj_create(parent);
    if (!stack->container)
    {
        eos_free(stack);
        return NULL;
    }
    lv_obj_remove_style_all(stack->container);
    lv_obj_set_size(stack->container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(stack->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_clip_corner(stack->container, false, 0);
    lv_obj_remove_flag(stack->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(stack->container, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    stack->config = *config;
    stack->vertical_gesture_enabled = true;
    if (stack->config.vertical_drag_factor == 0)
        stack->config.vertical_drag_factor = 256;
    if (stack->config.vertical_drag_factor > 256U)
        stack->config.vertical_drag_factor = 256;
    if (stack->config.wrapper_width <= 0)
        stack->config.wrapper_width = stack->config.card_width;
    if (stack->config.slot_count > EOS_CARD_STACK_MAX_SLOTS)
        stack->config.slot_count = EOS_CARD_STACK_MAX_SLOTS;
    lv_obj_set_user_data(stack->container, stack);
    lv_obj_add_event_cb(stack->container, _container_delete_cb, LV_EVENT_DELETE, stack);

    /* Keep crown scrolling on an independent, invisible proxy. Touch input
     * is handled by the card gesture callbacks, so native LVGL scrolling must
     * not compete with that gesture or reverse its direction. */
    stack->crown_target = lv_obj_create(stack->container);
    if (!stack->crown_target)
    {
        lv_obj_remove_event_cb(stack->container, _container_delete_cb);
        lv_obj_set_user_data(stack->container, NULL);
        lv_obj_delete(stack->container);
        eos_free(stack);
        return NULL;
    }
    lv_obj_remove_style_all(stack->crown_target);
    lv_obj_set_size(stack->crown_target, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(stack->crown_target, 0, 0);
    lv_obj_add_flag(stack->crown_target, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(stack->crown_target, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(stack->crown_target, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(stack->crown_target, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(stack->crown_target, stack);
    lv_obj_add_event_cb(stack->crown_target, _crown_scroll_cb, LV_EVENT_SCROLL, stack);
    lv_obj_add_event_cb(stack->crown_target, _crown_scroll_end_cb, LV_EVENT_SCROLL_END, stack);

    stack->crown_content = lv_obj_create(stack->crown_target);
    if (!stack->crown_content)
    {
        lv_obj_remove_event_cb(stack->crown_target, _crown_scroll_cb);
        lv_obj_remove_event_cb(stack->crown_target, _crown_scroll_end_cb);
        lv_obj_set_user_data(stack->crown_target, NULL);
        lv_obj_remove_event_cb(stack->container, _container_delete_cb);
        lv_obj_set_user_data(stack->container, NULL);
        lv_obj_delete(stack->container);
        eos_free(stack);
        return NULL;
    }
    lv_obj_remove_style_all(stack->crown_content);
    lv_obj_set_pos(stack->crown_content, 0, 0);
    lv_obj_set_size(stack->crown_content, 1, lv_obj_get_height(stack->crown_target));
    lv_obj_remove_flag(stack->crown_content, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return stack;
}

eos_card_stack_item_t *eos_card_stack_add(eos_card_stack_t *stack, lv_obj_t *card, lv_obj_t *touch_obj)
{
    if (!stack || !stack->container || !card || !touch_obj || stack->destroying)
        return NULL;

    eos_card_stack_item_t *item = eos_malloc_zeroed(sizeof(eos_card_stack_item_t));
    if (!item)
        return NULL;

    item->stack = stack;
    item->card = card;
    item->touch_obj = touch_obj;
    item->wrapper = lv_obj_create(stack->container);
    if (!item->wrapper)
    {
        eos_free(item);
        return NULL;
    }
    lv_obj_remove_style_all(item->wrapper);
    lv_obj_set_size(item->wrapper, stack->config.wrapper_width, stack->config.card_height);
    lv_obj_set_style_bg_opa(item->wrapper, LV_OPA_TRANSP, 0);
    lv_obj_set_style_clip_corner(item->wrapper, false, 0);
    lv_obj_remove_flag(item->wrapper, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item->wrapper, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_user_data(item->wrapper, item);
    lv_obj_add_event_cb(item->wrapper, _item_delete_cb, LV_EVENT_DELETE, item);

    lv_obj_set_parent(card, item->wrapper);
    lv_obj_set_pos(card, stack->config.card_x, 0);
    lv_obj_set_style_transform_pivot_x(card, stack->config.card_width / 2, 0);
    lv_obj_set_style_transform_pivot_y(card, stack->config.card_height / 2, 0);
    lv_obj_set_style_transform_scale(card, 256, 0);
    lv_obj_add_event_cb(touch_obj, _item_pressed_cb, LV_EVENT_PRESSED, item);
    lv_obj_add_event_cb(touch_obj, _item_pressing_cb, LV_EVENT_PRESSING, item);
    lv_obj_add_event_cb(touch_obj, _item_released_cb, LV_EVENT_RELEASED, item);
    lv_obj_add_flag(touch_obj, LV_OBJ_FLAG_PRESS_LOCK);

    if (stack->tail)
    {
        stack->tail->next = item;
        item->prev = stack->tail;
    }
    else
    {
        stack->head = item;
    }
    stack->tail = item;
    stack->count++;
    _update_crown_scroll_range(stack);
    _apply_fixed_z_order(stack);
    _layout(stack, false);
    return item;
}

void eos_card_stack_remove(eos_card_stack_item_t *item)
{
    if (!item || !item->stack || item->removed)
        return;

    eos_card_stack_t *stack = item->stack;
    _unlink_item(item);
    if (item->wrapper && lv_obj_is_valid(item->wrapper))
        lv_obj_delete_async(item->wrapper);
    stack->drag = 0;
    _update_crown_scroll_range(stack);
    _sync_crown_scroll(stack);
    _apply_fixed_z_order(stack);
    _layout(stack, true);
}

void eos_card_stack_delete(eos_card_stack_t *stack)
{
    if (!stack || stack->destroying)
        return;
    stack->destroying = true;
    if (stack->container)
        lv_anim_delete(stack->container, (lv_anim_exec_xcb_t)_stack_anim_exec_cb);

    while (stack->head)
    {
        eos_card_stack_item_t *item = stack->head;
        stack->head = item->next;
        item->stack = NULL;
        item->removed = true;
        lv_anim_delete(item, _item_reflow_anim_exec_cb);
        lv_obj_remove_event_cb(item->wrapper, _item_delete_cb);
        lv_obj_delete(item->wrapper);
        eos_free(item);
        stack->count--;
    }
    stack->tail = NULL;
    if (stack->crown_target)
    {
        lv_obj_remove_event_cb(stack->crown_target, _crown_scroll_cb);
        lv_obj_remove_event_cb(stack->crown_target, _crown_scroll_end_cb);
        lv_obj_set_user_data(stack->crown_target, NULL);
    }
    if (stack->container)
        lv_obj_remove_event_cb(stack->container, _container_delete_cb);
    lv_obj_set_user_data(stack->container, NULL);
    eos_free(stack);
}

lv_obj_t *eos_card_stack_get_container(eos_card_stack_t *stack)
{
    return stack ? stack->container : NULL;
}

lv_obj_t *eos_card_stack_get_crown_target(eos_card_stack_t *stack)
{
    return stack ? stack->crown_target : NULL;
}

lv_obj_t *eos_card_stack_item_get_container(eos_card_stack_item_t *item)
{
    return item ? item->wrapper : NULL;
}

lv_obj_t *eos_card_stack_item_get_card(eos_card_stack_item_t *item)
{
    return item ? item->card : NULL;
}

uint32_t eos_card_stack_get_focus(eos_card_stack_t *stack)
{
    return stack ? stack->focus : 0;
}

bool eos_card_stack_item_is_focused(eos_card_stack_t *stack, eos_card_stack_item_t *item)
{
    return stack && item && item->stack == stack && !item->removed && item->index == stack->focus;
}

bool eos_card_stack_is_settled(eos_card_stack_t *stack)
{
    return stack && !stack->destroying && !stack->settling_focus && stack->drag == 0;
}

void eos_card_stack_set_focus_changed_cb(eos_card_stack_t *stack, eos_card_stack_focus_changed_cb_t cb, void *user_data)
{
    if (!stack)
        return;
    stack->focus_changed_cb = cb;
    stack->focus_changed_user_data = user_data;
}

void eos_card_stack_set_vertical_gesture_enabled(eos_card_stack_t *stack, bool enable)
{
    if (!stack || stack->destroying)
        return;
    stack->vertical_gesture_enabled = enable;
}

void eos_card_stack_set_side_reveal(eos_card_stack_t *stack, eos_card_stack_item_t *item, uint16_t progress)
{
    if (!stack || stack->destroying)
        return;
    if (item && (item->stack != stack || item->removed))
        return;
    uint16_t clamped_progress = progress > 256U ? 256U : progress;
    if (stack->side_reveal_item == item && stack->side_reveal_progress == clamped_progress)
        return;
    stack->side_reveal_item = item;
    stack->side_reveal_progress = clamped_progress;
    _layout(stack, false);
}
