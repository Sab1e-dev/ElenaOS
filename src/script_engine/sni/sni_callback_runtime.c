/**
 * @file sni_callback_runtime.c
 * @brief SNI callback runtime
 */

#include "sni_callback_runtime.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "SNI Callback Runtime"
#include "eos_mem.h"
#include "eos_log.h"
#include "sni_api_export.h"
#include "sni_type_bridge.h"
#include "sni_types.h"
#include "sni_context.h"
#include "script_engine_core.h"
#include "spm.h"
#include "eos_dispatcher.h"
#include "lvgl/src/misc/lv_timer_private.h"

/* Macros and Definitions -------------------------------------*/

/*
 * Guards to prevent sni_context_sweep_all() from deleting the native
 * LVGL timer / animation that is currently executing its JS callback.
 * Deletion would corrupt LVGL's internal iteration and cause EXC_BAD_ACCESS.
 */
static lv_timer_t *s_dispatching_timer = NULL;
static sni_anim_callback_ctx_t *s_dispatching_anim_ctx = NULL;

bool sni_cb_is_dispatching_timer(lv_timer_t *t)
{
    return s_dispatching_timer != NULL && s_dispatching_timer == t;
}

bool sni_cb_is_dispatching_anim(sni_anim_callback_ctx_t *ctx)
{
    return s_dispatching_anim_ctx != NULL && s_dispatching_anim_ctx == ctx;
}

static inline void sni_cb_safe_jerry_value_free(sni_context_t *owner_ctx, jerry_value_t *value)
{
    if (!value)
        return;

    if (jerry_value_is_undefined(*value) || jerry_value_is_null(*value))
    {
        return;
    }

    /* Key off teardown_phase when we have context, not global engine state.
     * During teardown Phases 0-2 the engine is IDLE but the JS heap is
     * still alive — jerry_value_free MUST be called.  Only skip when the
     * engine has actually been stopped (Phase >= ENGINE_STOPPED). */
    if (owner_ctx)
    {
        if (owner_ctx->teardown_phase >= SNI_TEARDOWN_PHASE_ENGINE_STOPPED)
        {
            *value = jerry_undefined();
            return;
        }
        jerry_value_free(*value);
        *value = jerry_undefined();
        return;
    }

    /* No context (runtime path) — only free if engine is RUNNING. */
    script_engine_state_t state = script_engine_get_state();
    if (state == SCRIPT_ENGINE_STATE_UNINITIALIZED || state == SCRIPT_ENGINE_STATE_IDLE)
    {
        *value = jerry_undefined();
        return;
    }

    jerry_value_free(*value);
    *value = jerry_undefined();
}

/**
 * @brief Detect whether a fatal engine recovery happened during spm_call
 *
 * After spm_call() returns, callers MUST invoke this before accessing any
 * local jerry_value_t variables or native pointers (t, ctx, owner_ctx).
 * If the engine generation changed, jerry_init() already wiped the heap
 * and all jerry_value_t handles acquired before the call are invalid.
 *
 * @param saved_gen  script_engine_get_gen() value captured before spm_call
 * @param owner_ctx  SNI context pointer (may be dangling after recovery)
 * @return true if recovery destroyed the context — caller must return immediately
 */
static bool sni_cb_detect_recovery(uint32_t saved_gen, sni_context_t *owner_ctx)
{
    /* Engine generation changed → jerry_init() wiped the heap */
    if (saved_gen != script_engine_get_gen())
        return true;

    /* Context / program destroyed */
    if (!owner_ctx || !owner_ctx->owner)
        return true;

    /* Program is no longer active */
    if (owner_ctx->owner->state != SCRIPT_PROGRAM_STATE_ACTIVE)
        return true;

    return false;
}

typedef struct sni_event_callback_ctx
{
    lv_obj_t *owner;
    lv_event_dsc_t *dsc;
    jerry_value_t js_cb;
    jerry_value_t js_user_data;
    sni_context_t *owner_ctx;
    bool alive;
    uint32_t engine_gen; /**< Engine generation at creation time, used to detect stale contexts after recovery */
    struct sni_event_callback_ctx *next;
} sni_event_callback_ctx_t;

/* Function Implementations -----------------------------------*/

static sni_event_callback_ctx_t **sni_cb_event_list_ptr(sni_context_t *ctx)
{
    return (sni_event_callback_ctx_t **)&ctx->event_ctx_list;
}

