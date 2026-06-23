/**
 * @file eos_slide_widget.c
 * @brief Slide widget implementation
 */

#include "eos_slide_widget.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EOS_LOG_TAG "SlideWidget"
#include "eos_log.h"
#include "eos_config.h"
#include "eos_theme.h"
#include "eos_anim.h"
#include "eos_port.h"
#include "eos_basic_widgets.h"
#include "eos_mem.h"
#include "eos_event.h"

/* Macros and Definitions -------------------------------------*/
#define _HIGHLIGHT_TOUCH_AREA 0
#define _SLIDE_ANIM_DURATION 120

/* Private Structures -----------------------------------------*/

struct eos_slide_widget_t
{
    lv_obj_t *touch_obj;
    lv_obj_t *target_obj;
    eos_slide_widget_dir_t dir;
    lv_coord_t base;
    lv_coord_t target;
    lv_coord_t touch_obj_base;
    lv_coord_t touch_obj_target;
    eos_threshold_t threshold;
    eos_slide_widget_state_t state;
    eos_slide_widget_state_t settle_state;
    lv_coord_t _indev_start;
    lv_coord_t _target_start;
    lv_coord_t last_touch_displacement;
    bool bidirectional;
    bool move_foreground_on_pressed;
    bool reversed;
    bool owns_touch_obj;
    bool sync_touch_obj;
    eos_slide_widget_state_t origin_settle_state;
};

/* Variables --------------------------------------------------*/
static lv_event_code_t _event_reached_threshold = LV_EVENT_LAST;
static lv_event_code_t _event_reverted = LV_EVENT_LAST;
static lv_event_code_t _event_moving = LV_EVENT_LAST;
static lv_event_code_t _event_done = LV_EVENT_LAST;
static lv_event_code_t _event_opened = LV_EVENT_LAST;
static lv_event_code_t _event_closed = LV_EVENT_LAST;

/* Private Function Prototypes --------------------------------*/
static void _sync_touch_obj_position(eos_slide_widget_t *sw, lv_coord_t target_pos);
static eos_slide_widget_state_t _get_resting_state(const eos_slide_widget_t *sw);

/* Function Implementations -----------------------------------*/

void eos_slide_widget_init(void)
{
    _event_reached_threshold = lv_event_register_id();
    _event_reverted = lv_event_register_id();
    _event_moving = lv_event_register_id();
    _event_done = lv_event_register_id();
    _event_opened = lv_event_register_id();
    _event_closed = lv_event_register_id();
    EOS_LOG_I("Slide widget events registered");
}

/*============================ Event Callback Registration ============================*/

void eos_slide_widget_add_event_cb_reached_threshold(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data)
{
    EOS_CHECK_PTR_RETURN(sw);
    lv_obj_add_event_cb(sw->touch_obj, cb, _event_reached_threshold, user_data);
}

void eos_slide_widget_add_event_cb_reverted(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data)
{
    EOS_CHECK_PTR_RETURN(sw);
    lv_obj_add_event_cb(sw->touch_obj, cb, _event_reverted, user_data);
}

void eos_slide_widget_add_event_cb_moving(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data)
{
    EOS_CHECK_PTR_RETURN(sw);
    lv_obj_add_event_cb(sw->touch_obj, cb, _event_moving, user_data);
}

void eos_slide_widget_add_event_cb_done(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data)
{
    EOS_CHECK_PTR_RETURN(sw);
    lv_obj_add_event_cb(sw->touch_obj, cb, _event_done, user_data);
}

void eos_slide_widget_add_event_cb_opened(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data)
{
    EOS_CHECK_PTR_RETURN(sw);
    lv_obj_add_event_cb(sw->touch_obj, cb, _event_opened, user_data);
}

void eos_slide_widget_add_event_cb_closed(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data)
{
    EOS_CHECK_PTR_RETURN(sw);
    lv_obj_add_event_cb(sw->touch_obj, cb, _event_closed, user_data);
}

