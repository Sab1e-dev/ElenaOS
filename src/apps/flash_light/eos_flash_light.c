/**
 * @file eos_flash_light.c
 * @brief Flashlight
 */

#include "eos_flash_light.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "eos_theme.h"
#include "eos_config.h"
#include "eos_swipe_panel.h"
#define EOS_LOG_TAG "FlashLight"
#include "eos_log.h"
#include "eos_event.h"
#include "eos_icon.h"
#include "eos_anim.h"
#include "eos_utils.h"
#include "eos_card_pager.h"
#include "eos_watchface.h"
#include "eos_port.h"
#include "eos_service_config.h"
#include "eos_service_display.h"
#include "eos_app_list.h"
#include "eos_lang.h"
#include "eos_basic_widgets.h"
#include "eos_app_header.h"
#include "eos_mem.h"
#include "eos_chrome_manager.h"

/* Macros and Definitions -------------------------------------*/
#define _MASK_OPA LV_OPA_80
#define _OPA_MAX_DIST_DIV 1 /**< Distance to reach maximum opacity */
#define _OPA_SCALE 1000 /**< Scaling factor for proportion calculation */
#define _BRIGHTNESS_SMOOTH_DURATION 300
#define _IMMERSIVE_TAP_MAX_DISPLACEMENT 8
#define _IMMERSIVE_FADE_DURATION 220
#define _INDICATOR_MODE1_ACTIVE_COLOR EOS_COLOR_WHITE
#define _INDICATOR_MODE1_INACTIVE_COLOR EOS_COLOR_BLACK
#define _INDICATOR_MODE2_ACTIVE_COLOR EOS_COLOR_BLACK
#define _INDICATOR_MODE2_INACTIVE_COLOR EOS_COLOR_TEXT_GREY
#define _BRIGHTNESS_DURATION 750
typedef struct
{
    eos_swipe_panel_t *sp;
    lv_obj_t *mask;
} _pressing_user_data_t;

typedef struct
{
    eos_activity_t *activity;
    eos_card_pager_t *cp;
    lv_obj_t *pure_white_page;
    lv_obj_t *flash_page;
    lv_obj_t *red_page;
    lv_timer_t *flash_timer;
    bool flash_to_grey;
    bool immersive_mode;
} _flash_light_card_pager_ctx_t;
/* Variables --------------------------------------------------*/

static _pressing_user_data_t *_flash_light_ud = NULL;
static void _flash_light_overlay_pull_back(void);
static void _flash_light_overlay_hide(void);
static void _flash_light_overlay_on_focus(void);
static const eos_chrome_overlay_t _flash_light_overlay = {
    .pull_back = _flash_light_overlay_pull_back,
    .hide = _flash_light_overlay_hide,
    .on_focus = _flash_light_overlay_on_focus,
};

/* Function Implementations -----------------------------------*/
static void _flash_light_on_destroy(eos_activity_t *a);
static inline void _flash_light_delete(_pressing_user_data_t *ud);
static void _flash_light_card_pager_timer_cb(lv_timer_t *timer);
lv_obj_t *eos_flash_light_get_touch_obj(void);
static void _flash_light_card_pager_page_changed_cb(eos_card_pager_t *cp, uint8_t current_page_index, void *user_data);
static void _flash_light_card_pager_clicked_cb(lv_event_t *e);
static lv_obj_t *_flash_light_get_indicator_for_page(eos_card_pager_t *cp, lv_obj_t *page);
static void _flash_light_apply_page_visual_state(_flash_light_card_pager_ctx_t *ctx, uint8_t current_page_index);
static void _flash_light_set_indicator_visible_animated(_flash_light_card_pager_ctx_t *ctx,
                                                        bool visible,
                                                        uint32_t duration_ms);
static const eos_activity_lifecycle_t _flash_light_lifecycle = {
    .on_enter = NULL,
    .on_destroy = _flash_light_on_destroy,
};

static void _flash_light_indicator_fade_out_ready_cb(eos_anim_t *a)
{
    lv_obj_t *obj = a->tar_obj;
    if (!obj || !lv_obj_is_valid(obj))
        return;

    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
}

