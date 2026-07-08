/**
 * @file eos_swipe_panel.c
 * @brief Swipe panel implementation
 */

#include "eos_swipe_panel.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "SwipePanel"
#include "eos_log.h"
#include "eos_event.h"
#include "eos_config.h"
#include "eos_theme.h"
#include "eos_port.h"
#include "eos_basic_widgets.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/
#define GESTURE_AREA_HEIGHT 50
#define TOUCH_BAR_MARGIN 20
#define HANDLE_BAR_WIDTH 80
#define HANDLE_BAR_HEIGHT 10
#define SWIPE_ANIM_DURATION 200

typedef struct
{
    eos_slide_widget_dir_t slide_dir;
    lv_coord_t target_base;
    lv_coord_t target_target;
    lv_coord_t touch_base;
    lv_coord_t touch_target;
    lv_coord_t swipe_obj_x;
    lv_coord_t swipe_obj_y;
    lv_coord_t touch_obj_x;
    lv_coord_t touch_obj_y;
    int32_t pull_back_target;
} _direction_config_t;

static const _direction_config_t _dir_configs[] = {
    [EOS_SWIPE_DIR_UP] =
        {
            .slide_dir = EOS_SLIDE_DIR_VER,
            .target_base = EOS_DISPLAY_HEIGHT,
            .target_target = 0,
            .touch_base = EOS_DISPLAY_HEIGHT - GESTURE_AREA_HEIGHT,
            .touch_target = 0,
            .swipe_obj_x = 0,
            .swipe_obj_y = EOS_DISPLAY_HEIGHT,
            .touch_obj_x = 0,
            .touch_obj_y = EOS_DISPLAY_HEIGHT - GESTURE_AREA_HEIGHT,
            .pull_back_target = EOS_DISPLAY_HEIGHT,
        },
    [EOS_SWIPE_DIR_DOWN] =
        {
            .slide_dir = EOS_SLIDE_DIR_VER,
            .target_base = -EOS_DISPLAY_HEIGHT,
            .target_target = 0,
            .touch_base = 0,
            .touch_target = EOS_DISPLAY_HEIGHT - GESTURE_AREA_HEIGHT,
            .swipe_obj_x = 0,
            .swipe_obj_y = -EOS_DISPLAY_HEIGHT,
            .touch_obj_x = 0,
            .touch_obj_y = 0,
            .pull_back_target = -EOS_DISPLAY_HEIGHT,
        },
    [EOS_SWIPE_DIR_LEFT] =
        {
            .slide_dir = EOS_SLIDE_DIR_HOR,
            .target_base = EOS_DISPLAY_WIDTH,
            .target_target = 0,
            .touch_base = EOS_DISPLAY_WIDTH - GESTURE_AREA_HEIGHT,
            .touch_target = 0,
            .swipe_obj_x = EOS_DISPLAY_WIDTH,
            .swipe_obj_y = 0,
            .touch_obj_x = EOS_DISPLAY_WIDTH - GESTURE_AREA_HEIGHT,
            .touch_obj_y = 0,
            .pull_back_target = EOS_DISPLAY_WIDTH,
        },
    [EOS_SWIPE_DIR_RIGHT] =
        {
            .slide_dir = EOS_SLIDE_DIR_HOR,
            .target_base = -EOS_DISPLAY_WIDTH,
            .target_target = 0,
            .touch_base = 0,
            .touch_target = EOS_DISPLAY_WIDTH - GESTURE_AREA_HEIGHT,
            .swipe_obj_x = -EOS_DISPLAY_WIDTH,
            .swipe_obj_y = 0,
            .touch_obj_x = 0,
            .touch_obj_y = 0,
            .pull_back_target = -EOS_DISPLAY_WIDTH,
        },
};
/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/
static void _update_handle_bar_position(eos_swipe_panel_t *sp, eos_swipe_dir_t dir);