static bool sni_cb_js_strict_equal(jerry_value_t lhs, jerry_value_t rhs)
{
    jerry_value_t eq = jerry_binary_op(JERRY_BIN_OP_STRICT_EQUAL, lhs, rhs);
    if (jerry_value_is_exception(eq))
    {
        jerry_value_free(eq);
        return false;
    }

    bool result = jerry_value_is_true(eq);
    jerry_value_free(eq);
    return result;
}

static void sni_cb_event_prepare_js_event(jerry_value_t event_obj, lv_event_t *e, jerry_value_t js_user_data)
{
    jerry_value_t flag = jerry_object_get_sz(event_obj, "__inited");
    if (jerry_value_is_boolean(flag))
    {
        jerry_value_free(flag);
        return;
    }
    jerry_value_free(flag);

    jerry_value_t inited = jerry_boolean(true);
    jerry_value_free(jerry_object_set_sz(event_obj, "__inited", inited));
    jerry_value_free(inited);

    lv_obj_t *target_ptr = lv_event_get_target(e);
    jerry_value_t js_target = sni_tb_c2js(&target_ptr, SNI_H_LV_OBJ);
    jerry_value_free(jerry_object_set_sz(event_obj, "target", js_target));
    jerry_value_free(js_target);

    jerry_value_t js_type = jerry_number((double)lv_event_get_code(e));
    jerry_value_free(jerry_object_set_sz(event_obj, "type", js_type));
    jerry_value_free(js_type);

    jerry_value_t copied_user_data = jerry_value_copy(js_user_data);
    jerry_value_free(jerry_object_set_sz(event_obj, "user_data", copied_user_data));
    jerry_value_free(copied_user_data);
}

static void sni_cb_event_free_ctx(sni_event_callback_ctx_t *ctx)
{
    if (!ctx || !ctx->alive)
    {
        return;
    }

    ctx->alive = false;

    if (ctx->owner_ctx)
    {
        sni_event_callback_ctx_t **pp = sni_cb_event_list_ptr(ctx->owner_ctx);
        while (*pp)
        {
            if (*pp == ctx)
            {
                *pp = ctx->next;
                break;
            }
            pp = &(*pp)->next;
        }
    }

    sni_cb_safe_jerry_value_free(NULL, &ctx->js_cb);
    sni_cb_safe_jerry_value_free(NULL, &ctx->js_user_data);

    eos_free(ctx);
}

static void sni_cb_event_dispatch(lv_event_t *e)
{
    sni_event_callback_ctx_t *ctx = (sni_event_callback_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->alive)
    {
        return;
    }

    if (lv_event_get_code(e) == LV_EVENT_DELETE)
    {
        sni_cb_event_free_ctx(ctx);
        return;
    }

    if (sni_context_is_paused(ctx->owner_ctx))
    {
        return;
    }

    if (!ctx->owner_ctx || !ctx->owner_ctx->owner)
    {
        return;
    }

    if (ctx->owner_ctx->owner->state != SCRIPT_PROGRAM_STATE_ACTIVE)
    {
        return;
    }

    /* Capture engine generation BEFORE spm_call so we can detect
     * fatal recovery / re-entrant destruction after the call returns. */
    uint32_t saved_gen = script_engine_get_gen();

    lv_event_t *event_ptr = e;
    jerry_value_t event_obj = sni_tb_c2js(&event_ptr, SNI_H_LV_EVENT);
    sni_cb_event_prepare_js_event(event_obj, e, ctx->js_user_data);

    jerry_value_t args[1] = {event_obj};
    jerry_value_t ret = spm_call(ctx->owner_ctx->owner, ctx->js_cb, jerry_undefined(), args, 1);

    /* If a fatal recovery happened inside spm_call, return immediately
     * — all local jerry_value_t variables belong to the old heap. */
    if (sni_cb_detect_recovery(saved_gen, ctx->owner_ctx))
    {
        return;
    }

    if (jerry_value_is_error(ret) || jerry_value_is_exception(ret))
    {
        EOS_LOG_E("Event callback encounter an error");
    }

    jerry_value_free(ret);
    jerry_value_free(event_obj);
}

void sni_cb_event_cleanup_descriptor(lv_event_dsc_t *dsc)
{
    if (!dsc)
    {
        return;
    }

    sni_event_callback_ctx_t *ctx = (sni_event_callback_ctx_t *)lv_event_dsc_get_user_data(dsc);
    if (!ctx)
    {
        return;
    }

    sni_cb_event_free_ctx(ctx);
}

static void sni_cb_event_dsc_destroy_cb(void *native_ptr)
{
    lv_event_dsc_t *dsc = (lv_event_dsc_t *)native_ptr;
    sni_cb_event_cleanup_descriptor(dsc);
}

