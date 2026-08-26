/**
 * @file eos_anim.c
 * @brief Animation library
 */

#include "eos_anim.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
// #define EOS_LOG_DISABLE
#define EOS_LOG_TAG "Animation"
#include "eos_log.h"
#include "eos_theme.h"
#include "eos_port.h"
#include "eos_mem.h"
#include "eos_overlay_layer.h"
#include "eos_basic_widgets.h"
#include "eos_activity.h"
#include "core/lv_obj_draw_private.h"

/* Macros and Definitions -------------------------------------*/
#define DEBUG_BLOCKER_VISIBLE 0
#define SNAP_COLOR_FORMAT LV_COLOR_FORMAT_RGB565

/* Variables --------------------------------------------------*/
static lv_obj_t *blocker = NULL;
static bool is_blocker_show = false;

/* Debug intercept (simulator-side only) */
static eos_anim_intercept_cb_t _eos_anim_intercept_cb = NULL;

/* Batch snapshot mode */
static bool _snap_batch_active = false;
static int _snap_batch_count = 0;

#define _SNAP_BATCH_MAX 128
typedef struct
{
    lv_obj_t *target;
    lv_obj_t *image;
    bool preserve_layout;
    lv_opa_t saved_opa;
} _snap_batch_entry_t;

static _snap_batch_entry_t _snap_batch_entries[_SNAP_BATCH_MAX];

static void _snapshot_sync_to_target(eos_anim_t *anim);

/* Function Implementations -----------------------------------*/

/* Basic Functionality ----------------------------------------*/

void eos_anim_set_backend(eos_anim_t *anim, eos_anim_backend_type_t type)
{
    if (!anim)
        return;
    anim->backend_type = type;
}

void eos_anim_set_delay(eos_anim_t *anim, uint32_t delay)
{
    if (!anim)
        return;
    anim->delay = delay;
}

void eos_anim_set_repeat_count(eos_anim_t *anim, uint16_t count)
{
    if (!anim)
        return;
    anim->repeat_count = count;
}

void eos_anim_set_playback_time(eos_anim_t *anim, uint32_t time_ms)
{
    if (!anim)
        return;
    anim->playback_time = time_ms;
}

void eos_anim_set_no_blocker(eos_anim_t *anim, bool no_blocker)
{
    if (!anim)
        return;
    anim->no_blocker = no_blocker;
}

void eos_anim_set_preserve_layout(eos_anim_t *anim, bool preserve)
{
    if (!anim)
        return;
    anim->preserve_layout = preserve;
}

void eos_anim_set_path(eos_anim_t *anim, lv_anim_path_cb_t path_cb)
{
    if (!anim || !path_cb)
        return;
    switch (anim->type)
    {
        case EOS_ANIM_SCALE:
            lv_anim_set_path_cb(&anim->anim.scale.a_width, path_cb);
            lv_anim_set_path_cb(&anim->anim.scale.a_height, path_cb);
            break;
        case EOS_ANIM_FADE:
            lv_anim_set_path_cb(&anim->anim.fade.a_opa, path_cb);
            break;
        case EOS_ANIM_MOVE:
            if (!anim->cfg.move.disable_x)
                lv_anim_set_path_cb(&anim->anim.move.a_x, path_cb);
            if (!anim->cfg.move.disable_y)
                lv_anim_set_path_cb(&anim->anim.move.a_y, path_cb);
            break;
        case EOS_ANIM_TRANSFORM_SCALE:
            lv_anim_set_path_cb(&anim->anim.transform_scale.a_scale, path_cb);
            break;
        case EOS_ANIM_IMAGE_SCALE:
            lv_anim_set_path_cb(&anim->anim.image_scale.a_scale, path_cb);
            break;
        case EOS_ANIM_RESIZE:
            if (!anim->cfg.resize.disable_w)
                lv_anim_set_path_cb(&anim->anim.resize.a_w, path_cb);
            if (!anim->cfg.resize.disable_h)
                lv_anim_set_path_cb(&anim->anim.resize.a_h, path_cb);
            break;
    }
}

static void _apply_bezier3_params(eos_anim_t *anim, int16_t bx1, int16_t by1, int16_t bx2, int16_t by2)
{
    if (!anim)
        return;
    switch (anim->type)
    {
        case EOS_ANIM_SCALE:
            lv_anim_set_bezier3_param(&anim->anim.scale.a_width, bx1, by1, bx2, by2);
            lv_anim_set_bezier3_param(&anim->anim.scale.a_height, bx1, by1, bx2, by2);
            break;
        case EOS_ANIM_FADE:
            lv_anim_set_bezier3_param(&anim->anim.fade.a_opa, bx1, by1, bx2, by2);
            break;
        case EOS_ANIM_MOVE:
            if (!anim->cfg.move.disable_x)
                lv_anim_set_bezier3_param(&anim->anim.move.a_x, bx1, by1, bx2, by2);
            if (!anim->cfg.move.disable_y)
                lv_anim_set_bezier3_param(&anim->anim.move.a_y, bx1, by1, bx2, by2);
            break;
        case EOS_ANIM_TRANSFORM_SCALE:
            lv_anim_set_bezier3_param(&anim->anim.transform_scale.a_scale, bx1, by1, bx2, by2);
            break;
        case EOS_ANIM_IMAGE_SCALE:
            lv_anim_set_bezier3_param(&anim->anim.image_scale.a_scale, bx1, by1, bx2, by2);
            break;
        case EOS_ANIM_RESIZE:
            if (!anim->cfg.resize.disable_w)
                lv_anim_set_bezier3_param(&anim->anim.resize.a_w, bx1, by1, bx2, by2);
            if (!anim->cfg.resize.disable_h)
                lv_anim_set_bezier3_param(&anim->anim.resize.a_h, bx1, by1, bx2, by2);
            break;
        default:
            break;
    }
}