static void _flash_light_on_destroy(eos_activity_t *a)
{
    _flash_light_card_pager_ctx_t *ctx = (_flash_light_card_pager_ctx_t *)eos_activity_get_user_data(a);
    if (ctx)
    {
        if (ctx->flash_timer)
        {
            lv_timer_delete(ctx->flash_timer);
            ctx->flash_timer = NULL;
        }

        eos_activity_set_user_data(a, NULL);

        eos_free(ctx);
    }

    _flash_light_delete(NULL);
    eos_display_restore(_BRIGHTNESS_DURATION);
}

static inline void _flash_light_delete(_pressing_user_data_t *ud)
{
    EOS_CHECK_PTR_RETURN(ud);

    if (ud->sp)
        eos_swipe_panel_delete(ud->sp);

    if (ud->mask && lv_obj_is_valid(ud->mask))
        lv_obj_delete_async(ud->mask);

    ud->mask = NULL;
    ud->sp = NULL;

    if (_flash_light_ud == ud)
        _flash_light_ud = NULL;

    eos_free(ud);
    EOS_LOG_I("Flash light deleted");
}

static void _swipe_panel_pull_back_cb(lv_event_t *e)
{
    _pressing_user_data_t *ud = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(ud);

    int32_t swipe_obj_coord_y = lv_obj_get_y(ud->sp->swipe_obj);
    if (swipe_obj_coord_y >= EOS_DISPLAY_HEIGHT)
    {
        _flash_light_delete(ud);
        eos_display_restore(_BRIGHTNESS_DURATION);
    }
}

static void _flash_light_closed_cb(lv_event_t *e)
{
    EOS_LOG_I("Flash light closed");
    eos_chrome_manager_notify_overlay_closed(&_flash_light_overlay);
}

static void _flash_light_overlay_pull_back(void)
{
    eos_flash_light_pull_back();
}

static void _flash_light_overlay_hide(void)
{
    eos_flash_light_hide();
}

static void _flash_light_overlay_on_focus(void)
{
    lv_obj_t *touch_obj = eos_flash_light_get_touch_obj();
    if (touch_obj)
    {
        lv_obj_move_foreground(touch_obj);
    }
}

/**
 * @brief Used to set background opacity, decreases opacity as y-axis increases
 */
static void _swipe_panel_moving_cb(lv_event_t *e)
{
    _pressing_user_data_t *ud = lv_event_get_user_data(e);
    eos_swipe_panel_t *sp = ud->sp;

    lv_obj_update_layout(sp->swipe_obj);

    int32_t y = lv_obj_get_y(sp->swipe_obj);

    int32_t max_dist = EOS_DISPLAY_HEIGHT / _OPA_MAX_DIST_DIV;

    int32_t ratio = ((max_dist - y) * _OPA_SCALE) / max_dist;

    ratio = EOS_CLAMP(ratio, 0, _OPA_SCALE);

    lv_opa_t opa = (lv_opa_t)((ratio * _MASK_OPA) / _OPA_SCALE);

    EOS_LOG_I("y=%d, ratio=%d‰, opa=%d", y, ratio, opa);

    lv_obj_set_style_bg_opa(ud->mask, opa, 0);
}

static void _flash_light_clicked_cb(lv_event_t *e)
{
    _pressing_user_data_t *ud = lv_event_get_user_data(e);
    _flash_light_delete(ud);
    eos_flash_light_enter();
}

static void _flash_light_card_pager_timer_cb(lv_timer_t *timer)
{
    _flash_light_card_pager_ctx_t *ctx = lv_timer_get_user_data(timer);
    EOS_CHECK_PTR_RETURN(ctx && ctx->flash_page);

    ctx->flash_to_grey = !ctx->flash_to_grey;
    lv_obj_set_style_bg_color(ctx->flash_page,
                              ctx->flash_to_grey ? (ctx->immersive_mode ? EOS_COLOR_BLACK : EOS_COLOR_GREY)
                                                 : EOS_COLOR_WHITE,
                              0);
}