static void sni_cb_style_handle_destroy_cb(void *native_ptr)
{
    lv_style_t *style = (lv_style_t *)native_ptr;
    lv_style_reset(style);
}

static void sni_cb_font_handle_destroy_cb(void *native_ptr)
{
    (void)native_ptr;
}

void sni_cb_runtime_init(void)
{
    sni_tb_register_handle_destroy_cb(SNI_H_LV_EVENT_DSC, sni_cb_event_dsc_destroy_cb);
    sni_tb_register_handle_destroy_cb(SNI_H_LV_STYLE, sni_cb_style_handle_destroy_cb);
    sni_tb_register_handle_destroy_cb(SNI_H_LV_FONT, sni_cb_font_handle_destroy_cb);
}

/* Timer Callback Implementation ------------------------------*/

static void sni_cb_timer_dispatch(lv_timer_t *t)
{
    sni_timer_callback_ctx_t *ctx = (sni_timer_callback_ctx_t *)lv_timer_get_user_data(t);
    if (!ctx || ctx->state != SNI_TIMER_STATE_ACTIVE)
    {
        return;
    }

    if (sni_context_is_paused(ctx->owner_ctx))
    {
        return;
    }

    if (!ctx->owner_ctx || !ctx->owner_ctx->owner)
    {
        EOS_LOG_W("Timer dispatch skipped: owner context or program is NULL");
        return;
    }

    if (ctx->owner_ctx->owner->state != SCRIPT_PROGRAM_STATE_ACTIVE)
    {
        EOS_LOG_W("Timer dispatch skipped: program not active (state=%d)", ctx->owner_ctx->owner->state);
        return;
    }

    /* Capture engine generation BEFORE spm_call so we can detect
     * fatal recovery / re-entrant destruction after the call returns. */
    uint32_t saved_gen = script_engine_get_gen();

    lv_timer_t *timer_ptr = t;
    jerry_value_t js_timer = sni_tb_c2js(&timer_ptr, SNI_H_LV_TIMER);
    jerry_value_t args[1] = {js_timer};

    /* Guard: prevent sni_context_sweep_all from deleting this timer
     * while we are inside its callback (would corrupt LVGL internals). */
    s_dispatching_timer = t;
    jerry_value_t ret = spm_call(ctx->owner_ctx->owner, ctx->js_cb, jerry_undefined(), args, 1);
    s_dispatching_timer = NULL;

    /* If a fatal recovery happened inside spm_call, the engine was
     * reset (jerry_init), SNI context and callback ctx were freed,
     * and all local jerry_value_t variables belong to the old heap.
     * Return immediately — do NOT touch t, ctx, js_timer, or ret. */
    if (sni_cb_detect_recovery(saved_gen, ctx->owner_ctx))
    {
        return;
    }

    if (jerry_value_is_error(ret) || jerry_value_is_exception(ret))
    {
        EOS_LOG_E("Timer callback encountered an error");
    }

    jerry_value_free(ret);
    jerry_value_free(js_timer);

    /* If the JS callback called timer.delete() on itself, sni_context_
     * delete_timer_sync() marked the context PENDING_DELETE and deferred
     * the actual free.  Now that spm_call has returned and s_dispatching_
     * timer is NULL, it's safe to finish the deletion. */
    if (ctx->state == SNI_TIMER_STATE_PENDING_DELETE)
    {
        sni_context_delete_timer_sync(ctx->owner_ctx, t);
        return;
    }

    if (t->repeat_count <= 0 && ctx->auto_delete && ctx->state == SNI_TIMER_STATE_ACTIVE)
    {
        sni_context_request_async_delete_timer(ctx->owner_ctx, t);
    }
}

/* Event Callback Implementation ------------------------------*/

bool sni_cb_event_add(lv_obj_t *obj,
                      jerry_value_t js_cb,
                      lv_event_code_t filter,
                      jerry_value_t js_user_data,
                      lv_event_dsc_t **out_dsc)
{
    if (!obj || !out_dsc || !jerry_value_is_function(js_cb))
    {
        return false;
    }

    sni_event_callback_ctx_t *ctx = eos_malloc_zeroed(sizeof(sni_event_callback_ctx_t));
    if (!ctx)
    {
        return false;
    }

    ctx->owner = obj;
    ctx->js_cb = jerry_value_copy(js_cb);
    ctx->js_user_data = jerry_value_copy(js_user_data);
    ctx->owner_ctx = sni_cb_get_context();
    ctx->alive = true;
    ctx->engine_gen = script_engine_get_gen();

    lv_event_dsc_t *dsc = lv_obj_add_event_cb(obj, sni_cb_event_dispatch, filter, ctx);
    if (!dsc)
    {
        jerry_value_free(ctx->js_cb);
        jerry_value_free(ctx->js_user_data);
        eos_free(ctx);
        return false;
    }

    ctx->dsc = dsc;
    {
        sni_event_callback_ctx_t **head = sni_cb_event_list_ptr(ctx->owner_ctx);
        ctx->next = *head;
        *head = ctx;
    }

    *out_dsc = dsc;
    return true;
}