void eos_anim_set_path_bezier3(eos_anim_t *anim, int16_t bx1, int16_t by1, int16_t bx2, int16_t by2)
{
    if (!anim)
        return;
    eos_anim_set_path(anim, lv_anim_path_custom_bezier3);
    _apply_bezier3_params(anim, bx1, by1, bx2, by2);
}

eos_anim_group_t *eos_anim_group_create(eos_anim_group_cb_t cb, void *user_data)
{
    eos_anim_group_t *group = eos_malloc_zeroed(sizeof(eos_anim_group_t));
    if (!group)
        return NULL;
    group->callback = cb;
    group->user_data = user_data;
    EOS_LOG_I("Group created: %p", group);
    return group;
}

void eos_anim_group_del(eos_anim_group_t *group)
{
    if (!group)
        return;
    EOS_LOG_I("Group freed: %p", group);
    eos_free(group);
}

void eos_anim_group_attach(eos_anim_t *anim, eos_anim_group_t *group)
{
    if (!anim || !group)
        return;
    anim->group = group;
    anim->no_blocker = true;
    group->expected++;
    EOS_LOG_I("Anim[%p] attached to group[%p] (expected=%d)", anim, group, group->expected);
}

void eos_anim_del(eos_anim_t *anim)
{
    if (!anim)
        return;

    if (anim->group)
    {
        eos_anim_group_t *g = anim->group;
        anim->group = NULL;
        g->completed++;
        EOS_LOG_E("Anim[%p] DEL: group[%p] completed=%d expected=%d", anim, g, g->completed, g->expected);
        if (g->completed >= g->expected && g->callback)
        {
            EOS_LOG_E("Group[%p] ALL COMPLETE via del: triggering callback", g);
            g->callback(g->user_data);
        }
    }

    if (anim->snap_image && lv_obj_is_valid(anim->snap_image))
    {
        _snapshot_sync_to_target(anim);
        lv_obj_delete(anim->snap_image);
        anim->snap_image = NULL;
    }
    if (anim->snap_buf)
    {
        eos_draw_buf_destroy(anim->snap_buf);
        anim->snap_buf = NULL;
    }
    if (anim->tar_obj && lv_obj_is_valid(anim->tar_obj))
    {
        if (anim->preserve_layout)
        {
            lv_obj_set_style_opa(anim->tar_obj, anim->saved_orig_opa, 0);
        }
        else
        {
            lv_obj_remove_flag(anim->tar_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (anim->auto_delete_obj && anim->tar_obj && lv_obj_is_valid(anim->tar_obj))
    {
        EOS_LOG_D("Target obj freed [%p]", anim->tar_obj);
        lv_obj_delete_async(anim->tar_obj);
        anim->tar_obj = NULL;
    }

    eos_free(anim);
    EOS_LOG_D("Anim freed");
}

void eos_anim_set_auto_delete(eos_anim_t *anim)
{
    EOS_CHECK_PTR_RETURN(anim);
    anim->auto_delete_obj = true;
}

void eos_anim_add_cb(eos_anim_t *anim, eos_anim_cb_t user_cb, void *user_data)
{
    if (!anim)
        return;
    anim->user_cb = user_cb;
    anim->user_data = user_data;
}

void *eos_anim_get_user_data(eos_anim_t *anim)
{
    return anim ? anim->user_data : NULL;
}

void eos_anim_blocker_show(void)
{
    if (is_blocker_show)
        return;

    blocker = lv_obj_create(eos_overlay_get_snapshot_layer());
    lv_obj_remove_style_all(blocker);
#if DEBUG_BLOCKER_VISIBLE
    lv_obj_set_style_bg_color(blocker, EOS_COLOR_MINT, 0);
    lv_obj_set_style_bg_opa(blocker, LV_OPA_40, 0);
#endif
    lv_obj_set_size(blocker, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(blocker, LV_OBJ_FLAG_CLICKABLE);
    is_blocker_show = true;
}

void eos_anim_blocker_hide(void)
{
    if (is_blocker_show)
    {
        if (!(blocker && lv_obj_is_valid(blocker) && lv_obj_has_class(blocker, &lv_obj_class)))
            return;
        lv_obj_delete_async(blocker);
        blocker = NULL;
        is_blocker_show = false;
    }
}

/* Animation Execution Callbacks ------------------------------*/

static void _set_width_cb(void *var, int32_t v)
{
    lv_obj_set_width((lv_obj_t *)var, v);
}

static void _set_height_cb(void *var, int32_t v)
{
    lv_obj_set_height((lv_obj_t *)var, v);
}

static void _set_x_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0);
}

static void _set_y_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t *)var, v, 0);
}

static void _set_scale_cb(void *var, int32_t v)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)var, v, 0);
}

static void _set_image_scale_cb(void *var, int32_t v)
{
    lv_image_set_scale((lv_obj_t *)var, v);
}

