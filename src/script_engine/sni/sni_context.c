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

static const char *_sni_type_names[] = {
    /* Tree-Dependent */
    "LV_CHART_CURSOR",
    "LV_CHART_SERIES",
    "LV_EVENT_CB",
    "LV_EVENT_DSC",
    /* Hybrid */
    "EOS_ACTIVITY",
    "EOS_VIEW",
    /* Pure Managed */
    "LV_TIMER",
    "LV_STYLE",
    "LV_ANIM",
    "LV_FONT",
    "LV_GROUP",
    "LV_LAYER",
    "LV_OBSERVER",
    "LV_DRAW_BUF",
    "LV_SUBJECT",
    "LV_COLOR_FILTER_DSC",
    /* Value-like Handles (TODO: migrate to __SNI_VALUE) */
    "INT32",
    "LV_DISPLAY",
    "LV_DRAW_ARC_DSC",
    "LV_DRAW_IMAGE_DSC",
    "LV_DRAW_LABEL_DSC",
    "LV_DRAW_LINE_DSC",
    "LV_DRAW_RECT_DSC",
    "LV_EVENT",
    "LV_GRAD_DSC",
    "LV_IMAGE_DSC",
    "LV_OBJ_CLASS",
    "LV_OBJ_TREE_WALK_CB",
    "LV_STYLE_TRANSITION_DSC",
    "LV_STYLE_VALUE",
};

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
    EOS_LOG_I("[COUNTER] ctx=%p phase=%d resource counts:", (void *)ctx, ctx->teardown_phase);
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

    ctx->teardown_phase = SNI_TEARDOWN_PHASE_COMPLETE;
    EOS_LOG_D("DESTROY context=%p phase=%d", (void *)ctx, ctx->teardown_phase);
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

