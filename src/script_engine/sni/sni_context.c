/**
 * @file sni_context.c
 * @brief Per-Realm context lifecycle management
 */

#include "sni_context.h"

/* Includes ---------------------------------------------------*/
#include <stdlib.h>
#include "eos_mem.h"
#include "sni_types.h"
#include "lvgl.h"
#include "sni_callback_runtime.h"
#include "sni_type_bridge.h"
#include "eos_activity.h"
#include "script_engine_core.h"
#include "jerryscript.h"
#define EOS_LOG_TAG "SNI-Context"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

#define _SWEEP_HEAP_LOG(tag)                                                                                \
    do                                                                                                      \
    {                                                                                                       \
        if (jerry_feature_enabled(JERRY_FEATURE_HEAP_STATS))                                                \
        {                                                                                                   \
            jerry_heap_stats_t _s = {0};                                                                    \
            if (jerry_heap_stats(&_s))                                                                      \
                EOS_LOG_I("[HEAP] %s: alloc=%u peak=%u", tag, _s.allocated_bytes, _s.peak_allocated_bytes); \
        }                                                                                                   \
    } while (0)

static const char *_sni_type_names[] = {"LV_TIMER",
                                        "LV_STYLE",
                                        "LV_ANIM",
                                        "LV_CHART_CURSOR",
                                        "LV_CHART_SERIES",
                                        "INT32",
                                        "LV_COLOR_FILTER_DSC",
                                        "LV_DISPLAY",
                                        "EOS_ACTIVITY",
                                        "LV_DRAW_BUF",
                                        "LV_DRAW_ARC_DSC",
                                        "LV_DRAW_IMAGE_DSC",
                                        "LV_DRAW_LABEL_DSC",
                                        "LV_DRAW_LINE_DSC",
                                        "LV_DRAW_RECT_DSC",
                                        "LV_EVENT",
                                        "LV_EVENT_CB",
                                        "LV_EVENT_DSC",
                                        "LV_FONT",
                                        "LV_GRAD_DSC",
                                        "LV_GROUP",
                                        "LV_IMAGE_DSC",
                                        "LV_LAYER",
                                        "LV_OBJ_CLASS",
                                        "LV_OBJ_TREE_WALK_CB",
                                        "LV_OBSERVER",
                                        "LV_STYLE_TRANSITION_DSC",
                                        "LV_STYLE_VALUE",
                                        "LV_SUBJECT"};

const char *sni_type_name(sni_type_t type)
{
    int idx = sni_context_get_type_index(type);
    if (idx < 0 || idx >= SNI_MANAGED_RESOURCE_COUNT)
        return "UNKNOWN";
    return _sni_type_names[idx];
}

void sni_context_dump_counters(sni_context_t *ctx)
{
    if (!ctx)
        return;
    int total = 0;
    EOS_LOG_I("[COUNTER] ctx=%p resource counts:", (void *)ctx);
    for (int i = 0; i < SNI_MANAGED_RESOURCE_COUNT; i++)
    {
        if (ctx->resource_counts[i] > 0)
        {
            EOS_LOG_I("  [%2d] %-25s : %d", i, _sni_type_names[i], ctx->resource_counts[i]);
            total += ctx->resource_counts[i];
        }
    }
    if (total == 0)
    {
        EOS_LOG_I("  (all zero)");
    }
    else
    {
        EOS_LOG_I("  TOTAL: %d", total);
    }
}

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

int sni_context_get_type_index(sni_type_t type)
{
    if (!SNI_TYPE_IS_MANAGED_RESOURCE(type))
    {
        return -1;
    }
    return (int)(type - __SNI_HANDLE_RESOURCE_START - 1);
}

sni_context_t *sni_context_create(void)
{
    sni_context_t *ctx = eos_malloc_zeroed(sizeof(sni_context_t));
    if (!ctx)
    {
        EOS_LOG_E("CREATE: failed to allocate context");
        return NULL;
    }
    EOS_LOG_D("CREATE context=%p (size=%zu)", ctx, sizeof(sni_context_t));
    return ctx;
}

