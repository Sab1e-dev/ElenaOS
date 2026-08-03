/**
 * @file sni_context.h
 * @brief Per-Realm context lifecycle management
 *
 * Each Realm owns a sni_context_t instance. Managed Resources (timers, anims,
 * styles, fonts, etc.) are registered into type-indexed linked lists so that
 * when the context is torn down every outstanding resource is reliably freed.
 *
 * Object Tree Nodes (e.g., lv.obj) are NOT stored in the context - they use
 * LVGL's user_data mechanism for O(1) bidirectional access and their lifecycle
 * is tied to the LVGL object tree.
 */

#ifndef SNI_CONTEXT_H
#define SNI_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "sni_types.h"
#include "lvgl.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a per-realm cleanup context
 * @return New sni_context_t instance, or NULL on allocation failure
 */
sni_context_t *sni_context_create(void);

/**
 * @brief Destroy a context and free all resource list nodes
 * @param ctx Context to destroy (safe to pass NULL)
 */
void sni_context_destroy(sni_context_t *ctx);

/**
 * @brief Clear all resource lists without destroying the context itself
 * @param ctx Target context
 */
void sni_context_clear(sni_context_t *ctx);

/**
 * @brief Set paused state on context (suppresses callback dispatch)
 * @param ctx Target context
 * @param paused Whether to suppress callbacks
 */
void sni_context_set_paused(sni_context_t *ctx, bool paused);

/**
 * @brief Query paused state of context
 * @param ctx Target context
 * @return true if callbacks are suppressed
 */
bool sni_context_is_paused(sni_context_t *ctx);

/**
 * @brief Get array index for a managed resource type
 * @param type Handle type (must be a managed resource type)
 * @return Array index (0-based), or -1 if not a managed resource
 */
int sni_context_get_type_index(sni_type_t type);

/**
 * @brief Register a managed resource into its type-specific list
 * @param ctx Target context
 * @param ptr Native resource pointer
 * @param js_obj JavaScript object reference
 * @param type Resource type
 */
void sni_context_add_resource(sni_context_t *ctx, void *ptr, jerry_value_t js_obj, sni_type_t type);

/**
 * @brief Remove a managed resource from its type-specific list
 * @param ctx Target context
 * @param ptr Native resource pointer
 * @param type Resource type
 */
void sni_context_remove_resource(sni_context_t *ctx, void *ptr, sni_type_t type);

/**
 * @brief Find a resource node by native pointer in a specific type list
 * @param ctx Target context
 * @param ptr Native resource pointer
 * @param type Resource type
 * @return Resource node pointer, or NULL if not found
 */
sni_managed_resource_node_t *sni_context_find_resource(sni_context_t *ctx, void *ptr, sni_type_t type);

/**
 * @brief Invalidate a resource without freeing its node.
 *
 * Sets node->ptr = NULL so the sweep (Phase 4b) does not double-free a
 * Hybrid Resource already destroyed by another subsystem (e.g. Activity
 * Controller).  The node memory itself is NOT freed — the sweep will
 * free it during Phase 4b.
 *
 * @param ctx Target context (NULL-safe)
 * @param ptr Native resource pointer (NULL-safe)
 * @param type Resource type
 */
void sni_context_invalidate_resource(sni_context_t *ctx, void *ptr, sni_type_t type);

/**
 * @brief Iterate over all resources in a specific type
 * @param ctx Target context
 * @param type Resource type
 * @param cb Callback invoked for each entry (ptr, js_obj, type, is_alive, user_data)
 * @param user_data User pointer passed to callback
 */
void sni_context_iterate_type(sni_context_t *ctx,
                              sni_type_t type,
                              void (*cb)(void *, jerry_value_t, sni_type_t, bool, void *),
                              void *user_data);

/* Convenience wrappers for common resource types */

void sni_context_add_timer(sni_context_t *ctx, void *ptr, jerry_value_t js_obj);
void sni_context_remove_timer(sni_context_t *ctx, void *ptr);

void sni_context_add_anim(sni_context_t *ctx, void *ptr, jerry_value_t js_obj);
void sni_context_remove_anim(sni_context_t *ctx, void *ptr);

/* Unified resource lifecycle management */

void sni_context_delete_timer_sync(sni_context_t *ctx, lv_timer_t *timer);
void sni_context_request_async_delete_timer(sni_context_t *ctx, lv_timer_t *timer);

void sni_context_delete_anim_sync(sni_context_t *ctx, void *anim_ctx);
void sni_context_request_async_delete_anim(sni_context_t *ctx, void *anim_ctx);

void sni_context_sweep_all(sni_context_t *ctx);

/**
 * @brief Release all JS callback references without freeing native resources.
 *
 * Frees timer/animation JavaScript function references held by this context
 * and runs multiple GC passes to reclaim bytecode memory. This must be called
 * BEFORE script_engine_stop() so that bytecode refcounts reach zero before
 * the engine releases modules.
 *
 * Safe to call multiple times - subsequent calls are no-ops because freed
 * values are set to jerry_undefined().
 *
 * @param ctx Target context
 */
void sni_context_sweep_js_refs(sni_context_t *ctx);

void sni_context_clear_native_ptrs_all(sni_context_t *ctx);

/**
 * @brief Neutralize all LVGL timers in this context without deleting them.
 *
 * Sets cb and user_data to NULL on each timer so they become harmless
 * zombies.  Does NOT call lv_timer_delete (unsafe during callback) and
 * does NOT touch any JerryScript values (unsafe after fatal assertion).
 * Safe to call during spm_handle_engine_reset.
 */
void sni_context_neutralize_timers(sni_context_t *ctx);

/**
 * @brief Neutralize all animation contexts without freeing them.
 *
 * Marks each anim_ctx as DELETED and clears owner_ctx so callbacks
 * will be rejected by sni_cb_detect_recovery.  Does NOT free anim_ctx
 * memory (may still be on the stack) and does NOT touch JerryScript.
 * Safe to call during spm_handle_engine_reset.
 */
void sni_context_neutralize_anims(sni_context_t *ctx);

void sni_context_dump_counters(sni_context_t *ctx);
const char *sni_type_name(sni_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* SNI_CONTEXT_H */