bool sni_cb_event_remove_dsc(lv_obj_t *obj, lv_event_dsc_t *dsc)
{
    if (!obj || !dsc)
    {
        return false;
    }

    sni_cb_event_cleanup_descriptor(dsc);
    bool removed = lv_obj_remove_event_dsc(obj, dsc);
    return removed;
}

bool sni_cb_event_remove_by_js_cb(lv_obj_t *obj, jerry_value_t js_cb)
{
    if (!obj || !jerry_value_is_function(js_cb))
    {
        return false;
    }

    bool removed_any = false;
    sni_event_callback_ctx_t *ctx = *sni_cb_event_list_ptr(sni_cb_get_context());

    while (ctx)
    {
        sni_event_callback_ctx_t *next = ctx->next;

        if (ctx->owner != obj)
        {
            ctx = next;
            continue;
        }

        if (!sni_cb_js_strict_equal(ctx->js_cb, js_cb))
        {
            ctx = next;
            continue;
        }

        lv_obj_remove_event_dsc(obj, ctx->dsc);
        ctx->dsc = NULL;
        sni_cb_event_free_ctx(ctx);
        removed_any = true;
        ctx = next;
    }

    return removed_any;
}

uint32_t sni_cb_event_remove_by_js_cb_user_data(lv_obj_t *obj, jerry_value_t js_cb, jerry_value_t js_user_data)
{
    if (!obj || !jerry_value_is_function(js_cb))
    {
        return 0;
    }

    uint32_t removed = 0;
    sni_event_callback_ctx_t *ctx = *sni_cb_event_list_ptr(sni_cb_get_context());

    while (ctx)
    {
        sni_event_callback_ctx_t *next = ctx->next;

        if (ctx->owner != obj)
        {
            ctx = next;
            continue;
        }

        if (!sni_cb_js_strict_equal(ctx->js_cb, js_cb))
        {
            ctx = next;
            continue;
        }

        if (!sni_cb_js_strict_equal(ctx->js_user_data, js_user_data))
        {
            ctx = next;
            continue;
        }

        lv_obj_remove_event_dsc(obj, ctx->dsc);
        ctx->dsc = NULL;
        sni_cb_event_free_ctx(ctx);
        removed++;
        ctx = next;
    }

    return removed;
}

bool sni_cb_timer_create(jerry_value_t js_cb, uint32_t period, lv_timer_t **out_timer)
{
    if (!jerry_value_is_function(js_cb) || !out_timer)
    {
        return false;
    }

    sni_timer_callback_ctx_t *ctx = eos_malloc_zeroed(sizeof(sni_timer_callback_ctx_t));
    if (!ctx)
    {
        return false;
    }

    ctx->js_cb = jerry_value_copy(js_cb);
    ctx->owner_ctx = sni_cb_get_context();
    ctx->state = SNI_TIMER_STATE_ACTIVE;
    ctx->auto_delete = true;
    ctx->engine_gen = script_engine_get_gen();

    lv_timer_t *timer = lv_timer_create(sni_cb_timer_dispatch, period, ctx);
    if (!timer)
    {
        sni_cb_safe_jerry_value_free(NULL, &ctx->js_cb);
        eos_free(ctx);
        return false;
    }

    lv_timer_set_auto_delete(timer, false);

    ctx->timer = timer;

    *out_timer = timer;
    return true;
}

bool sni_cb_timer_set_cb(lv_timer_t *timer, jerry_value_t js_cb)
{
    if (!timer || !jerry_value_is_function(js_cb))
    {
        return false;
    }

    sni_timer_callback_ctx_t *ctx = (sni_timer_callback_ctx_t *)lv_timer_get_user_data(timer);
    if (!ctx || ctx->state != SNI_TIMER_STATE_ACTIVE)
    {
        return false;
    }

    if (!jerry_value_is_undefined(ctx->js_cb) && !jerry_value_is_null(ctx->js_cb))
    {
        sni_cb_safe_jerry_value_free(NULL, &ctx->js_cb);
    }

    ctx->js_cb = jerry_value_copy(js_cb);

    /* Ensure the freed old callback is fully collected before returning
       to JS, preventing JerryScript internal reference entanglement
       between the released closure and any concurrently held callbacks
       (e.g. animation custom_exec callbacks from the same module). */
    jerry_heap_gc(JERRY_GC_PRESSURE_LOW);

    return true;
}