static void _update_handle_bar_position(eos_swipe_panel_t *sp, eos_swipe_dir_t dir)
{
    EOS_CHECK_PTR_RETURN(sp && sp->handle_bar);
    switch (dir)
    {
        case EOS_SWIPE_DIR_UP:
            lv_obj_align(sp->handle_bar, LV_ALIGN_TOP_MID, 0, TOUCH_BAR_MARGIN);
            break;
        case EOS_SWIPE_DIR_DOWN:
            lv_obj_align(sp->handle_bar, LV_ALIGN_BOTTOM_MID, 0, -TOUCH_BAR_MARGIN);
            break;
        case EOS_SWIPE_DIR_LEFT:
            lv_obj_align(sp->handle_bar, LV_ALIGN_RIGHT_MID, -TOUCH_BAR_MARGIN, 0);
            break;
        case EOS_SWIPE_DIR_RIGHT:
            lv_obj_align(sp->handle_bar, LV_ALIGN_LEFT_MID, TOUCH_BAR_MARGIN, 0);
            break;
    }

    switch (dir)
    {
        case EOS_SWIPE_DIR_UP:
        case EOS_SWIPE_DIR_DOWN:
            lv_obj_set_size(sp->handle_bar, HANDLE_BAR_WIDTH, HANDLE_BAR_HEIGHT);
            break;
        case EOS_SWIPE_DIR_LEFT:
        case EOS_SWIPE_DIR_RIGHT:
            lv_obj_set_size(sp->handle_bar, HANDLE_BAR_HEIGHT, HANDLE_BAR_WIDTH);
            break;

        default:
            break;
    }
}

void eos_swipe_panel_move(eos_swipe_panel_t *sp, int32_t target, bool anim)
{
    EOS_CHECK_PTR_RETURN(sp);
    lv_coord_t cur = eos_slide_widget_get_current_pos(sp->sw);
    eos_slide_widget_move(sp->sw, cur, target, anim ? 120 : 0);
}

void eos_swipe_panel_pull_back(eos_swipe_panel_t *sp)
{
    EOS_CHECK_PTR_RETURN(sp);

    if (sp->dir >= EOS_SWIPE_DIR_COUNT)
        return;

    int32_t target = _dir_configs[sp->dir].pull_back_target;
    eos_slide_widget_set_anim_transition(sp->sw, EOS_SLIDE_WIDGET_STATE_REVERTING, EOS_SLIDE_WIDGET_STATE_IDLE);
    eos_swipe_panel_move(sp, target, true);
    eos_slide_widget_reverse(sp->sw);
}

void eos_swipe_panel_hide_handle_bar(eos_swipe_panel_t *sp)
{
    if (!sp || !sp->handle_bar)
        return;

    lv_obj_add_flag(sp->handle_bar, LV_OBJ_FLAG_HIDDEN);
}

void eos_swipe_panel_show_handle_bar(eos_swipe_panel_t *sp)
{
    if (!sp || !sp->handle_bar)
        return;

    lv_obj_remove_flag(sp->handle_bar, LV_OBJ_FLAG_HIDDEN);
}

void eos_swipe_panel_set_dir(eos_swipe_panel_t *sp, const eos_swipe_dir_t dir)
{
    EOS_CHECK_PTR_RETURN(sp);

    if (dir >= EOS_SWIPE_DIR_COUNT)
        return;

    sp->dir = dir;
    const _direction_config_t *cfg = &_dir_configs[dir];

    lv_obj_t *touch_obj = eos_slide_widget_get_touch_obj(sp->sw);

    lv_obj_set_size(touch_obj,
                    (cfg->slide_dir == EOS_SLIDE_DIR_VER) ? EOS_DISPLAY_WIDTH : GESTURE_AREA_HEIGHT,
                    (cfg->slide_dir == EOS_SLIDE_DIR_VER) ? GESTURE_AREA_HEIGHT : EOS_DISPLAY_HEIGHT);

    lv_obj_set_pos(sp->swipe_obj, cfg->swipe_obj_x, cfg->swipe_obj_y);
    lv_obj_set_pos(touch_obj, cfg->touch_obj_x, cfg->touch_obj_y);

    _update_handle_bar_position(sp, dir);

    eos_slide_widget_config_t config = {.target_base = cfg->target_base,
                                        .target_target = cfg->target_target,
                                        .touch_base = cfg->touch_base,
                                        .touch_target = cfg->touch_target,
                                        .threshold = EOS_THRESHOLD_40,
                                        .sync_touch_obj = true};
    eos_slide_widget_configure(sp->sw, &config);
}

void eos_swipe_panel_delete(eos_swipe_panel_t *sp)
{
    EOS_CHECK_PTR_RETURN(sp);
    lv_obj_delete_async(sp->swipe_obj);
    eos_free(sp);
}