void sni_context_iterate_type(sni_context_t *ctx,
                              sni_type_t type,
                              void (*cb)(void *, jerry_value_t, sni_type_t, bool, void *),
                              void *user_data)
{
    if (!ctx || !cb)
        return;

    int idx = sni_context_get_type_index(type);
    if (idx < 0)
        return;

    sni_managed_resource_node_t *node = ctx->resource_heads[idx];
    while (node)
    {
        sni_managed_resource_node_t *next = node->next;
        if (node->ptr)
        {
            cb(node->ptr, node->js_obj, node->type, node->is_alive, user_data);
        }
        node = next;
    }
}

static void sni_context_free_type_list(sni_context_t *ctx, int idx)
{
    if (!ctx || idx < 0 || idx >= SNI_MANAGED_RESOURCE_COUNT)
        return;

    sni_managed_resource_node_t *node = ctx->resource_heads[idx];
    int count = 0;
    while (node)
    {
        sni_managed_resource_node_t *next = node->next;

        if (!jerry_value_is_undefined(node->js_obj) && !jerry_value_is_null(node->js_obj))
        {
            sni_tb_clear_resource_native_ptr(node->js_obj);
            jerry_value_free(node->js_obj);
        }

        count++;
        eos_free(node);
        node = next;
    }
    ctx->resource_heads[idx] = NULL;
    if (count > 0)
    {
        EOS_LOG_D("FREE_TYPE[%d] %s: freed %d nodes", idx, _sni_type_names[idx], count);
    }
}

void sni_context_destroy(sni_context_t *ctx)
{
    if (!ctx)
    {
        EOS_LOG_D("DESTROY: ctx is NULL");
        return;
    }

    EOS_LOG_D("DESTROY context=%p", (void *)ctx);
    sni_context_dump_counters(ctx);

    for (int i = 0; i < SNI_MANAGED_RESOURCE_COUNT; i++)
    {
        sni_context_free_type_list(ctx, i);
        ctx->resource_counts[i] = 0;
    }
    eos_free(ctx);

    EOS_LOG_D("DESTROY complete: context freed");
}

void sni_context_clear(sni_context_t *ctx)
{
    if (!ctx)
    {
        EOS_LOG_D("CLEAR: ctx is NULL");
        return;
    }

    EOS_LOG_D("CLEAR context=%p", (void *)ctx);
    sni_context_dump_counters(ctx);

    for (int i = 0; i < SNI_MANAGED_RESOURCE_COUNT; i++)
    {
        sni_context_free_type_list(ctx, i);
        ctx->resource_counts[i] = 0;
    }

    EOS_LOG_D("CLEAR complete");
}

void sni_context_add_resource(sni_context_t *ctx, void *ptr, jerry_value_t js_obj, sni_type_t type)
{
    if (!ctx || !ptr)
        return;

    int idx = sni_context_get_type_index(type);
    if (idx < 0)
        return;

    sni_managed_resource_node_t *node = eos_malloc_zeroed(sizeof(sni_managed_resource_node_t));
    if (!node)
        return;

    node->ptr = ptr;
    node->js_obj = jerry_value_copy(js_obj);
    node->type = type;
    node->is_alive = true;
    node->next = ctx->resource_heads[idx];
    ctx->resource_heads[idx] = node;
    ctx->resource_counts[idx]++;

    EOS_LOG_D("ADD_RESOURCE: ctx=%p ptr=%p type=%s(%d) idx=%d (count=%d)",
              ctx,
              ptr,
              sni_type_name(type),
              type,
              idx,
              ctx->resource_counts[idx]);
}