/*============================ Private Helpers ============================*/

static const char *_state_to_str(eos_slide_widget_state_t state)
{
    switch (state)
    {
    case EOS_SLIDE_WIDGET_STATE_IDLE:
        return "IDLE";
    case EOS_SLIDE_WIDGET_STATE_DRAGGING:
        return "DRAGGING";
    case EOS_SLIDE_WIDGET_STATE_THRESHOLD:
        return "THRESHOLD";
    case EOS_SLIDE_WIDGET_STATE_REVERTING:
        return "REVERTING";
    case EOS_SLIDE_WIDGET_STATE_ANIMATING:
        return "ANIMATING";
    case EOS_SLIDE_WIDGET_STATE_OPEN:
        return "OPEN";
    default:
        return "UNKNOWN";
    }
}

static void _set_state(eos_slide_widget_t *sw, eos_slide_widget_state_t next, const char *reason)
{
    EOS_CHECK_PTR_RETURN(sw);
    if (sw->state == next)
        return;

    EOS_LOG_D("State: %s -> %s (%s)",
              _state_to_str(sw->state),
              _state_to_str(next),
              reason ? reason : "-");
    sw->state = next;
}

static void _sync_touch_obj_position(eos_slide_widget_t *sw, lv_coord_t target_pos)
{
    EOS_CHECK_PTR_RETURN(sw);

    if (!sw->sync_touch_obj || !sw->touch_obj)
        return;

    lv_coord_t range = sw->target - sw->base;
    if (range == 0)
        return;

    lv_coord_t touch_range = sw->touch_obj_target - sw->touch_obj_base;
    int32_t progress = ((int32_t)(target_pos - sw->base) * EOS_THRESHOLD_SCALE) / range;
    lv_coord_t touch_pos = sw->touch_obj_base + (touch_range * progress) / EOS_THRESHOLD_SCALE;

    if (sw->dir == EOS_SLIDE_DIR_VER)
    {
        lv_obj_set_y(sw->touch_obj, touch_pos);
    }
    else
    {
        lv_obj_set_x(sw->touch_obj, touch_pos);
    }
}

static eos_slide_widget_state_t _get_resting_state(const eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN_VAL(sw, EOS_SLIDE_WIDGET_STATE_IDLE);
    return sw->reversed ? EOS_SLIDE_WIDGET_STATE_OPEN : EOS_SLIDE_WIDGET_STATE_IDLE;
}

/*============================ Touch Event Handlers ============================*/

static void _touch_obj_pressed_cb(lv_event_t *e)
{
    eos_slide_widget_t *sw = (eos_slide_widget_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(sw);
    eos_anim_blocker_show();
    sw->origin_settle_state = _get_resting_state(sw);
    _set_state(sw, EOS_SLIDE_WIDGET_STATE_DRAGGING, "pressed");

    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);

    if (sw->dir == EOS_SLIDE_DIR_VER)
    {
        sw->_indev_start = p.y;
        sw->_target_start = lv_obj_get_y(sw->target_obj);
    }
    else
    {
        sw->_indev_start = p.x;
        sw->_target_start = lv_obj_get_x(sw->target_obj);
    }

    if (sw->move_foreground_on_pressed)
    {
        lv_obj_move_foreground(sw->target_obj);
        lv_obj_move_foreground(sw->touch_obj);
    }
}