bool sni_cb_timer_set_auto_delete(lv_timer_t *timer, bool auto_delete)
{
    if (!timer)
    {
        return false;
    }

    sni_timer_callback_ctx_t *ctx = (sni_timer_callback_ctx_t *)lv_timer_get_user_data(timer);
    if (!ctx || ctx->state != SNI_TIMER_STATE_ACTIVE)
    {
        return false;
    }

    ctx->auto_delete = auto_delete;
    return true;
}

/* Anim Callback Implementation -------------------------------*/

static lv_anim_path_cb_t s_anim_path_table[SNI_ANIM_PATH_ENUM_MAX] = {
    NULL,
    lv_anim_path_linear,
    lv_anim_path_ease_in,
    lv_anim_path_ease_out,
    lv_anim_path_ease_in_out,
    lv_anim_path_overshoot,
    lv_anim_path_bounce,
    lv_anim_path_step,
    lv_anim_path_custom_bezier3,
};

static bool sni_cb_anim_has_js_cb(sni_anim_callback_ctx_t *ctx, sni_anim_cb_slot_t slot)
{
    return ctx && ctx->state == SNI_ANIM_STATE_ACTIVE && jerry_value_is_function(ctx->cb_slots[slot]);
}

static jerry_value_t sni_cb_anim_make_js_anim(sni_anim_callback_ctx_t *ctx)
{
    sni_anim_callback_ctx_t *ctx_ptr = ctx;
    return sni_tb_c2js(&ctx_ptr, SNI_H_LV_ANIM);
}

static void sni_cb_anim_clear_slot(sni_anim_callback_ctx_t *ctx, sni_anim_cb_slot_t slot)
{
    if (!ctx || ctx->state == SNI_ANIM_STATE_DELETED)
    {
        return;
    }

    sni_cb_safe_jerry_value_free(NULL, &ctx->cb_slots[slot]);
}

static bool sni_cb_anim_store_slot(sni_anim_callback_ctx_t *ctx, sni_anim_cb_slot_t slot, jerry_value_t js_cb)
{
    if (!ctx || !jerry_value_is_function(js_cb))
    {
        return false;
    }

    if (ctx->state != SNI_ANIM_STATE_ACTIVE)
    {
        return false;
    }

    sni_cb_anim_clear_slot(ctx, slot);
    ctx->cb_slots[slot] = jerry_value_copy(js_cb);
    return true;
}

static void sni_cb_anim_call_void_slot(sni_anim_callback_ctx_t *ctx, sni_anim_cb_slot_t slot)
{
    if (!sni_cb_anim_has_js_cb(ctx, slot))
    {
        return;
    }

    if (sni_context_is_paused(ctx->owner_ctx))
    {
        return;
    }

    if (!ctx->owner_ctx || !ctx->owner_ctx->owner)
    {
        EOS_LOG_W("Anim dispatch skipped: owner context or program is NULL");
        return;
    }

    if (ctx->owner_ctx->owner->state != SCRIPT_PROGRAM_STATE_ACTIVE)
    {
        EOS_LOG_W("Anim dispatch skipped: program not active (state=%d)", ctx->owner_ctx->owner->state);
        return;
    }

    /* Capture engine generation BEFORE spm_call to detect re-entrant recovery. */
    uint32_t saved_gen = script_engine_get_gen();

    jerry_value_t js_anim = sni_cb_anim_make_js_anim(ctx);
    jerry_value_t args[1] = {js_anim};

    /* Guard: prevent sni_context_sweep_all from deleting this anim ctx
     * while we are inside its callback (would cause use-after-free). */
    s_dispatching_anim_ctx = ctx;
    jerry_value_t ret = spm_call(ctx->owner_ctx->owner, ctx->cb_slots[slot], jerry_undefined(), args, 1);
    s_dispatching_anim_ctx = NULL;

    if (sni_cb_detect_recovery(saved_gen, ctx->owner_ctx))
    {
        return;
    }

    if (jerry_value_is_error(ret) || jerry_value_is_exception(ret))
    {
        EOS_LOG_E("Anim callback encountered an error");
    }

    jerry_value_free(ret);
    jerry_value_free(js_anim);
}