void sni_context_remove_resource(sni_context_t *ctx, void *ptr, sni_type_t type)
{
    if (!ctx || !ptr)
        return;

    int idx = sni_context_get_type_index(type);
    if (idx < 0)
        return;

    sni_managed_resource_node_t *prev = NULL;
    sni_managed_resource_node_t *node = ctx->resource_heads[idx];

    while (node)
    {
        if (node->ptr == ptr)
        {
            if (prev)
            {
                prev->next = node->next;
            }
            else
            {
                ctx->resource_heads[idx] = node->next;
            }

            if (!jerry_value_is_undefined(node->js_obj) && !jerry_value_is_null(node->js_obj))
            {
                sni_tb_clear_resource_native_ptr(node->js_obj);
                jerry_value_free(node->js_obj);
            }

            ctx->resource_counts[idx]--;
            eos_free(node);
            EOS_LOG_D("REMOVE_RESOURCE: ctx=%p ptr=%p type=%s(%d) (count=%d)",
                      ctx,
                      ptr,
                      sni_type_name(type),
                      type,
                      ctx->resource_counts[idx]);
            return;
        }
        prev = node;
        node = node->next;
    }
}

sni_managed_resource_node_t *sni_context_find_resource(sni_context_t *ctx, void *ptr, sni_type_t type)
{
    if (!ctx || !ptr)
        return NULL;

    int idx = sni_context_get_type_index(type);
    if (idx < 0)
        return NULL;

    sni_managed_resource_node_t *node = ctx->resource_heads[idx];
    while (node)
    {
        if (node->ptr == ptr)
        {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

/* Convenience wrappers ---------------------------------------*/

void sni_context_add_timer(sni_context_t *ctx, void *ptr, jerry_value_t js_obj)
{
    if (!ctx || !ptr)
        return;
    sni_context_add_resource(ctx, ptr, js_obj, SNI_H_LV_TIMER);
}

void sni_context_remove_timer(sni_context_t *ctx, void *ptr)
{
    sni_context_remove_resource(ctx, ptr, SNI_H_LV_TIMER);
}

void sni_context_add_anim(sni_context_t *ctx, void *ptr, jerry_value_t js_obj)
{
    if (!ctx || !ptr)
        return;
    sni_context_add_resource(ctx, ptr, js_obj, SNI_H_LV_ANIM);
}

void sni_context_remove_anim(sni_context_t *ctx, void *ptr)
{
    sni_context_remove_resource(ctx, ptr, SNI_H_LV_ANIM);
}

/* Unified resource lifecycle management ----------------------*/

static inline void _sni_ctx_safe_js_free(jerry_value_t *value)
{
    if (!value)
        return;
    if (jerry_value_is_undefined(*value) || jerry_value_is_null(*value))
        return;
    if (script_engine_get_state() == SCRIPT_ENGINE_STATE_UNINITIALIZED)
    {
        EOS_LOG_W("Skip jerry_value_free: engine not running");
        *value = jerry_undefined();
        return;
    }
    jerry_value_free(*value);
    *value = jerry_undefined();
}

void sni_context_delete_timer_sync(sni_context_t *ctx, lv_timer_t *timer)
{
    if (!ctx || !timer)
        return;

    sni_timer_callback_ctx_t *cb_ctx = (sni_timer_callback_ctx_t *)lv_timer_get_user_data(timer);

    if (cb_ctx)
    {
        _sni_ctx_safe_js_free(&cb_ctx->js_cb);
        cb_ctx->state = SNI_TIMER_STATE_DELETED;
        eos_free(cb_ctx);
    }

    lv_timer_set_user_data(timer, NULL);
    sni_context_remove_timer(ctx, timer);
    lv_timer_delete(timer);
}

void sni_context_request_async_delete_timer(sni_context_t *ctx, lv_timer_t *timer)
{
    if (!ctx || !timer)
        return;

    sni_timer_callback_ctx_t *cb_ctx = (sni_timer_callback_ctx_t *)lv_timer_get_user_data(timer);
    if (!cb_ctx || cb_ctx->state != SNI_TIMER_STATE_ACTIVE)
        return;

    sni_context_delete_timer_sync(ctx, timer);
}

void sni_context_delete_anim_sync(sni_context_t *ctx, void *anim_ctx_ptr)
{
    if (!ctx || !anim_ctx_ptr)
        return;

    sni_anim_callback_ctx_t *anim_ctx = (sni_anim_callback_ctx_t *)anim_ctx_ptr;

    if (anim_ctx->active_anim)
    {
        lv_anim_set_user_data(anim_ctx->active_anim, NULL);
        anim_ctx->active_anim = NULL;
    }

    for (int i = 0; i < SNI_ANIM_CB_SLOT_COUNT; i++)
    {
        if (!jerry_value_is_undefined(anim_ctx->cb_slots[i]) && !jerry_value_is_null(anim_ctx->cb_slots[i]))
        {
            jerry_value_free(anim_ctx->cb_slots[i]);
            anim_ctx->cb_slots[i] = jerry_undefined();
        }
    }

    sni_context_remove_anim(ctx, anim_ctx_ptr);
    anim_ctx->state = SNI_ANIM_STATE_DELETED;
    eos_free(anim_ctx);
}

void sni_context_request_async_delete_anim(sni_context_t *ctx, void *anim_ctx_ptr)
{
    if (!ctx || !anim_ctx_ptr)
        return;

    sni_anim_callback_ctx_t *anim_ctx = (sni_anim_callback_ctx_t *)anim_ctx_ptr;
    if (anim_ctx->state != SNI_ANIM_STATE_ACTIVE)
        return;

    sni_context_delete_anim_sync(ctx, anim_ctx_ptr);
}

void sni_context_clear_native_ptrs_all(sni_context_t *ctx)
{
    if (!ctx)
        return;
    for (int i = 0; i < SNI_MANAGED_RESOURCE_COUNT; i++)
    {
        sni_managed_resource_node_t *node = ctx->resource_heads[i];
        while (node)
        {
            if (!jerry_value_is_undefined(node->js_obj) && !jerry_value_is_null(node->js_obj))
            {
                sni_tb_clear_resource_native_ptr(node->js_obj);
            }
            node = node->next;
        }
    }
}

void sni_context_sweep_js_refs(sni_context_t *ctx)
{
    if (!ctx)
        return;

    EOS_LOG_I("SWEEP-JS: ctx=%p releasing JS callback references", (void *)ctx);

    _SWEEP_HEAP_LOG("sweep-js start");

    for (int i = 0; i < SNI_MANAGED_RESOURCE_COUNT; i++)
    {
        sni_managed_resource_node_t *node = ctx->resource_heads[i];
        while (node)
        {
            if (node->type == SNI_H_LV_TIMER && node->ptr)
            {
                sni_timer_callback_ctx_t *cb_ctx =
                    (sni_timer_callback_ctx_t *)lv_timer_get_user_data((lv_timer_t *)node->ptr);
                if (cb_ctx)
                    _sni_ctx_safe_js_free(&cb_ctx->js_cb);
            }
            else if (node->type == SNI_H_LV_ANIM && node->ptr)
            {
                sni_anim_callback_ctx_t *anim_ctx = (sni_anim_callback_ctx_t *)node->ptr;
                if (anim_ctx)
                {
                    for (int j = 0; j < SNI_ANIM_CB_SLOT_COUNT; j++)
                        _sni_ctx_safe_js_free(&anim_ctx->cb_slots[j]);
                }
            }

            if (!jerry_value_is_undefined(node->js_obj) && !jerry_value_is_null(node->js_obj))
            {
                jerry_value_free(node->js_obj);
                node->js_obj = jerry_undefined();
            }

            node = node->next;
        }
    }

    _SWEEP_HEAP_LOG("after sweep-js (JS frees)");

    for (int i = 0; i < 3; i++)
        jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);

    _SWEEP_HEAP_LOG("after sweep-js GC loop");
}

void sni_context_sweep_all(sni_context_t *ctx)
{
    if (!ctx)
        return;

    EOS_LOG_I("SWEEP: ctx=%p", (void *)ctx);
    sni_context_dump_counters(ctx);

    _SWEEP_HEAP_LOG("sweep start");

    /* Clear all native pointers from JS objects BEFORE releasing JS
     * references.  This prevents subsequent jerry_heap_gc (triggered by
     * sni_context_sweep_js_refs) from calling sni_resource_node_free_cb
     * which would free linked-list nodes while they are still referenced
     * by the context's resource_heads lists — causing a use-after-free
     * in Phase 2 below.
     *
     * This call is idempotent and safe to re-execute even if the caller
     * already cleared native pointers. */
    sni_context_clear_native_ptrs_all(ctx);

    /* Phase 1: Release all JS values only.
     * Native pointers are now cleared, so jerry_value_free and the
     * subsequent jerry_heap_gc will not trigger any native free
     * callbacks. All JS releases are batched here, separate from
     * native resource destruction.
     *
     * This phase is idempotent: if sni_context_sweep_js_refs was already
     * called before engine stop, re-running this phase is a no-op because
     * _sni_ctx_safe_js_free skips undefined/empty values. */
    sni_context_sweep_js_refs(ctx);

    /* Phase 2: Release all native resources and free node memory. */
    for (int i = 0; i < SNI_MANAGED_RESOURCE_COUNT; i++)
    {
        sni_managed_resource_node_t *node = ctx->resource_heads[i];
        int freed_count = 0;
        while (node)
        {
            sni_managed_resource_node_t *next = node->next;

            if (node->ptr)
            {
                if (node->type == SNI_H_LV_TIMER)
                {
                    lv_timer_t *timer = (lv_timer_t *)node->ptr;

                    /* If we are currently inside this timer's JS callback,
                     * skip native deletion — lv_timer_delete() on the
                     * executing timer would corrupt LVGL's internal
                     * iteration and cause EXC_BAD_ACCESS when the timer
                     * handler loop continues.  The timer will become a
                     * harmless zombie (NULL cb + user_data) and be
                     * cleaned up later. */
                    if (sni_cb_is_dispatching_timer(timer))
                    {
                        EOS_LOG_W("SWEEP: skipping currently dispatching timer %p", (void *)timer);
                        lv_timer_set_user_data(timer, NULL);
                        lv_timer_set_cb(timer, NULL);
                        /* Leave cb_ctx allocated — sni_cb_timer_dispatch
                         * will return through sni_cb_detect_recovery and
                         * we must not free ctx while it is on the stack.
                         * Advance to next node without freeing this one. */
                        node = next;
                        continue;
                    }

                    sni_timer_callback_ctx_t *cb_ctx = (sni_timer_callback_ctx_t *)lv_timer_get_user_data(timer);

                    /* Null out user_data and callback BEFORE freeing cb_ctx
                     * so that even if the timer fires concurrently (race),
                     * sni_cb_timer_dispatch sees NULL ctx and returns early. */
                    lv_timer_set_user_data(timer, NULL);
                    lv_timer_set_cb(timer, NULL);

                    if (cb_ctx)
                    {
                        cb_ctx->state = SNI_TIMER_STATE_DELETED;
                        cb_ctx->owner_ctx = NULL;
                        eos_free(cb_ctx);
                    }

                    lv_timer_delete(timer);
                }
                else if (node->type == SNI_H_LV_ANIM)
                {
                    sni_anim_callback_ctx_t *anim_ctx = (sni_anim_callback_ctx_t *)node->ptr;

                    /* If we are currently inside this animation's JS callback,
                     * skip freeing the anim_ctx — it is still on the stack.
                     * Mark it stale so the callback returns through
                     * sni_cb_detect_recovery. */
                    if (sni_cb_is_dispatching_anim(anim_ctx))
                    {
                        EOS_LOG_W("SWEEP: skipping currently dispatching anim ctx %p", (void *)anim_ctx);
                        anim_ctx->state = SNI_ANIM_STATE_DELETED;
                        anim_ctx->owner_ctx = NULL;
                        node = next;
                        continue;
                    }

                    if (anim_ctx)
                    {
                        /* Mark as DELETED so any concurrent dispatch sees it. */
                        anim_ctx->state = SNI_ANIM_STATE_DELETED;
                        anim_ctx->owner_ctx = NULL;

                        if (anim_ctx->active_anim)
                        {
                            /* Verify the pointer is still valid: if the
                               animation already completed (freed by LVGL's
                               anim_timer → anim_completed_handler), writing
                               to it would be use-after-free.  The user_data
                               check confirms this anim_ctx still owns the
                               active lv_anim_t. */
                            if (lv_anim_get_user_data(anim_ctx->active_anim) == anim_ctx)
                            {
                                anim_ctx->active_anim->deleted_cb = NULL;
                                anim_ctx->active_anim->custom_exec_cb = NULL;
                                lv_anim_set_user_data(anim_ctx->active_anim, NULL);
                            }
                            anim_ctx->active_anim = NULL;
                        }
                        eos_free(anim_ctx);
                    }
                }
                else if (node->type == SNI_H_EOS_ACTIVITY)
                {
                    /* Only destroy off-stack Activities that were never entered
                       (has_started == false).  These are JS-created Activities
                       with NULL lifecycle that exist purely as managed resources.

                       Native Activities (created by the app launcher) have
                       on_destroy callbacks that tear down the script engine;
                       destroying them from inside the sweep triggers a
                       re-entrant cascade (on_destroy → spm_app_stop →
                       _program_destroy → sni_context_sweep_all) and double-free.

                       JS-created Activities that WERE entered (has_started==true)
                       are managed by the activity controller and destroyed via
                       the animation-callback path. */
                    eos_activity_t *act = (eos_activity_t *)node->ptr;
                    if (!eos_activity_has_started(act))
                    {
                        eos_activity_destroy(act);
                        node->ptr = NULL;
                    }
                }
            }

            eos_free(node);
            freed_count++;
            node = next;
        }
        ctx->resource_heads[i] = NULL;
        ctx->resource_counts[i] = 0;
        if (freed_count > 0)
        {
            EOS_LOG_I("SWEEP: freed %d nodes of type [%d] %s", freed_count, i, _sni_type_names[i]);
        }
    }

    _SWEEP_HEAP_LOG("sweep end");
    sni_context_dump_counters(ctx);

    EOS_LOG_I("SWEEP: complete for ctx=%p", (void *)ctx);
}

/*
 * Engine-reset helpers — neutralize native LVGL resources WITHOUT touching
 * JerryScript values or deleting native objects.  Called from
 * spm_handle_engine_reset() after a fatal assertion, when the engine heap
 * may be internally inconsistent and any jerry_* API call can crash.
 */

void sni_context_neutralize_timers(sni_context_t *ctx)
{
    if (!ctx)
        return;

    int idx = sni_context_get_type_index(SNI_H_LV_TIMER);
    if (idx < 0)
        return;

    sni_managed_resource_node_t *node = ctx->resource_heads[idx];
    int count = 0;
    while (node)
    {
        if (node->ptr)
        {
            lv_timer_t *timer = (lv_timer_t *)node->ptr;
            lv_timer_set_user_data(timer, NULL);
            lv_timer_set_cb(timer, NULL);
            count++;
        }
        node = node->next;
    }
    if (count > 0)
        EOS_LOG_I("NEUTRALIZE: nulled %d timer(s) — LVGL objects kept alive as zombies", count);
}

void sni_context_neutralize_anims(sni_context_t *ctx)
{
    if (!ctx)
        return;

    int idx = sni_context_get_type_index(SNI_H_LV_ANIM);
    if (idx < 0)
        return;

    sni_managed_resource_node_t *node = ctx->resource_heads[idx];
    int count = 0;
    while (node)
    {
        if (node->ptr)
        {
            sni_anim_callback_ctx_t *anim_ctx = (sni_anim_callback_ctx_t *)node->ptr;
            anim_ctx->state = SNI_ANIM_STATE_DELETED;
            anim_ctx->owner_ctx = NULL;
            count++;
        }
        node = node->next;
    }
    if (count > 0)
        EOS_LOG_I("NEUTRALIZE: nulled %d anim(s) — ctx blocks kept alive", count);
}

void sni_context_set_paused(sni_context_t *ctx, bool paused)
{
    if (!ctx)
        return;
    ctx->paused = paused;
}

bool sni_context_is_paused(sni_context_t *ctx)
{
    return ctx ? ctx->paused : false;
}