static void _slide_widget_move_done_cb(lv_event_t *e)
{
    eos_swipe_panel_t *sp = (eos_swipe_panel_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(sp);

    eos_slide_widget_state_t state = eos_slide_widget_get_state(sp->sw);
    if (state == EOS_SLIDE_WIDGET_STATE_THRESHOLD)
    {
        bool closing_from_open = eos_slide_widget_is_reversed(sp->sw);
        EOS_LOG_I("Move cb threshold");
        eos_slide_widget_reverse(sp->sw);

        if (closing_from_open)
        {
            eos_slide_widget_set_anim_transition(sp->sw, EOS_SLIDE_WIDGET_STATE_REVERTING, EOS_SLIDE_WIDGET_STATE_IDLE);
        }
    }
}

void eos_swipe_panel_slide_down(eos_swipe_panel_t *sp)
{
    EOS_CHECK_PTR_RETURN(sp);

    if (sp->dir >= EOS_SWIPE_DIR_COUNT)
        return;

    /* Ensure objects are visible — they may have been hidden by
     * eos_chrome_manager_handle_activity_switch() via hide() */
    lv_obj_remove_flag(sp->swipe_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(eos_slide_widget_get_touch_obj(sp->sw), LV_OBJ_FLAG_HIDDEN);

    const _direction_config_t *cfg = &_dir_configs[sp->dir];
    eos_slide_widget_move(sp->sw, cfg->target_base, cfg->target_target, SWIPE_ANIM_DURATION);

    lv_obj_move_foreground(sp->swipe_obj);
    lv_obj_move_foreground(eos_slide_widget_get_touch_obj(sp->sw));
}

void eos_swipe_panel_hide(eos_swipe_panel_t *sp)
{
    EOS_CHECK_PTR_RETURN(sp);
    lv_obj_add_flag(sp->swipe_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(eos_slide_widget_get_touch_obj(sp->sw), LV_OBJ_FLAG_HIDDEN);
}

void eos_swipe_panel_show(eos_swipe_panel_t *sp)
{
    EOS_CHECK_PTR_RETURN(sp);
    lv_obj_remove_flag(sp->swipe_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(eos_slide_widget_get_touch_obj(sp->sw), LV_OBJ_FLAG_HIDDEN);
}

eos_swipe_panel_t *eos_swipe_panel_create(lv_obj_t *parent)
{
    eos_swipe_panel_t *sp = eos_malloc(sizeof(eos_swipe_panel_t));
    EOS_CHECK_PTR_RETURN_VAL(sp && parent, NULL);

    sp->swipe_obj = lv_obj_create(parent);
    lv_obj_set_size(sp->swipe_obj, EOS_DISPLAY_WIDTH, EOS_DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(sp->swipe_obj, EOS_COLOR_BLACK, 0);
    lv_obj_set_style_border_width(sp->swipe_obj, 0, 0);
    lv_obj_set_style_shadow_width(sp->swipe_obj, 0, 0);
    lv_obj_set_style_radius(sp->swipe_obj, EOS_DISPLAY_RADIUS, 0);
    lv_obj_set_style_clip_corner(sp->swipe_obj, true, 0);
    lv_obj_set_style_pad_all(sp->swipe_obj, 0, 0);
    lv_obj_set_y(sp->swipe_obj, -EOS_DISPLAY_HEIGHT);
    lv_obj_move_foreground(sp->swipe_obj);

    sp->handle_bar = lv_obj_create(sp->swipe_obj);
    lv_obj_set_size(sp->handle_bar, HANDLE_BAR_WIDTH, HANDLE_BAR_HEIGHT);
    lv_obj_set_style_radius(sp->handle_bar, 5, 0);
    lv_obj_set_style_bg_color(sp->handle_bar, EOS_COLOR_GREY, 0);
    lv_obj_set_style_border_width(sp->handle_bar, 0, 0);
    lv_obj_remove_flag(sp->handle_bar, LV_OBJ_FLAG_SCROLLABLE);

    _update_handle_bar_position(sp, EOS_SWIPE_DIR_DOWN);

    sp->sw = eos_slide_widget_create(parent, sp->swipe_obj, EOS_SLIDE_DIR_VER, 0, EOS_THRESHOLD_40);

    eos_slide_widget_config_t config = {.target_base = -EOS_DISPLAY_HEIGHT,
                                        .target_target = 0,
                                        .touch_base = 0,
                                        .touch_target = EOS_DISPLAY_HEIGHT - GESTURE_AREA_HEIGHT,
                                        .threshold = EOS_THRESHOLD_40,
                                        .sync_touch_obj = true};
    eos_slide_widget_configure(sp->sw, &config);

    sp->dir = EOS_SWIPE_DIR_DOWN;

    eos_slide_widget_add_event_cb_done(sp->sw, _slide_widget_move_done_cb, sp);

    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(sp->swipe_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(eos_slide_widget_get_touch_obj(sp->sw), LV_OBJ_FLAG_SCROLLABLE);

    return sp;
}