static void sni_cb_anim_custom_exec_dispatch(lv_anim_t *var, int32_t value)
{
    sni_anim_callback_ctx_t *ctx = (sni_anim_callback_ctx_t *)lv_anim_get_user_data(var);
    if (!sni_cb_anim_has_js_cb(ctx, SNI_ANIM_CB_SLOT_CUSTOM_EXEC))
    {
        return;
    }

    if (sni_context_is_paused(ctx->owner_ctx))
    {
        return;
    }

    if (!ctx->owner_ctx || !ctx->owner_ctx->owner)
    {
        return;
    }

    if (ctx->owner_ctx->owner->state != SCRIPT_PROGRAM_STATE_ACTIVE)
    {
        EOS_LOG_W("Anim custom exec dispatch skipped: program not active (state=%d)", ctx->owner_ctx->owner->state);
        return;
    }

    /* Capture engine generation BEFORE spm_call to detect re-entrant recovery. */
    uint32_t saved_gen = script_engine_get_gen();

    jerry_value_t js_anim = sni_cb_anim_make_js_anim(ctx);
    jerry_value_t js_value = sni_tb_c2js(&value, SNI_T_INT32);
    jerry_value_t args[2] = {js_anim, js_value};

    s_dispatching_anim_ctx = ctx;
    jerry_value_t ret =
        spm_call(ctx->owner_ctx->owner, ctx->cb_slots[SNI_ANIM_CB_SLOT_CUSTOM_EXEC], jerry_undefined(), args, 2);
    s_dispatching_anim_ctx = NULL;

    if (sni_cb_detect_recovery(saved_gen, ctx->owner_ctx))
    {
        return;
    }

    if (jerry_value_is_error(ret) || jerry_value_is_exception(ret))
    {
        EOS_LOG_E("Anim custom_exec callback encountered an error");
    }

    jerry_value_free(ret);
    jerry_value_free(js_value);
    jerry_value_free(js_anim);
}

static void sni_cb_anim_start_dispatch(lv_anim_t *a)
{
    sni_anim_callback_ctx_t *ctx = (sni_anim_callback_ctx_t *)lv_anim_get_user_data(a);
    sni_cb_anim_call_void_slot(ctx, SNI_ANIM_CB_SLOT_START);
}

static void sni_cb_anim_completed_dispatch(lv_anim_t *a)
{
    sni_anim_callback_ctx_t *ctx = (sni_anim_callback_ctx_t *)lv_anim_get_user_data(a);
    sni_cb_anim_call_void_slot(ctx, SNI_ANIM_CB_SLOT_COMPLETED);
}

static void sni_cb_anim_deleted_dispatch(lv_anim_t *a)
{
    sni_anim_callback_ctx_t *ctx = (sni_anim_callback_ctx_t *)lv_anim_get_user_data(a);
    if (!ctx)
    {
        return;
    }

    if (ctx->state != SNI_ANIM_STATE_ACTIVE)
    {
        return;
    }

    if (!ctx->owner_ctx || sni_context_is_paused(ctx->owner_ctx))
    {
        return;
    }

    lv_anim_set_user_data(a, NULL);
    ctx->active_anim = NULL;

    sni_context_request_async_delete_anim(ctx->owner_ctx, ctx);
}

static int32_t sni_cb_anim_call_int_slot(sni_anim_callback_ctx_t *ctx, sni_anim_cb_slot_t slot, int32_t fallback)
{
    if (!sni_cb_anim_has_js_cb(ctx, slot))
    {
        return fallback;
    }

    if (sni_context_is_paused(ctx->owner_ctx))
    {
        return fallback;
    }

    if (!ctx->owner_ctx || !ctx->owner_ctx->owner)
    {
        return fallback;
    }

    if (ctx->owner_ctx->owner->state != SCRIPT_PROGRAM_STATE_ACTIVE)
    {
        EOS_LOG_W("Anim int callback skipped: program not active (state=%d)", ctx->owner_ctx->owner->state);
        return fallback;
    }

    /* Capture engine generation BEFORE spm_call to detect re-entrant recovery. */
    uint32_t saved_gen = script_engine_get_gen();

    int32_t result = fallback;
    jerry_value_t js_anim = sni_cb_anim_make_js_anim(ctx);
    jerry_value_t args[1] = {js_anim};

    s_dispatching_anim_ctx = ctx;
    jerry_value_t ret = spm_call(ctx->owner_ctx->owner, ctx->cb_slots[slot], jerry_undefined(), args, 1);
    s_dispatching_anim_ctx = NULL;

    if (sni_cb_detect_recovery(saved_gen, ctx->owner_ctx))
    {
        return fallback; /* context destroyed — return safe default */
    }

    if (jerry_value_is_error(ret) || jerry_value_is_exception(ret))
    {
        EOS_LOG_E("Anim callback encountered an error");
    }
    else if (jerry_value_is_number(ret))
    {
        result = jerry_value_as_int32(ret);
    }

    jerry_value_free(ret);
    jerry_value_free(js_anim);
    return result;
}