static void _touch_obj_pressing_cb(lv_event_t *e)
{
    eos_slide_widget_t *sw = (eos_slide_widget_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(sw);
    EOS_LOG_I("Pressing: [%p]", sw->target_obj);
    _set_state(sw, EOS_SLIDE_WIDGET_STATE_DRAGGING, "pressing");
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);

    lv_coord_t touch_diff = (sw->dir == EOS_SLIDE_DIR_VER)
                                ? (p.y - sw->_indev_start)
                                : (p.x - sw->_indev_start);

    lv_coord_t new_pos = sw->_target_start;
    new_pos += touch_diff;

    if (sw->dir == EOS_SLIDE_DIR_VER)
    {
        if (lv_obj_has_class(lv_obj_get_parent(sw->target_obj), &lv_list_class))
        {
            lv_obj_set_style_translate_y(sw->target_obj, new_pos, 0);
        }
        else
        {
            lv_obj_set_y(sw->target_obj, new_pos);
        }
    }
    else
    {
        if (lv_obj_has_class(lv_obj_get_parent(sw->target_obj), &lv_list_class))
        {
            lv_obj_set_style_translate_x(sw->target_obj, new_pos, 0);
        }
        else
        {
            lv_obj_set_x(sw->target_obj, new_pos);
        }
    }

    _sync_touch_obj_position(sw, new_pos);
    lv_obj_send_event(sw->touch_obj, _event_moving, (void *)(intptr_t)new_pos);
}

/*============================ Animation Callbacks ============================*/

static void _slide_widget_anim_completed_cb(lv_anim_t *a)
{
    eos_slide_widget_t *sw = (eos_slide_widget_t *)lv_anim_get_user_data(a);
    EOS_CHECK_PTR_RETURN(sw);
    eos_slide_widget_state_t transit_state = sw->state;
    eos_slide_widget_state_t settle_state = sw->settle_state;
    eos_slide_widget_state_t origin_settle_state = sw->origin_settle_state;

    lv_obj_send_event(sw->touch_obj, _event_moving, NULL);
    lv_obj_send_event(sw->touch_obj, _event_done, sw);

    transit_state = sw->state;
    if (transit_state == EOS_SLIDE_WIDGET_STATE_THRESHOLD)
    {
        lv_obj_send_event(sw->touch_obj, _event_reached_threshold, sw);
    }
    else if (transit_state == EOS_SLIDE_WIDGET_STATE_REVERTING)
    {
        lv_obj_send_event(sw->touch_obj, _event_reverted, sw);
    }

    if (sw->state != transit_state)
    {
        settle_state = sw->state;
    }

    _set_state(sw, settle_state, "anim completed");
    sw->settle_state = settle_state;

    if (settle_state == EOS_SLIDE_WIDGET_STATE_OPEN &&
        origin_settle_state != EOS_SLIDE_WIDGET_STATE_OPEN)
    {
        lv_obj_send_event(sw->touch_obj, _event_opened, sw);
    }
    else if (settle_state == EOS_SLIDE_WIDGET_STATE_IDLE &&
             origin_settle_state != EOS_SLIDE_WIDGET_STATE_IDLE)
    {
        lv_obj_send_event(sw->touch_obj, _event_closed, sw);
    }

    eos_anim_blocker_hide();
}

static void _moving_set_x(void *var, int32_t value)
{
    eos_slide_widget_t *sw = (eos_slide_widget_t *)var;
    if (lv_obj_has_class(lv_obj_get_parent(sw->target_obj), &lv_list_class))
    {
        lv_obj_set_style_translate_x(sw->target_obj, value, 0);
    }
    else
    {
        lv_obj_set_x(sw->target_obj, value);
    }
    _sync_touch_obj_position(sw, value);
    lv_obj_send_event(sw->touch_obj, _event_moving, (void *)(intptr_t)value);
}

static void _moving_set_y(void *var, int32_t value)
{
    eos_slide_widget_t *sw = (eos_slide_widget_t *)var;
    if (lv_obj_has_class(lv_obj_get_parent(sw->target_obj), &lv_list_class))
    {
        lv_obj_set_style_translate_y(sw->target_obj, value, 0);
    }
    else
    {
        lv_obj_set_y(sw->target_obj, value);
    }
    _sync_touch_obj_position(sw, value);
    lv_obj_send_event(sw->touch_obj, _event_moving, (void *)(intptr_t)value);
}