static void _flash_light_update_indicator_theme(_flash_light_card_pager_ctx_t *ctx,
                                                lv_obj_t *current_page,
                                                bool red_page_active)
{
    EOS_CHECK_PTR_RETURN(ctx && ctx->cp);

    for (eos_card_pager_node_t *node = ctx->cp->page_list_head; node; node = node->next)
    {
        if (!node->indicator)
            continue;

        bool is_current = (node->page == current_page);
        if (red_page_active)
        {
            // mode1: current=white, non-current=black
            lv_obj_set_style_bg_color(node->indicator,
                                      is_current ? _INDICATOR_MODE1_ACTIVE_COLOR : _INDICATOR_MODE1_INACTIVE_COLOR,
                                      0);
        }
        else
        {
            // mode2: current=black, non-current=grey
            lv_obj_set_style_bg_color(node->indicator,
                                      is_current ? _INDICATOR_MODE2_ACTIVE_COLOR : _INDICATOR_MODE2_INACTIVE_COLOR,
                                      0);
        }
    }
}

static void _flash_light_card_pager_page_changed_cb(eos_card_pager_t *cp, uint8_t current_page_index, void *user_data)
{
    _flash_light_card_pager_ctx_t *ctx = user_data;
    EOS_CHECK_PTR_RETURN(ctx && cp);

    _flash_light_apply_page_visual_state(ctx, current_page_index);
}

static void _flash_light_card_pager_clicked_cb(lv_event_t *e)
{
    _flash_light_card_pager_ctx_t *ctx = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(ctx && ctx->cp);

    if (ctx->cp->sw)
    {
        lv_coord_t disp = eos_slide_widget_get_displacement(ctx->cp->sw);
        if (abs(disp) > _IMMERSIVE_TAP_MAX_DISPLACEMENT)
            return;
    }

    ctx->immersive_mode = !ctx->immersive_mode;

    eos_activity_set_app_header_visible_animated(ctx->activity, !ctx->immersive_mode, _IMMERSIVE_FADE_DURATION);

    _flash_light_set_indicator_visible_animated(ctx, !ctx->immersive_mode, _IMMERSIVE_FADE_DURATION);

    _flash_light_apply_page_visual_state(ctx, ctx->cp->current_page_index);
}