static int32_t sni_cb_anim_get_value_dispatch(lv_anim_t *a)
{
    sni_anim_callback_ctx_t *ctx = (sni_anim_callback_ctx_t *)lv_anim_get_user_data(a);
    return sni_cb_anim_call_int_slot(ctx, SNI_ANIM_CB_SLOT_GET_VALUE, 0);
}

static int32_t sni_cb_anim_path_dispatch(const lv_anim_t *a)
{
    sni_anim_callback_ctx_t *ctx = (sni_anim_callback_ctx_t *)lv_anim_get_user_data(a);
    if (!ctx)
    {
        return lv_anim_path_linear(a);
    }

    if (ctx->path_kind == SNI_ANIM_PATH_KIND_BUILTIN && ctx->builtin_path_fn)
    {
        return ctx->builtin_path_fn(a);
    }

    if (ctx->path_kind == SNI_ANIM_PATH_KIND_JS)
    {
        return sni_cb_anim_call_int_slot(ctx, SNI_ANIM_CB_SLOT_PATH, lv_anim_path_linear(a));
    }

    return lv_anim_path_linear(a);
}

sni_context_t *sni_cb_get_context(void)
{
    script_program_t *prog = spm_get_active_program();
    return prog ? prog->sni_ctx : NULL;
}

void sni_cb_context_cleanup_events(sni_context_t *ctx)
{
    if (!ctx)
        return;

    /* Phase guard: this function removes event descriptors from LIVE LVGL
     * objects and frees JS values.  Both require the JS heap and LVGL tree
     * to be intact.  If called after ENGINE_STOPPED, jerry_value_free on
     * stale handles is UB.  If called after LVGL tree deletion, the owner
     * object is a dangling pointer. */
    if (ctx->teardown_phase >= SNI_TEARDOWN_PHASE_ENGINE_STOPPED)
    {
        EOS_LOG_E("cleanup_events called at phase %d — JS heap may be stale! "
                  "Must run before script_engine_stop (Phase 3).",
                  ctx->teardown_phase);
    }

    sni_event_callback_ctx_t *event_ctx = *(sni_event_callback_ctx_t **)&ctx->event_ctx_list;
    *(sni_event_callback_ctx_t **)&ctx->event_ctx_list = NULL;
    while (event_ctx)
    {
        sni_event_callback_ctx_t *next = event_ctx->next;

        event_ctx->alive = false;

        if (event_ctx->dsc && event_ctx->owner)
        {
            /* Only remove the event descriptor if the owner object is still
             * in LVGL's widget tree.  If the object has already been freed
             * (e.g. by a deferred cleanup timer or off-stack activity
             * destruction), its spec_attr→event_list is garbage and calling
             * lv_obj_remove_event_dsc would EXC_BAD_ACCESS. */
            if (lv_obj_is_valid(event_ctx->owner))
            {
                lv_obj_remove_event_dsc(event_ctx->owner, event_ctx->dsc);
            }
            event_ctx->dsc = NULL;
        }

        sni_cb_safe_jerry_value_free(ctx, &event_ctx->js_cb);
        sni_cb_safe_jerry_value_free(ctx, &event_ctx->js_user_data);

        eos_free(event_ctx);
        event_ctx = next;
    }
}

bool sni_cb_anim_create(sni_anim_callback_ctx_t **out_ctx)
{
    if (!out_ctx)
    {
        return false;
    }

    sni_anim_callback_ctx_t *ctx = eos_malloc_zeroed(sizeof(sni_anim_callback_ctx_t));
    if (!ctx)
    {
        return false;
    }

    for (uint32_t i = 0; i < SNI_ANIM_CB_SLOT_COUNT; i++)
    {
        ctx->cb_slots[i] = jerry_undefined();
    }
    ctx->owner_ctx = sni_cb_get_context();
    ctx->state = SNI_ANIM_STATE_ACTIVE;
    ctx->engine_gen = script_engine_get_gen();

    lv_anim_init(&ctx->pre_anim);
    lv_anim_set_user_data(&ctx->pre_anim, ctx);
    lv_anim_set_deleted_cb(&ctx->pre_anim, sni_cb_anim_deleted_dispatch);

    *out_ctx = ctx;
    return true;
}