static void _touch_obj_released_cb(lv_event_t *e)
{
    eos_slide_widget_t *sw = (eos_slide_widget_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(sw);

    lv_coord_t cur;
    if (sw->dir == EOS_SLIDE_DIR_VER)
    {
        cur = lv_obj_get_y(sw->target_obj);
    }
    else
    {
        cur = lv_obj_get_x(sw->target_obj);
    }

    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_coord_t indev_cur = (sw->dir == EOS_SLIDE_DIR_VER) ? p.y : p.x;

    sw->last_touch_displacement = indev_cur - sw->_indev_start;
    EOS_LOG_D("Last touch displacement = %d", sw->last_touch_displacement);

    lv_coord_t swipe_delta = indev_cur - sw->_indev_start;
    lv_coord_t move_delta = sw->target - sw->base;

    lv_coord_t target;
    eos_slide_widget_state_t transit_state;
    eos_slide_widget_state_t settle_state;
    lv_coord_t total_move_range = abs(move_delta);
    uint32_t scaled_swipe_dist = abs(swipe_delta) * EOS_THRESHOLD_SCALE;

    if (total_move_range != 0 && scaled_swipe_dist / total_move_range > sw->threshold)
    {
        if (swipe_delta * move_delta >= 0)
        {
            transit_state = EOS_SLIDE_WIDGET_STATE_THRESHOLD;
            settle_state = sw->reversed ? EOS_SLIDE_WIDGET_STATE_IDLE : EOS_SLIDE_WIDGET_STATE_OPEN;
            EOS_LOG_I("Forward swipe: go to target");
            if (sw->bidirectional)
                target = sw->base + abs(move_delta);
            else
                target = sw->target;
        }
        else
        {
            if (sw->bidirectional)
            {
                transit_state = EOS_SLIDE_WIDGET_STATE_THRESHOLD;
                settle_state = sw->reversed ? EOS_SLIDE_WIDGET_STATE_IDLE : EOS_SLIDE_WIDGET_STATE_OPEN;
                EOS_LOG_I("Bidirectional reverse swipe: go to opposite target");

                target = sw->base - abs(move_delta);
            }
            else
            {
                transit_state = EOS_SLIDE_WIDGET_STATE_REVERTING;
                settle_state = _get_resting_state(sw);
                EOS_LOG_I("Reverse swipe: revert to start position");
                target = sw->_target_start;
            }
        }
    }
    else
    {
        transit_state = EOS_SLIDE_WIDGET_STATE_REVERTING;
        settle_state = _get_resting_state(sw);
        EOS_LOG_I("Swipe below threshold: revert to start position");
        target = sw->_target_start;
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, sw);
    lv_anim_set_values(&a, cur, target);
    EOS_LOG_I("Move from %d to %d", cur, target);

    sw->settle_state = settle_state;
    _set_state(sw, transit_state, "start anim");

    if (sw->dir == EOS_SLIDE_DIR_VER)
    {
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)_moving_set_y);
    }
    else
    {
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)_moving_set_x);
    }

    lv_anim_set_time(&a, _SLIDE_ANIM_DURATION);
    lv_anim_set_user_data(&a, sw);
    lv_anim_set_completed_cb(&a, _slide_widget_anim_completed_cb);
    lv_anim_start(&a);
}

/*============================ Configuration APIs ============================*/

void eos_slide_widget_set_target_obj(eos_slide_widget_t *sw, lv_obj_t *target_obj)
{
    EOS_CHECK_PTR_RETURN(sw);
    sw->target_obj = target_obj;
}

void eos_slide_widget_configure(eos_slide_widget_t *sw, const eos_slide_widget_config_t *config)
{
    EOS_CHECK_PTR_RETURN(sw && config);

    sw->base = config->target_base;
    sw->target = config->target_target;
    sw->touch_obj_base = config->touch_base;
    sw->touch_obj_target = config->touch_target;
    sw->threshold = config->threshold;
    sw->sync_touch_obj = config->sync_touch_obj;
}

void eos_slide_widget_set_range(eos_slide_widget_t *sw, lv_coord_t base, lv_coord_t target)
{
    EOS_CHECK_PTR_RETURN(sw);
    sw->base = base;
    sw->target = target;
}