static void _set_opa_bg_cb(void *var, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void _set_opa_layered_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa_layered((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void _set_opa_main_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

/* Animation Completion Callbacks -----------------------------*/

static void _free_anim_later(lv_timer_t *t)
{
    eos_anim_t *anim = lv_timer_get_user_data(t);
    eos_anim_del(anim);
}

static void _eos_anim_ready_cb(lv_anim_t *a)
{
    eos_anim_t *anim = lv_anim_get_user_data(a);
    anim->anim_completed_count++;

    if (anim->anim_completed_count == anim->anim_count)
    {
        if (anim->snap_image && lv_obj_is_valid(anim->snap_image))
        {
            _snapshot_sync_to_target(anim);
            lv_obj_delete(anim->snap_image);
            anim->snap_image = NULL;
        }
        if (anim->snap_buf)
        {
            eos_draw_buf_destroy(anim->snap_buf);
            anim->snap_buf = NULL;
        }
        if (anim->tar_obj && lv_obj_is_valid(anim->tar_obj))
        {
            if (anim->preserve_layout)
            {
                lv_obj_set_style_opa(anim->tar_obj, anim->saved_orig_opa, 0);
            }
            else
            {
                lv_obj_remove_flag(anim->tar_obj, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (anim->user_cb)
        {
            anim->user_cb(anim);
        }

        if (anim->group)
        {
            eos_anim_group_t *g = anim->group;
            anim->group = NULL;
            g->completed++;
            EOS_LOG_E("Anim[%p] group[%p]: completed=%d expected=%d", anim, g, g->completed, g->expected);
            if (g->completed >= g->expected && g->callback)
            {
                EOS_LOG_E("Group[%p] ALL COMPLETE: triggering cleanup callback", g);
                g->callback(g->user_data);
            }
        }

        lv_timer_t *t = lv_timer_create(_free_anim_later, 10, anim);
        lv_timer_set_repeat_count(t, 1);
    }

    if (!anim->no_blocker && !anim->group)
    {
        eos_anim_blocker_hide();
    }
}

/* Animation Initialization Functions -------------------------*/

static void _init_width_anim(lv_anim_t *a,
                             lv_obj_t *obj,
                             int32_t start,
                             int32_t end,
                             uint32_t duration,
                             eos_anim_t *ctx)
{
    lv_anim_init(a);
    lv_anim_set_var(a, obj);
    lv_anim_set_values(a, start, end);
    lv_anim_set_exec_cb(a, _set_width_cb);
    lv_anim_set_path_cb(a, lv_anim_path_ease_out);
    lv_anim_set_duration(a, duration);
    if (!ctx)
        return;
    lv_anim_set_completed_cb(a, _eos_anim_ready_cb);
    lv_anim_set_user_data(a, ctx);
}

static void _init_height_anim(lv_anim_t *a,
                              lv_obj_t *obj,
                              int32_t start,
                              int32_t end,
                              uint32_t duration,
                              eos_anim_t *ctx)
{
    lv_anim_init(a);
    lv_anim_set_var(a, obj);
    lv_anim_set_values(a, start, end);
    lv_anim_set_exec_cb(a, _set_height_cb);
    lv_anim_set_path_cb(a, lv_anim_path_ease_out);
    lv_anim_set_duration(a, duration);
    if (!ctx)
        return;
    lv_anim_set_completed_cb(a, _eos_anim_ready_cb);
    lv_anim_set_user_data(a, ctx);
}

static void _init_x_anim(lv_anim_t *a, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration, eos_anim_t *ctx)
{
    lv_anim_init(a);
    lv_anim_set_var(a, obj);
    lv_anim_set_values(a, start, end);
    lv_anim_set_exec_cb(a, _set_x_cb);
    lv_anim_set_path_cb(a, lv_anim_path_ease_out);
    lv_anim_set_duration(a, duration);
    if (!ctx)
        return;
    lv_anim_set_completed_cb(a, _eos_anim_ready_cb);
    lv_anim_set_user_data(a, ctx);
}

static void _init_y_anim(lv_anim_t *a, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration, eos_anim_t *ctx)
{
    lv_anim_init(a);
    lv_anim_set_var(a, obj);
    lv_anim_set_values(a, start, end);
    lv_anim_set_exec_cb(a, _set_y_cb);
    lv_anim_set_path_cb(a, lv_anim_path_ease_out);
    lv_anim_set_duration(a, duration);
    if (!ctx)
        return;
    lv_anim_set_completed_cb(a, _eos_anim_ready_cb);
    lv_anim_set_user_data(a, ctx);
}

static void _init_scale_anim(lv_anim_t *a,
                             lv_obj_t *obj,
                             int32_t start,
                             int32_t end,
                             uint32_t duration,
                             eos_anim_t *ctx)
{
    lv_anim_init(a);
    lv_anim_set_var(a, obj);
    lv_anim_set_values(a, start, end);
    lv_anim_set_exec_cb(a, _set_scale_cb);
    lv_anim_set_path_cb(a, lv_anim_path_ease_in_out);
    lv_anim_set_duration(a, duration);
    if (!ctx)
        return;
    lv_anim_set_completed_cb(a, _eos_anim_ready_cb);
    lv_anim_set_user_data(a, ctx);
}

static void _init_image_scale_anim(lv_anim_t *a,
                                   lv_obj_t *obj,
                                   int32_t start,
                                   int32_t end,
                                   uint32_t duration,
                                   eos_anim_t *ctx)
{
    lv_anim_init(a);
    lv_anim_set_var(a, obj);
    lv_anim_set_values(a, start, end);
    lv_anim_set_exec_cb(a, _set_image_scale_cb);
    lv_anim_set_path_cb(a, lv_anim_path_ease_in_out);
    lv_anim_set_duration(a, duration);
    if (!ctx)
        return;
    lv_anim_set_completed_cb(a, _eos_anim_ready_cb);
    lv_anim_set_user_data(a, ctx);
}

static void _select_opa_exec_cb(lv_anim_t *a, eos_anim_t *anim)
{
    if (anim->cfg.fade.main_opa)
    {
        lv_anim_set_exec_cb(a, _set_opa_main_cb);
    }
    else if (anim->cfg.fade.layered)
    {
        lv_anim_set_exec_cb(a, _set_opa_layered_cb);
    }
    else
    {
        lv_anim_set_exec_cb(a, _set_opa_bg_cb);
    }
}

static void _snapshot_present_once(lv_obj_t *image, const char *tag)
{
    if (!(image && lv_obj_is_valid(image)))
    {
        return;
    }

    LV_UNUSED(tag);

    lv_display_t *display = lv_display_get_default();
    if (!display)
    {
        EOS_LOG_W("snapshot present skipped: no default display (%s)", tag ? tag : "unknown");
        return;
    }

    lv_obj_invalidate(image);
    lv_refr_now(display);
}

static void _init_opa_anim(lv_anim_t *a, lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration, eos_anim_t *ctx)
{
    lv_anim_init(a);
    lv_anim_set_var(a, obj);
    lv_anim_set_values(a, start, end);
    lv_anim_set_path_cb(a, lv_anim_path_ease_in_out);
    lv_anim_set_duration(a, duration);
    if (ctx)
        _select_opa_exec_cb(a, ctx);
    else
        lv_anim_set_exec_cb(a, _set_opa_layered_cb);
    if (!ctx)
        return;
    lv_anim_set_completed_cb(a, _eos_anim_ready_cb);
    lv_anim_set_user_data(a, ctx);
}

/* Snapshot Backend -------------------------------------------*/

/*
 * 方案五 — Batch snapshot for list transitions.
 * Call eos_anim_snapshot_batch_begin() before
 * creating animations with SNAPSHOT backend, and
 * eos_anim_snapshot_batch_flush() after all are created.
 * This defers the "hide original + lv_refr_now" until all
 * snapshots are ready, eliminating intermediate per-item flushes.
 */
void eos_anim_snapshot_batch_begin(void)
{
    _snap_batch_active = true;
    _snap_batch_count = 0;
    EOS_LOG_I("[SNAP_BATCH] batch begin");
}

void eos_anim_snapshot_batch_flush(void)
{
    EOS_LOG_I("[SNAP_BATCH] batch flush: %d entries", _snap_batch_count);

    /* Hide all originals */
    for (int i = 0; i < _snap_batch_count; i++)
    {
        _snap_batch_entry_t *e = &_snap_batch_entries[i];
        if (!(e->target && lv_obj_is_valid(e->target)))
            continue;
        if (e->preserve_layout)
        {
            e->saved_opa = lv_obj_get_style_opa(e->target, 0);
            lv_obj_set_style_opa(e->target, LV_OPA_TRANSP, 0);
        }
        else
        {
            lv_obj_add_flag(e->target, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Single refresh cycle: all snapshots visible, all originals hidden */
    lv_display_t *disp = lv_display_get_default();
    if (disp)
    {
        /* invalidate all snapshot images to force re-render */
        for (int i = 0; i < _snap_batch_count; i++)
        {
            _snap_batch_entry_t *e = &_snap_batch_entries[i];
            if (e->image && lv_obj_is_valid(e->image))
                lv_obj_invalidate(e->image);
        }
        lv_refr_now(disp);
    }

    _snap_batch_active = false;
    _snap_batch_count = 0;
    EOS_LOG_I("[SNAP_BATCH] batch flush done");
}

static void _snapshot_apply_start_values(eos_anim_t *anim, lv_obj_t *image)
{
    if (!image)
        return;

    switch (anim->type)
    {
        case EOS_ANIM_SCALE:
            lv_obj_set_width(image, anim->anim.scale.a_width.start_value);
            lv_obj_set_height(image, anim->anim.scale.a_height.start_value);
            break;
        case EOS_ANIM_FADE:
            if (anim->cfg.fade.layered)
                lv_obj_set_style_opa_layered(image, (lv_opa_t)anim->anim.fade.a_opa.start_value, 0);
            else if (anim->cfg.fade.main_opa)
                lv_obj_set_style_opa(image, (lv_opa_t)anim->anim.fade.a_opa.start_value, 0);
            else
                lv_obj_set_style_bg_opa(image, (lv_opa_t)anim->anim.fade.a_opa.start_value, 0);
            break;
        case EOS_ANIM_MOVE:
            if (!anim->cfg.move.disable_x)
                lv_obj_set_style_translate_x(image, anim->anim.move.a_x.start_value, 0);
            if (!anim->cfg.move.disable_y)
                lv_obj_set_style_translate_y(image, anim->anim.move.a_y.start_value, 0);
            break;
        case EOS_ANIM_TRANSFORM_SCALE:
            lv_obj_set_style_transform_scale(image, anim->anim.transform_scale.a_scale.start_value, 0);
            break;
        case EOS_ANIM_IMAGE_SCALE:
            lv_image_set_scale(image, anim->anim.image_scale.a_scale.start_value);
            break;
        case EOS_ANIM_RESIZE:
            if (!anim->cfg.resize.disable_w)
                lv_obj_set_width(image, anim->anim.resize.a_w.start_value);
            if (!anim->cfg.resize.disable_h)
                lv_obj_set_height(image, anim->anim.resize.a_h.start_value);
            break;
        default:
            break;
    }
}

static void _snapshot_sync_to_target(eos_anim_t *anim)
{
    lv_obj_t *target = anim->tar_obj;
    lv_obj_t *image = anim->snap_image;
    if (!target || !lv_obj_is_valid(target) || !image || !lv_obj_is_valid(image))
        return;

    switch (anim->type)
    {
        case EOS_ANIM_SCALE:
            lv_obj_set_width(target, lv_obj_get_width(image));
            lv_obj_set_height(target, lv_obj_get_height(image));
            break;
        case EOS_ANIM_FADE:
            if (anim->cfg.fade.main_opa)
                lv_obj_set_style_opa(target, lv_obj_get_style_opa(image, 0), 0);
            else if (anim->cfg.fade.layered)
                lv_obj_set_style_opa_layered(target, lv_obj_get_style_opa_layered(image, 0), 0);
            else
                lv_obj_set_style_bg_opa(target, lv_obj_get_style_bg_opa(image, 0), 0);
            break;
        case EOS_ANIM_MOVE:
            if (!anim->cfg.move.disable_x)
                lv_obj_set_style_translate_x(target, lv_obj_get_style_translate_x(image, 0), 0);
            if (!anim->cfg.move.disable_y)
                lv_obj_set_style_translate_y(target, lv_obj_get_style_translate_y(image, 0), 0);
            break;
        case EOS_ANIM_TRANSFORM_SCALE:
            lv_obj_set_style_transform_scale(target, lv_obj_get_style_transform_scale_x(image, 0), 0);
            break;
        case EOS_ANIM_IMAGE_SCALE:
            lv_image_set_scale(target, lv_image_get_scale(image));
            break;
        case EOS_ANIM_RESIZE:
            if (!anim->cfg.resize.disable_w)
                lv_obj_set_width(target, lv_obj_get_width(image));
            if (!anim->cfg.resize.disable_h)
                lv_obj_set_height(target, lv_obj_get_height(image));
            break;
        default:
            break;
    }
}

static bool _snapshot_backend_prepare(eos_anim_t *anim)
{
#if EOS_CONFIG_ANIM_SNAPSHOT_ENABLED
    lv_obj_t *target = anim->tar_obj;
    if (!target || !lv_obj_is_valid(target))
        return false;

    lv_obj_update_layout(target);

    int32_t w = lv_obj_get_width(target);
    int32_t h = lv_obj_get_height(target);
    if (w <= 0 || h <= 0)
        return false;

    /* LVGL snapshots include the object's extended draw area (for example,
     * shadows) in the raster buffer.  Allocate that area up front; otherwise
     * lv_snapshot_take_to_draw_buf() rejects the buffer and silently falls
     * back to direct animation. */
    uint32_t ext_size = (uint32_t)lv_obj_get_ext_draw_size(target);
    uint32_t snap_w = (uint32_t)w + ext_size * 2U;
    uint32_t snap_h = (uint32_t)h + ext_size * 2U;
    uint32_t snap_stride = lv_draw_buf_width_to_stride(snap_w, SNAP_COLOR_FORMAT);
    lv_draw_buf_t *buf = eos_draw_buf_create(snap_w, snap_h, SNAP_COLOR_FORMAT, snap_stride);
    if (!buf)
    {
        EOS_LOG_W("snapshot backend: eos_draw_buf_create(%u,%u) failed, fallback to direct",
                  (unsigned int)snap_w,
                  (unsigned int)snap_h);
        return false;
    }

    lv_result_t res = lv_snapshot_take_to_draw_buf(target, SNAP_COLOR_FORMAT, buf);
    if (res != LV_RESULT_OK)
    {
        EOS_LOG_W("snapshot backend: lv_snapshot_take_to_draw_buf failed for %p, fallback to direct", target);
        eos_draw_buf_destroy(buf);
        return false;
    }

    eos_activity_t *activity = eos_activity_from_widget(target);
    lv_obj_t *snap_ctr = activity ? eos_activity_get_snap_container(activity) : NULL;
    lv_obj_t *parent = snap_ctr ? snap_ctr : eos_overlay_get_snapshot_layer();
    lv_obj_t *image = lv_image_create(parent);
    lv_image_set_src(image, buf);
    lv_obj_set_size(image, (lv_coord_t)buf->header.w, (lv_coord_t)buf->header.h);

    lv_area_t area;
    lv_obj_get_coords(target, &area);
    lv_area_t parent_area;
    lv_obj_get_coords(parent, &parent_area);
    lv_obj_set_pos(image,
                   area.x1 - (lv_coord_t)ext_size - parent_area.x1,
                   area.y1 - (lv_coord_t)ext_size - parent_area.y1);

    lv_obj_set_style_transform_pivot_x(image,
                                       lv_obj_get_style_transform_pivot_x(target, 0) + (lv_coord_t)ext_size,
                                       0);
    lv_obj_set_style_transform_pivot_y(image,
                                       lv_obj_get_style_transform_pivot_y(target, 0) + (lv_coord_t)ext_size,
                                       0);

    _snapshot_apply_start_values(anim, image);

    if (anim->preserve_layout)
    {
        anim->saved_orig_opa = lv_obj_get_style_opa(target, 0);
    }

    if (_snap_batch_active)
    {
        if (_snap_batch_count < _SNAP_BATCH_MAX)
        {
            _snap_batch_entry_t *e = &_snap_batch_entries[_snap_batch_count++];
            e->target = target;
            e->image = image;
            e->preserve_layout = anim->preserve_layout;
            e->saved_opa = LV_OPA_COVER;
            EOS_LOG_I("[SNAP_BATCH] queued[%d] target=%p image=%p", _snap_batch_count - 1, target, image);
        }
        else
        {
            EOS_LOG_E("[SNAP_BATCH] batch full! fall through to immediate path for target=%p", target);
            _snapshot_present_once(image, "frame_A_before_hide");
            if (anim->preserve_layout)
            {
                lv_obj_set_style_opa(target, LV_OPA_TRANSP, 0);
            }
            else
            {
                lv_obj_add_flag(target, LV_OBJ_FLAG_HIDDEN);
            }
            _snapshot_present_once(image, "frame_C_after_hide");
        }
    }
    else
    {
        if (anim->preserve_layout)
        {
            lv_obj_set_style_opa(target, LV_OPA_TRANSP, 0);
        }
        else
        {
            lv_obj_add_flag(target, LV_OBJ_FLAG_HIDDEN);
        }

        _snapshot_present_once(image, "frame_after_hide");
    }

    anim->snap_buf = buf;
    anim->snap_image = image;

    switch (anim->type)
    {
        case EOS_ANIM_SCALE:
            lv_anim_set_var(&anim->anim.scale.a_width, image);
            lv_anim_set_var(&anim->anim.scale.a_height, image);
            break;
        case EOS_ANIM_FADE:
            lv_anim_set_var(&anim->anim.fade.a_opa, image);
            break;
        case EOS_ANIM_MOVE:
            if (!anim->cfg.move.disable_x)
                lv_anim_set_var(&anim->anim.move.a_x, image);
            if (!anim->cfg.move.disable_y)
                lv_anim_set_var(&anim->anim.move.a_y, image);
            break;
        case EOS_ANIM_TRANSFORM_SCALE:
            lv_anim_set_var(&anim->anim.transform_scale.a_scale, image);
            break;
        case EOS_ANIM_IMAGE_SCALE:
            lv_anim_set_var(&anim->anim.image_scale.a_scale, image);
            break;
        case EOS_ANIM_RESIZE:
            if (!anim->cfg.resize.disable_w)
                lv_anim_set_var(&anim->anim.resize.a_w, image);
            if (!anim->cfg.resize.disable_h)
                lv_anim_set_var(&anim->anim.resize.a_h, image);
            break;
    }

    return true;
#else
    LV_UNUSED(anim);
    return false;
#endif
}

/* Repeat / Playback helpers ----------------------------------*/

static void _apply_repeat_playback(lv_anim_t *a, eos_anim_t *anim)
{
    if (anim->repeat_count > 0)
        lv_anim_set_repeat_count(a, anim->repeat_count);
    if (anim->playback_time > 0)
        lv_anim_set_playback_time(a, anim->playback_time);
}

/* Animation Creation and Start -------------------------------*/

static void _apply_delay(lv_anim_t *a, eos_anim_t *anim)
{
    if (anim->delay > 0)
    {
        lv_anim_set_delay(a, anim->delay);
    }
}

static void _anim_init_common(eos_anim_t *anim, eos_anim type, lv_obj_t *tar_obj, uint32_t duration, bool auto_delete)
{
    anim->type = type;
    anim->anim_count = 0;
    anim->anim_completed_count = 0;
    anim->user_cb = NULL;
    anim->user_data = NULL;
    anim->auto_delete_obj = auto_delete;
    anim->tar_obj = tar_obj;
    anim->delay = 0;
    anim->group = NULL;
    anim->no_blocker = false;
    anim->preserve_layout = false;
    anim->saved_orig_opa = LV_OPA_COVER;
    anim->backend_type = EOS_ANIM_BACKEND_DIRECT;
    anim->snap_buf = NULL;
    anim->snap_image = NULL;
    anim->repeat_count = 0;
    anim->playback_time = 0;
    LV_UNUSED(duration);
    lv_memzero(&anim->cfg, sizeof(anim->cfg));
}

// Scale animation group
eos_anim_t *eos_anim_scale_create(lv_obj_t *tar_obj,
                                  int32_t w_start,
                                  int32_t w_end,
                                  int32_t h_start,
                                  int32_t h_end,
                                  uint32_t duration,
                                  bool auto_delete)
{
    if (!tar_obj || duration == 0)
        return NULL;

    eos_anim_t *anim = eos_malloc(sizeof(eos_anim_t));
    if (!anim)
        return NULL;

    _anim_init_common(anim, EOS_ANIM_SCALE, tar_obj, duration, auto_delete);

    _init_width_anim(&anim->anim.scale.a_width, tar_obj, w_start, w_end, duration, anim);
    anim->anim_count++;

    _init_height_anim(&anim->anim.scale.a_height, tar_obj, h_start, h_end, duration, anim);
    anim->anim_count++;

    EOS_LOG_I("Scale anim created: anim[%p] obj[%p]", anim, anim->tar_obj);
    return anim;
}

void eos_anim_scale_start(lv_obj_t *tar_obj,
                          int32_t w_start,
                          int32_t w_end,
                          int32_t h_start,
                          int32_t h_end,
                          uint32_t duration,
                          bool auto_delete)
{
    eos_anim_t *anim = eos_anim_scale_create(tar_obj, w_start, w_end, h_start, h_end, duration, auto_delete);
    if (!anim)
        return;
    if (!eos_anim_start(anim))
        eos_anim_del(anim);
}

// Transform scale animation group
eos_anim_t *eos_anim_transform_scale_create(lv_obj_t *tar_obj,
                                            int32_t scale_start,
                                            int32_t scale_end,
                                            uint32_t duration,
                                            bool auto_delete)
{
    if (!tar_obj || duration == 0)
        return NULL;

    eos_anim_t *anim = eos_malloc(sizeof(eos_anim_t));
    if (!anim)
        return NULL;

    _anim_init_common(anim, EOS_ANIM_TRANSFORM_SCALE, tar_obj, duration, auto_delete);

    _init_scale_anim(&anim->anim.transform_scale.a_scale, tar_obj, scale_start, scale_end, duration, anim);
    anim->anim_count++;

    EOS_LOG_I("Transform Scale anim created: anim[%p] obj[%p]", anim, anim->tar_obj);
    return anim;
}

void eos_anim_transform_scale_start(lv_obj_t *tar_obj,
                                    int32_t scale_start,
                                    int32_t scale_end,
                                    uint32_t duration,
                                    bool auto_delete)
{
    eos_anim_t *anim = eos_anim_transform_scale_create(tar_obj, scale_start, scale_end, duration, auto_delete);
    if (!anim)
        return;
    if (!eos_anim_start(anim))
        eos_anim_del(anim);
}

void eos_anim_transform_scale_start_ex(lv_obj_t *tar_obj,
                                       int32_t scale_start,
                                       int32_t scale_end,
                                       uint32_t duration,
                                       uint32_t playback_time,
                                       uint16_t repeat_count,
                                       bool auto_delete)
{
    if (!tar_obj)
        return;

    eos_anim_t *anim = eos_anim_transform_scale_create(tar_obj, scale_start, scale_end, duration, auto_delete);
    if (!anim)
        return;

    if (playback_time > 0)
        anim->playback_time = playback_time;
    if (repeat_count > 0)
        anim->repeat_count = repeat_count;

    if (!eos_anim_start(anim))
        eos_anim_del(anim);
}

// Move animation group
eos_anim_t *eos_anim_move_create(lv_obj_t *tar_obj,
                                 int32_t start_x,
                                 int32_t start_y,
                                 int32_t end_x,
                                 int32_t end_y,
                                 uint32_t duration,
                                 bool auto_delete)
{
    if (!tar_obj || duration == 0)
        return NULL;

    eos_anim_t *anim = eos_malloc(sizeof(eos_anim_t));
    EOS_LOG_D("MOVE alloc size(%d) ptr[%p]", sizeof(eos_anim_t), anim);
    if (!anim)
        return NULL;

    _anim_init_common(anim, EOS_ANIM_MOVE, tar_obj, duration, auto_delete);

    if (start_x == end_x)
    {
        anim->cfg.move.disable_x = true;
        EOS_LOG_E("MOVE_X DISABLED: start_x=%d end_x=%d", start_x, end_x);
    }
    else
    {
        _init_x_anim(&anim->anim.move.a_x, tar_obj, start_x, end_x, duration, anim);
        anim->anim_count++;
    }

    if (start_y == end_y)
    {
        anim->cfg.move.disable_y = true;
        EOS_LOG_E("MOVE_Y DISABLED: start_y=%d end_y=%d", start_y, end_y);
    }
    else
    {
        _init_y_anim(&anim->anim.move.a_y, tar_obj, start_y, end_y, duration, anim);
        anim->anim_count++;
    }

    if (anim->anim_count == 0)
    {
        eos_free(anim);
        return NULL;
    }

    EOS_LOG_I("Move anim created: anim[%p] obj[%p]", anim, anim->tar_obj);
    return anim;
}

void eos_anim_move_start(lv_obj_t *tar_obj,
                         int32_t start_x,
                         int32_t start_y,
                         int32_t end_x,
                         int32_t end_y,
                         uint32_t duration,
                         bool auto_delete)
{
    eos_anim_t *anim = eos_anim_move_create(tar_obj, start_x, start_y, end_x, end_y, duration, auto_delete);
    if (!anim)
        return;
    if (!eos_anim_start(anim))
    {
        EOS_LOG_D("Delete anim: %p", anim);
        eos_anim_del(anim);
    }
}

// Opacity animation group
eos_anim_t *eos_anim_fade_create(lv_obj_t *tar_obj,
                                 int32_t opa_start,
                                 int32_t opa_end,
                                 uint32_t duration,
                                 bool auto_delete)
{
    if (!tar_obj || duration == 0)
        return NULL;

    eos_anim_t *anim = eos_malloc(sizeof(eos_anim_t));
    EOS_LOG_D("FADE alloc size(%d) ptr[%p]", sizeof(eos_anim_t), anim);
    if (!anim)
        return NULL;

    _anim_init_common(anim, EOS_ANIM_FADE, tar_obj, duration, auto_delete);

    anim->cfg.fade.layered = true;
    anim->cfg.fade.main_opa = false;

    _init_opa_anim(&anim->anim.fade.a_opa, tar_obj, opa_start, opa_end, duration, anim);
    anim->anim_count++;
    lv_anim_set_user_data(&anim->anim.fade.a_opa, anim);

    EOS_LOG_I("Fade anim created: anim[%p] obj[%p]", anim, anim->tar_obj);
    return anim;
}

void eos_anim_fade_start(lv_obj_t *tar_obj, int32_t opa_start, int32_t opa_end, uint32_t duration, bool auto_delete)
{
    eos_anim_t *anim = eos_anim_fade_create(tar_obj, opa_start, opa_end, duration, auto_delete);
    if (!anim)
        return;
    if (!eos_anim_start(anim))
        eos_anim_del(anim);
}

void eos_anim_fade_set_layered(eos_anim_t *a, bool layered)
{
    EOS_CHECK_PTR_RETURN(a);
    if (a->type == EOS_ANIM_FADE)
    {
        a->cfg.fade.layered = layered;
        _select_opa_exec_cb(&a->anim.fade.a_opa, a);
    }
}

void eos_anim_fade_set_main_opa(eos_anim_t *a, bool enabled)
{
    EOS_CHECK_PTR_RETURN(a);
    if (a->type == EOS_ANIM_FADE)
    {
        a->cfg.fade.main_opa = enabled;
        _select_opa_exec_cb(&a->anim.fade.a_opa, a);
    }
}

// Image scale animation
eos_anim_t *eos_anim_image_scale_create(lv_obj_t *tar_obj,
                                        int32_t scale_start,
                                        int32_t scale_end,
                                        uint32_t duration,
                                        bool auto_delete)
{
    if (!tar_obj || duration == 0)
        return NULL;

    eos_anim_t *anim = eos_malloc(sizeof(eos_anim_t));
    if (!anim)
        return NULL;

    _anim_init_common(anim, EOS_ANIM_IMAGE_SCALE, tar_obj, duration, auto_delete);

    _init_image_scale_anim(&anim->anim.image_scale.a_scale, tar_obj, scale_start, scale_end, duration, anim);
    anim->anim_count++;

    EOS_LOG_I("Image Scale anim created: anim[%p] obj[%p]", anim, anim->tar_obj);
    return anim;
}

void eos_anim_image_scale_start(lv_obj_t *tar_obj,
                                int32_t scale_start,
                                int32_t scale_end,
                                uint32_t duration,
                                bool auto_delete)
{
    eos_anim_t *anim = eos_anim_image_scale_create(tar_obj, scale_start, scale_end, duration, auto_delete);
    if (!anim)
        return;
    if (!eos_anim_start(anim))
        eos_anim_del(anim);
}

// Resize animation (independent width/height)
eos_anim_t *eos_anim_resize_create(lv_obj_t *tar_obj,
                                   int32_t w_start,
                                   int32_t w_end,
                                   int32_t h_start,
                                   int32_t h_end,
                                   uint32_t duration,
                                   bool auto_delete)
{
    if (!tar_obj || duration == 0)
        return NULL;

    eos_anim_t *anim = eos_malloc(sizeof(eos_anim_t));
    if (!anim)
        return NULL;

    _anim_init_common(anim, EOS_ANIM_RESIZE, tar_obj, duration, auto_delete);

    if (w_start == w_end)
    {
        anim->cfg.resize.disable_w = true;
    }
    else
    {
        _init_width_anim(&anim->anim.resize.a_w, tar_obj, w_start, w_end, duration, anim);
        anim->anim_count++;
    }

    if (h_start == h_end)
    {
        anim->cfg.resize.disable_h = true;
    }
    else
    {
        _init_height_anim(&anim->anim.resize.a_h, tar_obj, h_start, h_end, duration, anim);
        anim->anim_count++;
    }

    if (anim->anim_count == 0)
    {
        eos_free(anim);
        return NULL;
    }

    EOS_LOG_I("Resize anim created: anim[%p] obj[%p]", anim, anim->tar_obj);
    return anim;
}

void eos_anim_resize_start(lv_obj_t *tar_obj,
                           int32_t w_start,
                           int32_t w_end,
                           int32_t h_start,
                           int32_t h_end,
                           uint32_t duration,
                           bool auto_delete)
{
    eos_anim_t *anim = eos_anim_resize_create(tar_obj, w_start, w_end, h_start, h_end, duration, auto_delete);
    if (!anim)
        return;
    if (!eos_anim_start(anim))
        eos_anim_del(anim);
}

/* Debug Intercept --------------------------------------------*/

void eos_anim_set_intercept_cb(eos_anim_intercept_cb_t cb)
{
    _eos_anim_intercept_cb = cb;
}

/* Animation Start --------------------------------------------*/

bool eos_anim_start(eos_anim_t *anim)
{
    if (!anim)
        return false;

    if (anim->backend_type == EOS_ANIM_BACKEND_SNAPSHOT)
    {
#if EOS_CONFIG_ANIM_SNAPSHOT_ENABLED
        if (!_snapshot_backend_prepare(anim))
        {
            EOS_LOG_W("Snapshot backend prepare failed, falling back to direct for anim[%p] obj[%p]",
                      anim,
                      anim->tar_obj);
            anim->backend_type = EOS_ANIM_BACKEND_DIRECT;
        }
#else
        EOS_LOG_I("Snapshot backend disabled, using direct animation for anim[%p] obj[%p]",
                  anim,
                  anim->tar_obj);
        /* preserve_layout is meaningful to the snapshot path because the
         * original object is temporarily made transparent.  The direct path
         * never hides the object, but it still runs the common completion
         * callback; capture the real opacity so completion cannot overwrite
         * it with the constructor default. */
        if (anim->preserve_layout && anim->tar_obj && lv_obj_is_valid(anim->tar_obj))
        {
            anim->saved_orig_opa = lv_obj_get_style_opa(anim->tar_obj, 0);
        }
        anim->backend_type = EOS_ANIM_BACKEND_DIRECT;
#endif
    }

    if (!anim->no_blocker)
    {
        eos_anim_blocker_show();
    }

    /* Debug intercept — allow simulator-side tools to inspect/modify
     * animation parameters before the underlying lv_anim_t starts. */
    if (_eos_anim_intercept_cb)
    {
        _eos_anim_intercept_cb(anim);
    }

    switch (anim->type)
    {
        case EOS_ANIM_SCALE:
            _apply_delay(&anim->anim.scale.a_width, anim);
            _apply_delay(&anim->anim.scale.a_height, anim);
            _apply_repeat_playback(&anim->anim.scale.a_width, anim);
            _apply_repeat_playback(&anim->anim.scale.a_height, anim);
            lv_anim_start(&anim->anim.scale.a_width);
            lv_anim_start(&anim->anim.scale.a_height);
            break;
        case EOS_ANIM_FADE:
            _apply_delay(&anim->anim.fade.a_opa, anim);
            _apply_repeat_playback(&anim->anim.fade.a_opa, anim);
            lv_anim_start(&anim->anim.fade.a_opa);
            break;
        case EOS_ANIM_MOVE:
            EOS_LOG_E("MOVE start: disable_x=%d disable_y=%d anim_count=%d",
                      anim->cfg.move.disable_x,
                      anim->cfg.move.disable_y,
                      anim->anim_count);
            if (!anim->cfg.move.disable_x)
            {
                _apply_delay(&anim->anim.move.a_x, anim);
                _apply_repeat_playback(&anim->anim.move.a_x, anim);
                lv_anim_t *lv_anim_x = lv_anim_start(&anim->anim.move.a_x);
                EOS_LOG_E("MOVE a_x lv_anim_start=%p", lv_anim_x);
            }
            if (!anim->cfg.move.disable_y)
            {
                _apply_delay(&anim->anim.move.a_y, anim);
                _apply_repeat_playback(&anim->anim.move.a_y, anim);
                lv_anim_t *lv_anim_y = lv_anim_start(&anim->anim.move.a_y);
                EOS_LOG_E("MOVE a_y lv_anim_start=%p", lv_anim_y);
            }
            break;
        case EOS_ANIM_TRANSFORM_SCALE:
            _apply_delay(&anim->anim.transform_scale.a_scale, anim);
            _apply_repeat_playback(&anim->anim.transform_scale.a_scale, anim);
            lv_anim_start(&anim->anim.transform_scale.a_scale);
            break;
        case EOS_ANIM_IMAGE_SCALE:
            _apply_delay(&anim->anim.image_scale.a_scale, anim);
            _apply_repeat_playback(&anim->anim.image_scale.a_scale, anim);
            lv_anim_start(&anim->anim.image_scale.a_scale);
            break;
        case EOS_ANIM_RESIZE:
            if (!anim->cfg.resize.disable_w)
            {
                _apply_delay(&anim->anim.resize.a_w, anim);
                _apply_repeat_playback(&anim->anim.resize.a_w, anim);
                lv_anim_start(&anim->anim.resize.a_w);
            }
            if (!anim->cfg.resize.disable_h)
            {
                _apply_delay(&anim->anim.resize.a_h, anim);
                _apply_repeat_playback(&anim->anim.resize.a_h, anim);
                lv_anim_start(&anim->anim.resize.a_h);
            }
            break;
        default:
            EOS_LOG_E("Anim[%p] start FAILED: unknown type=%d", anim, anim->type);
            return false;
    }

    return true;
}
