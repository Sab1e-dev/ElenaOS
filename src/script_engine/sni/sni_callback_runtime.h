/**
 * @file sni_callback_runtime.h
 * @brief SNI callback runtime
 */

#ifndef SNI_CALLBACK_RUNTIME_H
#define SNI_CALLBACK_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include "jerryscript.h"
#include "lvgl.h"
#include "sni_types.h"
#include "sni_context.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef enum
{
    SNI_TIMER_STATE_ACTIVE,
    SNI_TIMER_STATE_PENDING_DELETE,
    SNI_TIMER_STATE_DELETED
} sni_timer_state_t;

typedef struct sni_timer_callback_ctx
{
    lv_timer_t *timer;
    jerry_value_t js_cb;
    sni_context_t *owner_ctx;
    sni_timer_state_t state;
    bool auto_delete;
} sni_timer_callback_ctx_t;

/* Public function prototypes --------------------------------*/

/**
 * @brief Initialize callback runtime
 */
void sni_cb_runtime_init(void);

/**
 * @brief Register LVGL event callback (JS Function)
 * @param obj LVGL object
 * @param js_cb JS callback function
 * @param filter Event filter code
 * @param js_user_data JS user data (can be undefined)
 * @param out_dsc Returned event descriptor
 * @return bool Whether successful
 */
bool sni_cb_event_add(lv_obj_t *obj,
                      jerry_value_t js_cb,
                      lv_event_code_t filter,
                      jerry_value_t js_user_data,
                      lv_event_dsc_t **out_dsc);

/**
 * @brief Remove event callback by descriptor
 * @param obj LVGL object
 * @param dsc Event descriptor
 * @return bool Whether successful
 */
bool sni_cb_event_remove_dsc(lv_obj_t *obj, lv_event_dsc_t *dsc);

/**
 * @brief Remove event callback by JS callback function
 * @param obj LVGL object
 * @param js_cb JS callback function
 * @return bool Whether at least one callback was removed
 */
bool sni_cb_event_remove_by_js_cb(lv_obj_t *obj, jerry_value_t js_cb);

/**
 * @brief Remove event callback by JS callback function and JS user_data
 * @param obj LVGL object
 * @param js_cb JS callback function
 * @param js_user_data JS user data
 * @return uint32_t Number of removals
 */
uint32_t sni_cb_event_remove_by_js_cb_user_data(lv_obj_t *obj, jerry_value_t js_cb, jerry_value_t js_user_data);

/**
 * @brief Clean up callback context through event descriptor (used for handle destroy callback)
 * @param dsc Event descriptor
 */
void sni_cb_event_cleanup_descriptor(lv_event_dsc_t *dsc);

/* Timer Callback API ----------------------------------------*/

/**
 * @brief Create LVGL timer and bind JS callback
 * @param js_cb JS callback function, signature (timer) => void
 * @param period Timer period (milliseconds)
 * @param out_timer Returned created timer pointer
 * @return bool Whether successful
 */
bool sni_cb_timer_create(jerry_value_t js_cb, uint32_t period, lv_timer_t **out_timer);

/**
 * @brief Replace timer's JS callback function
 * @param timer LVGL timer
 * @param js_cb New JS callback function
 * @return bool Whether successful
 */
bool sni_cb_timer_set_cb(lv_timer_t *timer, jerry_value_t js_cb);

/**
 * @brief Set timer auto_delete flag (managed by SNI, not LVGL)
 * @param timer LVGL timer
 * @param auto_delete Whether to auto-delete when timer finishes
 * @return bool Whether successful
 */
bool sni_cb_timer_set_auto_delete(lv_timer_t *timer, bool auto_delete);

/* Anim Callback API -----------------------------------------*/

typedef enum
{
    SNI_ANIM_CB_SLOT_CUSTOM_EXEC = 0,
    SNI_ANIM_CB_SLOT_START,
    SNI_ANIM_CB_SLOT_COMPLETED,
    SNI_ANIM_CB_SLOT_DELETED,
    SNI_ANIM_CB_SLOT_GET_VALUE,
    SNI_ANIM_CB_SLOT_PATH,
    SNI_ANIM_CB_SLOT_COUNT,
} sni_anim_cb_slot_t;

typedef enum
{
    SNI_ANIM_PATH_KIND_NONE = 0,
    SNI_ANIM_PATH_KIND_BUILTIN,
    SNI_ANIM_PATH_KIND_JS,
} sni_anim_path_kind_t;

typedef enum
{
    SNI_ANIM_PATH_NONE = 0,
    SNI_ANIM_PATH_LINEAR = 1,
    SNI_ANIM_PATH_EASE_IN,
    SNI_ANIM_PATH_EASE_OUT,
    SNI_ANIM_PATH_EASE_IN_OUT,
    SNI_ANIM_PATH_OVERSHOOT,
    SNI_ANIM_PATH_BOUNCE,
    SNI_ANIM_PATH_STEP,
    SNI_ANIM_PATH_CUSTOM_BEZIER3,
    SNI_ANIM_PATH_ENUM_MAX,
} sni_anim_path_builtin_t;

typedef enum
{
    SNI_ANIM_STATE_ACTIVE,
    SNI_ANIM_STATE_PENDING_DELETE,
    SNI_ANIM_STATE_DELETED
} sni_anim_state_t;

typedef struct sni_anim_callback_ctx
{
    lv_anim_t pre_anim;
    lv_anim_t *active_anim;
    jerry_value_t cb_slots[SNI_ANIM_CB_SLOT_COUNT];
    sni_anim_path_kind_t path_kind;
    lv_anim_path_cb_t builtin_path_fn;
    sni_context_t *owner_ctx;
    sni_anim_state_t state;
} sni_anim_callback_ctx_t;

bool sni_cb_anim_create(sni_anim_callback_ctx_t **out_ctx);
bool sni_cb_anim_set_cb(sni_anim_callback_ctx_t *ctx, sni_anim_cb_slot_t slot, jerry_value_t js_cb);
bool sni_cb_anim_set_path_builtin(sni_anim_callback_ctx_t *ctx, sni_anim_path_builtin_t path_kind);
bool sni_cb_anim_set_path_js(sni_anim_callback_ctx_t *ctx, jerry_value_t js_cb);
/**
 * @brief Start animation
 * @param ctx Animation context (created by sni_cb_anim_create)
 * @return bool Whether successful
 */
bool sni_cb_anim_start(sni_anim_callback_ctx_t *ctx);

/**
 * @brief Get the LVGL anim object from context
 * @param ctx Animation context
 * @return Pointer to the active lv_anim_t, or NULL if not started
 */
lv_anim_t *sni_cb_anim_get_lv_anim(sni_anim_callback_ctx_t *ctx);

/**
 * @brief Get the global sni context (shared across callback modules)
 * @return Global sni_context_t pointer
 */
sni_context_t *sni_cb_get_context(void);

void sni_cb_context_cleanup_events(sni_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SNI_CALLBACK_RUNTIME_H */