void eos_slide_widget_set_touch_range(eos_slide_widget_t *sw, lv_coord_t base, lv_coord_t target)
{
    EOS_CHECK_PTR_RETURN(sw);
    sw->touch_obj_base = base;
    sw->touch_obj_target = target;
    sw->sync_touch_obj = true;
}

void eos_slide_widget_set_sync_touch_obj(eos_slide_widget_t *sw, bool enable)
{
    EOS_CHECK_PTR_RETURN(sw);
    sw->sync_touch_obj = enable;
}

void eos_slide_widget_set_threshold(eos_slide_widget_t *sw, eos_threshold_t threshold)
{
    EOS_CHECK_PTR_RETURN(sw);
    sw->threshold = threshold;
}

void eos_slide_widget_set_bidirectional(eos_slide_widget_t *sw, bool enable)
{
    EOS_CHECK_PTR_RETURN(sw);
    sw->bidirectional = enable;
}

void eos_slide_widget_set_move_foreground_on_pressed(eos_slide_widget_t *sw, bool enable)
{
    EOS_CHECK_PTR_RETURN(sw);
    sw->move_foreground_on_pressed = enable;
}

void eos_slide_widget_set_anim_transition(eos_slide_widget_t *sw,
                                          eos_slide_widget_state_t transit_state,
                                          eos_slide_widget_state_t settle_state)
{
    EOS_CHECK_PTR_RETURN(sw);
    sw->settle_state = settle_state;
    _set_state(sw, transit_state, "external transition");
}

/*============================ State Query APIs ============================*/

eos_slide_widget_state_t eos_slide_widget_get_state(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN_VAL(sw, EOS_SLIDE_WIDGET_STATE_IDLE);
    return sw->state;
}

lv_coord_t eos_slide_widget_get_displacement(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN_VAL(sw, 0);
    return sw->last_touch_displacement;
}

lv_coord_t eos_slide_widget_get_current_pos(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN_VAL(sw, 0);
    if (sw->dir == EOS_SLIDE_DIR_VER)
    {
        return lv_obj_get_y(sw->target_obj);
    }
    else
    {
        return lv_obj_get_x(sw->target_obj);
    }
}

lv_coord_t eos_slide_widget_get_base(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN_VAL(sw, 0);
    return sw->base;
}

lv_coord_t eos_slide_widget_get_target(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN_VAL(sw, 0);
    return sw->target;
}

bool eos_slide_widget_is_reversed(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN_VAL(sw, false);
    return sw->reversed;
}

lv_obj_t *eos_slide_widget_get_touch_obj(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN_VAL(sw, NULL);
    return sw->touch_obj;
}

lv_obj_t *eos_slide_widget_get_target_obj(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN_VAL(sw, NULL);
    return sw->target_obj;
}

eos_slide_widget_dir_t eos_slide_widget_get_dir(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN_VAL(sw, EOS_SLIDE_DIR_VER);
    return sw->dir;
}

/*============================ Action APIs ============================*/