lv_anim_t *sni_cb_anim_get_lv_anim(sni_anim_callback_ctx_t *ctx)
{
    if (!ctx)
    {
        return NULL;
    }

    if (ctx->state == SNI_ANIM_STATE_ACTIVE && ctx->active_anim)
    {
        return ctx->active_anim;
    }

    return &ctx->pre_anim;
}

bool sni_cb_anim_set_cb(sni_anim_callback_ctx_t *ctx, sni_anim_cb_slot_t slot, jerry_value_t js_cb)
{
    lv_anim_t *anim = NULL;

    if (!ctx || slot >= SNI_ANIM_CB_SLOT_COUNT || slot == SNI_ANIM_CB_SLOT_PATH)
    {
        return false;
    }

    if (!sni_cb_anim_store_slot(ctx, slot, js_cb))
    {
        return false;
    }

    anim = sni_cb_anim_get_lv_anim(ctx);
    if (!anim)
    {
        return false;
    }

    switch (slot)
    {
        case SNI_ANIM_CB_SLOT_CUSTOM_EXEC:
            lv_anim_set_custom_exec_cb(anim, sni_cb_anim_custom_exec_dispatch);
            break;
        case SNI_ANIM_CB_SLOT_START:
            lv_anim_set_start_cb(anim, sni_cb_anim_start_dispatch);
            break;
        case SNI_ANIM_CB_SLOT_COMPLETED:
            lv_anim_set_completed_cb(anim, sni_cb_anim_completed_dispatch);
            break;
        case SNI_ANIM_CB_SLOT_DELETED:
            lv_anim_set_deleted_cb(anim, sni_cb_anim_deleted_dispatch);
            break;
        case SNI_ANIM_CB_SLOT_GET_VALUE:
            lv_anim_set_get_value_cb(anim, sni_cb_anim_get_value_dispatch);
            break;
        default:
            return false;
    }

    return true;
}

bool sni_cb_anim_set_path_builtin(sni_anim_callback_ctx_t *ctx, sni_anim_path_builtin_t path_kind)
{
    lv_anim_t *anim = NULL;

    if (!ctx || path_kind >= SNI_ANIM_PATH_ENUM_MAX)
    {
        return false;
    }

    anim = sni_cb_anim_get_lv_anim(ctx);
    if (!anim)
    {
        return false;
    }

    sni_cb_anim_clear_slot(ctx, SNI_ANIM_CB_SLOT_PATH);
    ctx->path_kind = SNI_ANIM_PATH_KIND_NONE;
    ctx->builtin_path_fn = NULL;

    if (path_kind == SNI_ANIM_PATH_NONE)
    {
        lv_anim_set_path_cb(anim, lv_anim_path_linear);
        return true;
    }

    if (!s_anim_path_table[path_kind])
    {
        return false;
    }

    ctx->path_kind = SNI_ANIM_PATH_KIND_BUILTIN;
    ctx->builtin_path_fn = s_anim_path_table[path_kind];
    lv_anim_set_path_cb(anim, sni_cb_anim_path_dispatch);
    return true;
}

bool sni_cb_anim_set_path_js(sni_anim_callback_ctx_t *ctx, jerry_value_t js_cb)
{
    lv_anim_t *anim = NULL;

    if (!ctx || !jerry_value_is_function(js_cb))
    {
        return false;
    }

    if (!sni_cb_anim_store_slot(ctx, SNI_ANIM_CB_SLOT_PATH, js_cb))
    {
        return false;
    }

    anim = sni_cb_anim_get_lv_anim(ctx);
    if (!anim)
    {
        return false;
    }

    ctx->path_kind = SNI_ANIM_PATH_KIND_JS;
    ctx->builtin_path_fn = NULL;
    lv_anim_set_path_cb(anim, sni_cb_anim_path_dispatch);
    return true;
}

bool sni_cb_anim_start(sni_anim_callback_ctx_t *ctx)
{
    lv_anim_t *active_anim = NULL;

    if (!ctx)
    {
        return false;
    }

    if (ctx->state == SNI_ANIM_STATE_ACTIVE && ctx->active_anim)
    {
        return false;
    }

    if (ctx->state != SNI_ANIM_STATE_ACTIVE)
    {
        return false;
    }

    lv_anim_set_user_data(&ctx->pre_anim, ctx);
    active_anim = lv_anim_start(&ctx->pre_anim);
    if (!active_anim)
    {
        return false;
    }

    ctx->active_anim = active_anim;
    lv_anim_set_user_data(active_anim, ctx);

    return true;
}