static void _flash_light_set_indicator_visible_animated(_flash_light_card_pager_ctx_t *ctx,
                                                        bool visible,
                                                        uint32_t duration_ms)
{
    EOS_CHECK_PTR_RETURN(ctx && ctx->cp);

    lv_obj_t *indicator = ctx->cp->indicator_container;
    if (!indicator || !lv_obj_is_valid(indicator))
        return;

    if (duration_ms == 0)
    {
        if (visible)
            lv_obj_remove_flag(indicator, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    eos_anim_t *anim = eos_anim_fade_create(indicator,
                                            visible ? LV_OPA_TRANSP : LV_OPA_COVER,
                                            visible ? LV_OPA_COVER : LV_OPA_TRANSP,
                                            duration_ms,
                                            false);
    if (!anim)
        return;

    eos_anim_fade_set_main_opa(anim, true);

    if (visible)
    {
        lv_obj_remove_flag(indicator, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(indicator, LV_OPA_TRANSP, 0);
        eos_anim_start(anim);
    }
    else
    {
        if (lv_obj_has_flag(indicator, LV_OBJ_FLAG_HIDDEN))
        {
            eos_anim_del(anim);
            return;
        }
        lv_obj_set_style_opa(indicator, LV_OPA_COVER, 0);
        eos_anim_add_cb(anim, _flash_light_indicator_fade_out_ready_cb, NULL);
        eos_anim_start(anim);
    }
}

static void _flash_light_apply_page_visual_state(_flash_light_card_pager_ctx_t *ctx, uint8_t current_page_index)
{
    EOS_CHECK_PTR_RETURN(ctx && ctx->cp);

    lv_obj_t *current_page = eos_card_pager_get_page(ctx->cp, current_page_index);
    bool red_page_active = (current_page == ctx->red_page);
    bool flash_page_active = (current_page == ctx->flash_page);

    if (flash_page_active)
    {
        if (ctx->flash_page && lv_obj_is_valid(ctx->flash_page))
        {
            ctx->flash_to_grey = false;
            lv_obj_set_style_bg_color(ctx->flash_page, EOS_COLOR_WHITE, 0);
        }

        if (ctx->flash_timer)
        {
            lv_timer_reset(ctx->flash_timer);
            lv_timer_resume(ctx->flash_timer);
        }

        if (!ctx->immersive_mode)
            eos_activity_set_app_header_time_only_text_color(ctx->activity, EOS_COLOR_BLACK);
    }
    else
    {
        if (ctx->flash_page && lv_obj_is_valid(ctx->flash_page))
        {
            ctx->flash_to_grey = false;
            lv_obj_set_style_bg_color(ctx->flash_page, EOS_COLOR_WHITE, 0);
        }

        if (ctx->flash_timer)
        {
            lv_timer_pause(ctx->flash_timer);
        }

        if (!ctx->immersive_mode)
        {
            eos_activity_set_app_header_time_only_text_color(ctx->activity,
                                                             red_page_active ? EOS_COLOR_WHITE : EOS_COLOR_BLACK);
        }
    }

    if (!ctx->immersive_mode)
        _flash_light_update_indicator_theme(ctx, current_page, red_page_active);
}

static lv_obj_t *_flash_light_get_indicator_for_page(eos_card_pager_t *cp, lv_obj_t *page)
{
    EOS_CHECK_PTR_RETURN_VAL(cp && page, NULL);

    for (eos_card_pager_node_t *node = cp->page_list_head; node; node = node->next)
    {
        if (node->page == page)
        {
            return node->indicator;
        }
    }

    return NULL;
}

void eos_flash_light_show(void)
{
    if (_flash_light_ud)
    {
        EOS_LOG_W("Flash light is already showing");
        return;
    }

    _pressing_user_data_t *ud = eos_malloc(sizeof(_pressing_user_data_t));
    EOS_CHECK_PTR_RETURN(ud);

    lv_obj_t *layer_top = lv_layer_top();

    lv_obj_t *mask = lv_obj_create(layer_top);
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(mask, EOS_COLOR_BLACK, 0);

    ud->mask = mask;

    eos_swipe_panel_t *sp = eos_swipe_panel_create(layer_top);
    eos_swipe_panel_set_dir(sp, EOS_SWIPE_DIR_UP);
    eos_swipe_panel_slide_down(sp);
    eos_swipe_panel_hide_handle_bar(sp);
    lv_obj_set_style_bg_opa(sp->swipe_obj, LV_OPA_TRANSP, 0);

    ud->sp = sp;

    eos_slide_widget_add_event_cb_done(sp->sw, _swipe_panel_pull_back_cb, ud);
    eos_slide_widget_add_event_cb_moving(sp->sw, _swipe_panel_moving_cb, ud);
    eos_slide_widget_add_event_cb_closed(sp->sw, _flash_light_closed_cb, NULL);
    int32_t touch_area_height = EOS_DISPLAY_HEIGHT * 0.2;
    lv_obj_set_height(eos_slide_widget_get_touch_obj(sp->sw), touch_area_height);

    lv_obj_t *container = sp->swipe_obj;
    lv_obj_set_height(container, 2 * EOS_DISPLAY_HEIGHT);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *row1 = lv_obj_create(container);
    lv_obj_remove_style_all(row1);
    lv_obj_set_size(row1, lv_pct(100), touch_area_height);

    lv_obj_t *label = lv_label_create(row1);
    lv_obj_set_height(label, LV_SIZE_CONTENT);

    lv_label_set_text_fmt(label, "%s\n" RI_ARROW_DOWN_WIDE_FILL, eos_lang_get_text(STR_ID_APP_FLASH_LIGHT_DISMISS));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *flash_light = lv_obj_create(container);
    lv_obj_set_style_bg_color(flash_light, EOS_COLOR_WHITE, 0);
    lv_obj_set_size(flash_light, lv_pct(100), EOS_DISPLAY_HEIGHT);
    lv_obj_set_style_border_width(flash_light, 0, 0);
    lv_obj_set_style_radius(flash_light, EOS_DISPLAY_RADIUS, 0);
    lv_obj_add_event_cb(flash_light, _flash_light_clicked_cb, LV_EVENT_CLICKED, ud);

    _flash_light_ud = ud;
    eos_display_set_brightness(EOS_DISPLAY_BRIGHTNESS_MAX, _BRIGHTNESS_DURATION, true);
    eos_chrome_manager_notify_overlay_opened(&_flash_light_overlay);
}

bool eos_flash_light_is_open(void)
{
    if (!_flash_light_ud || !_flash_light_ud->sp || !_flash_light_ud->sp->sw)
        return false;
    return eos_slide_widget_get_state(_flash_light_ud->sp->sw) == EOS_SLIDE_WIDGET_STATE_OPEN;
}

lv_obj_t *eos_flash_light_get_touch_obj(void)
{
    if (!_flash_light_ud || !_flash_light_ud->sp || !_flash_light_ud->sp->sw)
        return NULL;
    return eos_slide_widget_get_touch_obj(_flash_light_ud->sp->sw);
}

void eos_flash_light_pull_back(void)
{
    if (_flash_light_ud && _flash_light_ud->sp)
    {
        eos_swipe_panel_pull_back(_flash_light_ud->sp);
    }
}

void eos_flash_light_hide(void)
{
    if (_flash_light_ud)
    {
        _flash_light_delete(_flash_light_ud);
    }
}

const eos_chrome_overlay_t *eos_flash_light_get_overlay_descriptor(void)
{
    return &_flash_light_overlay;
}

void eos_flash_light_enter(void)
{
    eos_display_set_brightness(EOS_DISPLAY_BRIGHTNESS_MAX, _BRIGHTNESS_DURATION, true);
    eos_activity_t *a = eos_activity_create(&_flash_light_lifecycle);
    if (!a)
        return;

    _flash_light_card_pager_ctx_t *ctx = eos_malloc_zeroed(sizeof(_flash_light_card_pager_ctx_t));
    if (!ctx)
    {
        eos_activity_back();
        return;
    }

    eos_activity_set_type(a, EOS_ACTIVITY_TYPE_APP);
    eos_activity_set_app_header_visible(a, true);
    eos_activity_set_app_header_time_only(a, true);

    lv_obj_t *view = eos_activity_get_view(a);
    if (!view)
    {
        eos_free(ctx);
        eos_activity_back();
        return;
    }

    ctx->activity = a;
    ctx->immersive_mode = false;
    eos_activity_set_user_data(a, ctx);

    lv_obj_remove_style_all(view);
    lv_obj_set_size(view, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(view, EOS_COLOR_WHITE, 0);

    eos_card_pager_t *cp = eos_card_pager_create(view, EOS_CARD_PAGER_DIR_HOR);
    if (cp)
    {
        ctx->cp = cp;

        // CardPager create() already provides the first page.
        lv_obj_t *page = eos_card_pager_get_page(cp, 0);
        lv_obj_set_style_bg_color(page, EOS_COLOR_WHITE, 0);
        ctx->pure_white_page = page;

        page = eos_card_pager_create_page(cp);
        lv_obj_set_style_bg_color(page, EOS_COLOR_WHITE, 0);
        ctx->flash_page = page;

        page = eos_card_pager_create_page(cp);
        lv_obj_set_style_bg_color(page, lv_palette_main(LV_PALETTE_RED), 0);
        ctx->red_page = page;

        ctx->flash_timer = lv_timer_create(_flash_light_card_pager_timer_cb, 200, ctx);
        if (ctx->flash_timer)
            lv_timer_pause(ctx->flash_timer);

        eos_card_pager_set_page_changed_cb(cp, _flash_light_card_pager_page_changed_cb, ctx);

        lv_obj_t *sw1_touch_obj = eos_slide_widget_get_touch_obj(cp->sw);
        if (sw1_touch_obj)
        {
            lv_obj_add_event_cb(sw1_touch_obj, _flash_light_card_pager_clicked_cb, LV_EVENT_CLICKED, ctx);
        }
    }

    eos_activity_enter(a);

    if (ctx->cp)
    {
        _flash_light_apply_page_visual_state(ctx, ctx->cp->current_page_index);
    }
}