void eos_slide_widget_move(eos_slide_widget_t *sw, lv_coord_t start, lv_coord_t end, uint32_t duration)
{
    EOS_CHECK_PTR_RETURN(sw && sw->target_obj);
    eos_slide_widget_state_t transit_state = EOS_SLIDE_WIDGET_STATE_ANIMATING;
    eos_slide_widget_state_t settle_state = EOS_SLIDE_WIDGET_STATE_IDLE;
    sw->origin_settle_state = _get_resting_state(sw);

    if (sw->state == EOS_SLIDE_WIDGET_STATE_REVERTING)
    {
        transit_state = EOS_SLIDE_WIDGET_STATE_REVERTING;
        /* Preserve the externally assigned destination for explicit close animations. */
        settle_state = sw->settle_state;
    }
    else if (sw->state == EOS_SLIDE_WIDGET_STATE_THRESHOLD)
    {
        transit_state = EOS_SLIDE_WIDGET_STATE_THRESHOLD;
        /* Preserve the externally assigned destination for explicit open animations. */
        settle_state = sw->settle_state;
    }
    else if (end == sw->base)
    {
        transit_state = EOS_SLIDE_WIDGET_STATE_REVERTING;
        /* When reversed, base is the "away" (open) position */
        settle_state = sw->reversed ? EOS_SLIDE_WIDGET_STATE_OPEN : EOS_SLIDE_WIDGET_STATE_IDLE;
    }
    else if (end == sw->target)
    {
        transit_state = EOS_SLIDE_WIDGET_STATE_THRESHOLD;
        /* When reversed, target is the "home" (closed) position */
        settle_state = sw->reversed ? EOS_SLIDE_WIDGET_STATE_IDLE : EOS_SLIDE_WIDGET_STATE_OPEN;
    }

    sw->settle_state = settle_state;
    _set_state(sw, transit_state, "start anim");

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, sw);
    lv_anim_set_values(&a, start, end);
    EOS_LOG_I("Move from %d to %d", start, end);

    if (sw->dir == EOS_SLIDE_DIR_VER)
    {
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)_moving_set_y);
    }
    else
    {
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)_moving_set_x);
    }

    lv_anim_set_time(&a, duration);
    lv_anim_set_user_data(&a, sw);
    lv_anim_set_completed_cb(&a, _slide_widget_anim_completed_cb);
    lv_anim_start(&a);
}

void eos_slide_widget_reverse(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN(sw);
    lv_coord_t tmp;
    tmp = sw->base;
    sw->base = sw->target;
    sw->target = tmp;
    tmp = sw->touch_obj_base;
    sw->touch_obj_base = sw->touch_obj_target;
    sw->touch_obj_target = tmp;
    sw->reversed = sw->reversed ? false : true;
    EOS_LOG_D("Target path: %d -> %d  |  Touch path: %d -> %d", sw->base, sw->target, sw->touch_obj_base, sw->touch_obj_target);
}

void eos_slide_widget_sync_touch_obj(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN(sw);
    lv_coord_t current_pos = eos_slide_widget_get_current_pos(sw);
    _sync_touch_obj_position(sw, current_pos);
}

/*============================ Deletion ============================*/