void sni_context_invalidate_resource(sni_context_t *ctx, void *ptr, sni_type_t type)
{
    if (!ctx || !ptr)
        return;

    int idx = sni_context_get_type_index(type);
    if (idx < 0)
        return;

    sni_managed_resource_node_t *node = ctx->resource_heads[idx];
    while (node)
    {
        if (node->ptr == ptr)
        {
            /* Null out the native pointer so the sweep (Phase 4b) does not
             * double-free an Activity already destroyed by the controller. */
            node->ptr = NULL;
            EOS_LOG_D("INVALIDATE_RESOURCE: ctx=%p ptr=%p type=%s(%d)", ctx, ptr, sni_type_name(type), type);
            return;
        }
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
    script_engine_state_t state = script_engine_get_state();
    if (state == SCRIPT_ENGINE_STATE_UNINITIALIZED || state == SCRIPT_ENGINE_STATE_IDLE)
    {
        EOS_LOG_W("Skip jerry_value_free: engine not running (state=%d)", state);
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
        /* If this timer is currently executing its JS callback (i.e. the
         * script called timer.delete() on itself), we must NOT free the
         * callback context or the JerryScript function — both are still
         * on the C/JS call stack.  Instead mark the timer pending-delete
         * and null out the LVGL callback.  sni_cb_timer_dispatch() will
         * perform the actual deletion after spm_call() returns. */
        if (sni_cb_is_dispatching_timer(timer))
        {
            cb_ctx->state = SNI_TIMER_STATE_PENDING_DELETE;
            /* Null out the LVGL callback so the timer won't fire again,
             * but keep user_data intact so the deferred cleanup in
             * sni_cb_timer_dispatch() can find cb_ctx after spm_call
             * returns. */
            lv_timer_set_cb(timer, NULL);
            return;
        }

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

    EOS_LOG_I("SWEEP: ctx=%p entry_phase=%d", (void *)ctx, ctx->teardown_phase);
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

    /* Phase 2: Release all native resources and free node memory.
     *
     * SUB-PHASE ORDER IS CRITICAL — matches sni.mdx "Realm 销毁顺序" Phase 4.
     * 4a (Tree-Dependent) MUST run before 4b (Hybrid) because 4b's
     *   eos_activity_destroy → lv_obj_delete fires LV_EVENT_DELETE on child
     *   widgets whose sub_resource_head was freed in 4a.
     * 4b (Hybrid) MUST run before 4c (Pure) because Hybrid Activities may
     *   internally reference Pure resources (styles, fonts, etc.). */

    /* Phase 4a: Tree-Dependent Resources -------------------------*/
    ctx->teardown_phase = SNI_TEARDOWN_PHASE_SWEEP_TREE_DEP;
    for (int i = 0; i < SNI_MANAGED_RESOURCE_COUNT; i++)
    {
        sni_type_t list_type = (sni_type_t)(i + __SNI_HANDLE_RESOURCE_START + 1);
        if (!SNI_TYPE_IS_TREE_DEPENDENT(list_type))
            continue;

        sni_managed_resource_node_t *node = ctx->resource_heads[i];
        int freed_count = 0;
        while (node)
        {
            sni_managed_resource_node_t *next = node->next;

            /* Unlink from parent control block's sub_resource_head BEFORE
             * freeing the node.  If left linked, the parent's LV_EVENT_DELETE
             * handler (sni_obj_deleted_cb) would walk freed node memory. */
            if (node->parent_cb)
            {
                sni_managed_resource_node_t **pp = &node->parent_cb->sub_resource_head;
                while (*pp)
                {
                    if (*pp == node)
                    {
                        *pp = node->next;
                        break;
                    }
                    pp = &(*pp)->next;
                }
                node->parent_cb = NULL;
            }

            /* Tree-Dependent: never destroy the native object — LVGL
             * reclaims it automatically when the parent tree node is
             * deleted in Phase 6 (lv_obj_delete(activity->view)). */
            node->ptr = NULL;

            eos_free(node);
            freed_count++;
            node = next;
        }
        ctx->resource_heads[i] = NULL;
        ctx->resource_counts[i] = 0;
        if (freed_count > 0)
        {
            EOS_LOG_I("SWEEP-4a: freed %d Tree-Dependent nodes [%d] %s", freed_count, i, _sni_type_names[i]);
        }
    }

    /* Phase 4b: Hybrid Resources ---------------------------------*/
    ctx->teardown_phase = SNI_TEARDOWN_PHASE_SWEEP_HYBRID;
    for (int i = 0; i < SNI_MANAGED_RESOURCE_COUNT; i++)
    {
        sni_type_t list_type = (sni_type_t)(i + __SNI_HANDLE_RESOURCE_START + 1);
        if (!SNI_TYPE_IS_HYBRID(list_type))
            continue;

        sni_managed_resource_node_t *node = ctx->resource_heads[i];
        int freed_count = 0;
        while (node)
        {
            sni_managed_resource_node_t *next = node->next;

            if (node->ptr)
            {
                if (node->type == SNI_H_EOS_ACTIVITY)
                {
                    eos_activity_t *act = (eos_activity_t *)node->ptr;

                    /* Only destroy off-stack Activities that were never entered
                     * (has_started == false).  These are JS-created Activities
                     * with NULL lifecycle that exist purely as managed resources.
                     *
                     * Native Activities (created by the app launcher) have
                     * on_destroy callbacks that tear down the script engine;
                     * destroying them from inside the sweep triggers a
                     * re-entrant cascade (on_destroy → spm_app_stop →
                     * _program_destroy → sni_context_sweep_all) and double-free.
                     *
                     * JS-created Activities that WERE entered (has_started==true)
                     * are managed by the activity controller and destroyed via
                     * the animation-callback path.  Their node->ptr may have
                     * been set to NULL by sni_context_invalidate_resource()
                     * to prevent double-free. */
                    if (!eos_activity_has_started(act))
                    {
                        eos_activity_destroy(act);
                        node->ptr = NULL;
                    }
                }
                /* EOS_VIEW: no native destruction — views are cleaned up
                 * when their owning Activity is destroyed. */
            }

            eos_free(node);
            freed_count++;
            node = next;
        }
        ctx->resource_heads[i] = NULL;
        ctx->resource_counts[i] = 0;
        if (freed_count > 0)
        {
            EOS_LOG_I("SWEEP-4b: freed %d Hybrid nodes [%d] %s", freed_count, i, _sni_type_names[i]);
        }
    }

    /* Phase 4c: Pure Managed Resources ---------------------------*/
    ctx->teardown_phase = SNI_TEARDOWN_PHASE_SWEEP_PURE;
    for (int i = 0; i < SNI_MANAGED_RESOURCE_COUNT; i++)
    {
        sni_type_t list_type = (sni_type_t)(i + __SNI_HANDLE_RESOURCE_START + 1);
        if (!SNI_TYPE_IS_PURE_MANAGED(list_type))
            continue;

        sni_managed_resource_node_t *node = ctx->resource_heads[i];
        int freed_count = 0;
        while (node)
        {
            sni_managed_resource_node_t *next = node->next;

            if (node->ptr)
            {
                switch (node->type)
                {
                    case SNI_H_LV_TIMER:
                    {
                        lv_timer_t *timer = (lv_timer_t *)node->ptr;
                        if (sni_cb_is_dispatching_timer(timer))
                        {
                            EOS_LOG_W("SWEEP: skipping currently dispatching timer %p", (void *)timer);
                            lv_timer_set_user_data(timer, NULL);
                            lv_timer_set_cb(timer, NULL);
                            node = next;
                            continue;
                        }
                        sni_timer_callback_ctx_t *cb_ctx = (sni_timer_callback_ctx_t *)lv_timer_get_user_data(timer);
                        lv_timer_set_user_data(timer, NULL);
                        lv_timer_set_cb(timer, NULL);
                        if (cb_ctx)
                        {
                            cb_ctx->state = SNI_TIMER_STATE_DELETED;
                            cb_ctx->owner_ctx = NULL;
                            eos_free(cb_ctx);
                        }
                        lv_timer_delete(timer);
                        break;
                    }

                    case SNI_H_LV_ANIM:
                    {
                        sni_anim_callback_ctx_t *anim_ctx = (sni_anim_callback_ctx_t *)node->ptr;
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
                            anim_ctx->state = SNI_ANIM_STATE_DELETED;
                            anim_ctx->owner_ctx = NULL;
                            if (anim_ctx->active_anim)
                            {
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
                        break;
                    }

                    case SNI_H_LV_STYLE:
                    {
                        lv_style_t *style = (lv_style_t *)node->ptr;
                        lv_style_reset(style);
                        eos_free(style);
                        break;
                    }

                    case SNI_H_LV_FONT:
                    {
                        /* Only dynamically-loaded binary fonts need explicit
                         * cleanup.  System / built-in fonts are static. */
                        lv_font_t *font = (lv_font_t *)node->ptr;
                        lv_binfont_destroy(font);
                        break;
                    }

                    case SNI_H_LV_GROUP:
                    {
                        lv_group_t *group = (lv_group_t *)node->ptr;
                        lv_group_delete(group);
                        break;
                    }

                    case SNI_H_LV_LAYER:
                    {
                        /* LVGL layers are managed internally by the display
                         * system.  No explicit destroy API exists — the layer
                         * is cleaned up when its display is deleted. */
                        break;
                    }

                    case SNI_H_LV_OBSERVER:
                    {
                        lv_observer_t *observer = (lv_observer_t *)node->ptr;
                        lv_observer_remove(observer);
                        break;
                    }

                    case SNI_H_LV_DRAW_BUF:
                    {
                        lv_draw_buf_t *draw_buf = (lv_draw_buf_t *)node->ptr;
                        lv_draw_buf_destroy(draw_buf);
                        break;
                    }

                    case SNI_H_LV_SUBJECT:
                    {
                        lv_subject_t *subject = (lv_subject_t *)node->ptr;
                        lv_subject_deinit(subject);
                        eos_free(subject);
                        break;
                    }

                    case SNI_H_LV_COLOR_FILTER_DSC:
                    {
                        /* No public deinit API — color filter descriptors are
                         * typically stack-scoped.  Just free the heap copy. */
                        lv_color_filter_dsc_t *dsc = (lv_color_filter_dsc_t *)node->ptr;
                        eos_free(dsc);
                        break;
                    }

                    default:
                        break;
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
            EOS_LOG_I("SWEEP-4c: freed %d Pure-Managed nodes [%d] %s", freed_count, i, _sni_type_names[i]);
        }
    }

    /* Value-like & unhandled types: free node memory only --------*/
    ctx->teardown_phase = SNI_TEARDOWN_PHASE_SWEEP_VALUE_LIKE;
    for (int i = 0; i < SNI_MANAGED_RESOURCE_COUNT; i++)
    {
        if (ctx->resource_heads[i] == NULL)
            continue;

        sni_type_t list_type = (sni_type_t)(i + __SNI_HANDLE_RESOURCE_START + 1);
        /* Skip types already handled in 4a/4b/4c */
        if (SNI_TYPE_IS_TREE_DEPENDENT(list_type) || SNI_TYPE_IS_HYBRID(list_type)
            || SNI_TYPE_IS_PURE_MANAGED(list_type))
            continue;

        sni_managed_resource_node_t *node = ctx->resource_heads[i];
        int freed_count = 0;
        while (node)
        {
            sni_managed_resource_node_t *next = node->next;
            /* Value-like handles and unclassified types: no native resource
             * to destroy — just free the JS wrapper node. */
            node->ptr = NULL;
            eos_free(node);
            freed_count++;
            node = next;
        }
        ctx->resource_heads[i] = NULL;
        ctx->resource_counts[i] = 0;
        if (freed_count > 0)
        {
            EOS_LOG_I("SWEEP-4d: freed %d value-like/unclassified nodes [%d] %s", freed_count, i, _sni_type_names[i]);
        }
    }

    ctx->teardown_phase = SNI_TEARDOWN_PHASE_CTX_DESTROY;

    _SWEEP_HEAP_LOG("sweep end");
    sni_context_dump_counters(ctx);

    EOS_LOG_I("SWEEP: complete for ctx=%p phase=%d", (void *)ctx, ctx->teardown_phase);
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