static void _slide_widget_delete_cb(lv_event_t *e)
{
    eos_slide_widget_t *sw = (eos_slide_widget_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(sw);
    EOS_LOG_I("Deleting slide widget for target obj %p", lv_event_get_target(e));

    if (sw->touch_obj && sw->owns_touch_obj)
    {
        if (lv_obj_is_valid(sw->touch_obj))
        {
            lv_obj_delete(sw->touch_obj);
        }
        sw->touch_obj = NULL;
    }

    sw->target_obj = NULL;
    eos_free(sw);
}

void eos_slide_widget_delete(eos_slide_widget_t *sw)
{
    EOS_CHECK_PTR_RETURN(sw);
    EOS_LOG_I("Manually destroying slide widget %p", sw);

    if (sw->target_obj)
    {
        lv_obj_remove_event_cb(sw->target_obj, _slide_widget_delete_cb);
    }
    if (sw->touch_obj && sw->owns_touch_obj)
    {
        if (lv_obj_is_valid(sw->touch_obj))
        {
            lv_obj_remove_event_cb(sw->touch_obj, _touch_obj_pressed_cb);
            lv_obj_remove_event_cb(sw->touch_obj, _touch_obj_pressing_cb);
            lv_obj_remove_event_cb(sw->touch_obj, _touch_obj_released_cb);
            lv_obj_delete(sw->touch_obj);
        }
        sw->touch_obj = NULL;
    }

    if (sw->target_obj)
    {
        sw->target_obj = NULL;
    }

    eos_anim_blocker_hide();
    eos_free(sw);
}

/*============================ Creation ============================*/

static void _slide_widget_init_common(eos_slide_widget_t *sw,
                                      lv_obj_t *touch_obj,
                                      lv_obj_t *target_obj,
                                      eos_slide_widget_dir_t dir,
                                      lv_coord_t target,
                                      eos_threshold_t threshold)
{
    static bool _initialized = false;
    if (!_initialized)
    {
        eos_slide_widget_init();
        _initialized = true;
    }

    EOS_CHECK_PTR_RETURN(sw && touch_obj && target_obj);

    sw->dir = dir;
    sw->state = EOS_SLIDE_WIDGET_STATE_IDLE;
    sw->settle_state = EOS_SLIDE_WIDGET_STATE_IDLE;
    sw->threshold = threshold;
    sw->target_obj = target_obj;
    sw->base = (dir == EOS_SLIDE_DIR_VER)
                   ? lv_obj_get_y(target_obj)
                   : lv_obj_get_x(target_obj);
    sw->target = target;
    sw->bidirectional = false;
    sw->touch_obj = touch_obj;
    sw->sync_touch_obj = false;

    if (lv_obj_has_class(lv_obj_get_parent(target_obj), &lv_list_class))
    {
        sw->move_foreground_on_pressed = false;
    }
    else
    {
        sw->move_foreground_on_pressed = true;
    }

    lv_obj_add_event_cb(touch_obj, _touch_obj_pressed_cb, LV_EVENT_PRESSED, sw);
    lv_obj_add_event_cb(touch_obj, _touch_obj_pressing_cb, LV_EVENT_PRESSING, sw);
    lv_obj_add_event_cb(touch_obj, _touch_obj_released_cb, LV_EVENT_RELEASED, sw);
    lv_obj_add_event_cb(target_obj, _slide_widget_delete_cb, LV_EVENT_DELETE, sw);

    EOS_LOG_D("Slide widget created: [%p]\n"
              "target path %d->%d\n"
              "touch path %d->%d",
              sw, sw->base, sw->target, sw->touch_obj_base, sw->touch_obj_target);
}

eos_slide_widget_t *eos_slide_widget_create_with_touch(
    lv_obj_t *touch_obj,
    lv_obj_t *target_obj,
    eos_slide_widget_dir_t dir,
    lv_coord_t target,
    eos_threshold_t threshold)
{
    eos_slide_widget_t *sw = eos_malloc_zeroed(sizeof(eos_slide_widget_t));
    EOS_CHECK_PTR_RETURN_VAL(sw && touch_obj && target_obj, NULL);

    lv_obj_t *parent = lv_obj_get_parent(touch_obj);
    if (parent)
    {
        lv_coord_t parent_w = lv_obj_get_content_width(parent);
        lv_coord_t parent_h = lv_obj_get_content_height(parent);
        if (parent_w > 0 && parent_h > 0)
        {
            lv_obj_set_size(touch_obj, parent_w, parent_h);
        }
        lv_obj_set_pos(touch_obj, 0, 0);
    }

    _slide_widget_init_common(sw, touch_obj, target_obj, dir, target, threshold);
    sw->owns_touch_obj = false;
    return sw;
}

eos_slide_widget_t *eos_slide_widget_create(
    lv_obj_t *parent,
    lv_obj_t *target_obj,
    eos_slide_widget_dir_t dir,
    lv_coord_t target,
    eos_threshold_t threshold)
{
    eos_slide_widget_t *sw = eos_malloc_zeroed(sizeof(eos_slide_widget_t));
    EOS_CHECK_PTR_RETURN_VAL(sw && parent && target_obj, NULL);

    lv_obj_t *t = lv_obj_create(parent);
    lv_obj_remove_style_all(t);
#if _HIGHLIGHT_TOUCH_AREA
    lv_obj_set_style_bg_color(t, EOS_COLOR_RED, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_50, 0);
#else
    lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
#endif
    lv_obj_set_pos(t, 0, 0);

    _slide_widget_init_common(sw, t, target_obj, dir, target, threshold);
    sw->owns_touch_obj = true;
    return sw;
}
