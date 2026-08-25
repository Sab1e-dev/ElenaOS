/**
 * @file eos_activity.h
 * @brief Activity controller
 */

#ifndef EOS_ACTIVITY_H
#define EOS_ACTIVITY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_core.h"
#include "eos_lang.h"
#include "eos_anim.h"
/* Public macros ----------------------------------------------*/

#define EOS_VIEW_SWITCH_DURATION 300

/* Public typedefs --------------------------------------------*/

typedef struct eos_activity_t eos_activity_t;

typedef enum
{
    EOS_ACTIVITY_TYPE_NULL = 0,
    EOS_ACTIVITY_TYPE_APP,
    EOS_ACTIVITY_TYPE_INPUT_PAGE,
    EOS_ACTIVITY_TYPE_APP_LIST,
    EOS_ACTIVITY_TYPE_WATCHFACE,
    EOS_ACTIVITY_TYPE_WATCHFACE_LIST,
    EOS_ACTIVITY_TYPE_LOCK_SCREEN,
    EOS_ACTIVITY_TYPE_RECENT_APPS,
    EOS_ACTIVITY_TYPE_COUNT
} eos_activity_type_t;

typedef void (*eos_activity_on_enter_t)(eos_activity_t *activity);
typedef void (*eos_activity_on_destroy_t)(eos_activity_t *activity);
typedef void (*eos_activity_on_pause_t)(eos_activity_t *activity);
typedef void (*eos_activity_on_resume_t)(eos_activity_t *activity);
typedef void (*eos_activity_anim_cb_t)(eos_anim_group_t *group, eos_activity_t *this, eos_activity_t *next);

typedef struct
{
    eos_activity_on_enter_t on_enter;
    eos_activity_on_destroy_t on_destroy;
    eos_activity_on_pause_t on_pause;
    eos_activity_on_resume_t on_resume;
} eos_activity_lifecycle_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize Activity controller with root Activity
 * @param root_activity Root Activity (e.g., watchface), cannot be NULL
 * @return eos_result_t EOS_OK success, EOS_FAILED failed
 * @note The root Activity is managed separately from the stack and will not be destroyed by back()
 * @note The root Activity should be created with eos_activity_create_root() which auto-creates the view
 */
eos_result_t eos_activity_controller_init(eos_activity_t *root_activity);

/**
 * @brief Create an Activity
 * @return eos_activity_t* Returns Activity pointer on success, NULL on failure
 */
eos_activity_t *eos_activity_create(const eos_activity_lifecycle_t *lifecycle);

/**
 * @brief Destroy an Activity that is NOT on the activity stack
 * @param activity Activity pointer to destroy
 * @note This is intended for off-stack activities (e.g. temporary test activities).
 *       It calls on_destroy, deletes the view and frees the activity.
 *       Do NOT use this on an activity that is currently on the stack or visible.
 */
void eos_activity_destroy(eos_activity_t *activity);

/**
 * @brief Create a root Activity (watchface) with immediate view creation
 * @param lifecycle Activity lifecycle callbacks
 * @return eos_activity_t* Returns Activity pointer on success, NULL on failure
 *
 * Differences from eos_activity_create():
 * - Always creates view immediately with standard style (no lazy creation)
 * - Designed for root activities (watchfaces) that need custom content
 * - Auto-creates root_screen if not exists (lazy initialization)
 *
 * Usage:
 * 1. Create: eos_activity_create_root(&lifecycle)  // view created immediately
 * 2. Initialize: eos_activity_controller_init(root_activity)
 *
 * Note: Can be called before or after eos_activity_controller_init()
 */
eos_activity_t *eos_activity_create_root(const eos_activity_lifecycle_t *lifecycle);

/**
 * @brief Get Activity user data
 * @param activity Activity pointer
 * @return void* User data pointer, returns NULL on failure
 */
void *eos_activity_get_user_data(eos_activity_t *activity);

/**
 * @brief Set Activity user data
 * @param activity Activity pointer
 * @param user_data User data pointer
 */
void eos_activity_set_user_data(eos_activity_t *activity, void *user_data);

/**
 * @brief Set fault panel for activity (internal use)
 * @param activity Activity pointer
 * @param fault_panel Fault panel pointer
 */
void eos_activity_set_fault_panel(eos_activity_t *activity, void *fault_panel);

/**
 * @brief Get fault panel from activity
 * @param activity Activity pointer
 * @return void* Fault panel pointer, NULL if not set
 */
void *eos_activity_get_fault_panel(eos_activity_t *activity);

/**
 * @brief Get Activity title
 * @param activity Activity pointer
 * @return const char* Title string, returns NULL on failure
 */
const char *eos_activity_get_title(eos_activity_t *activity);

/**
 * @brief Set Activity title
 * @param activity Activity pointer
 * @param title Title string
 */
void eos_activity_set_title(eos_activity_t *activity, const char *title);

/**
 * @brief Set Activity title
 * @param activity Activity pointer
 * @param id Title string ID
 */
void eos_activity_set_title_id(eos_activity_t *activity, lang_string_id_t id);

/**
 * @brief Get Activity title color
 * @param activity Activity pointer
 * @return lv_color_t Title color
 */
lv_color_t eos_activity_get_title_color(eos_activity_t *activity);

/**
 * @brief Set Activity title color
 * @param activity Activity pointer
 * @param color Title color
 */
void eos_activity_set_title_color(eos_activity_t *activity, lv_color_t color);

/**
 * @brief Set Activity page type
 * @param activity Activity pointer
 * @param type Page type
 */
void eos_activity_set_type(eos_activity_t *activity, eos_activity_type_t type);

/**
 * @brief Get Activity page type
 * @param activity Activity pointer
 * @return eos_activity_type_t Page type
 */
eos_activity_type_t eos_activity_get_type(eos_activity_t *activity);

/**
 * @brief Register page transition animation route
 * @param from_type Source page type
 * @param to_type Target page type
 * @param cb Animation callback
 * @return eos_result_t EOS_OK success, EOS_FAILED failed
 */
eos_result_t eos_activity_register_anim_route(eos_activity_type_t from_type,
                                              eos_activity_type_t to_type,
                                              eos_activity_anim_cb_t cb);

/**
 * @brief Query page transition animation route
 * @param from_type Source page type
 * @param to_type Target page type
 * @return eos_activity_anim_cb_t Animation callback, returns NULL if not found
 */
eos_activity_anim_cb_t eos_activity_get_anim_route(eos_activity_type_t from_type, eos_activity_type_t to_type);

/**
 * @brief Set Activity title visibility
 * @param activity Activity pointer
 * @param visible Whether visible
 */
void eos_activity_set_app_header_visible(eos_activity_t *activity, bool visible);

/**
 * @brief Set Activity title visibility with animation
 * @param activity Activity pointer
 * @param visible Whether visible
 * @param duration_ms Animation duration (milliseconds), switches immediately when 0
 */
void eos_activity_set_app_header_visible_animated(eos_activity_t *activity, bool visible, uint32_t duration_ms);

/**
 * @brief Check if Activity title is visible
 * @param activity Activity pointer
 * @return bool true visible, false not visible
 */
bool eos_activity_is_app_header_visible(eos_activity_t *activity);

/**
 * @brief Set whether Activity AppHeader shows only time label
 * @param activity Activity pointer
 * @param time_only true shows only time, false shows full AppHeader
 */
void eos_activity_set_app_header_time_only(eos_activity_t *activity, bool time_only);

/**
 * @brief Check if Activity AppHeader shows only time label
 * @param activity Activity pointer
 * @return bool true shows only time, false shows full AppHeader
 */
bool eos_activity_is_app_header_time_only(eos_activity_t *activity);

/**
 * @brief Set Activity time font color in AppHeader time-only mode
 * @param activity Activity pointer
 * @param color Time font color
 */
void eos_activity_set_app_header_time_only_text_color(eos_activity_t *activity, lv_color_t color);

/**
 * @brief Get Activity time font color in AppHeader time-only mode
 * @param activity Activity pointer
 * @return lv_color_t Time font color
 */
lv_color_t eos_activity_get_app_header_time_only_text_color(eos_activity_t *activity);

/**
 * @brief Get Activity corresponding View
 * @param activity Activity pointer
 * @return lv_obj_t* View object, returns NULL on failure
 */
lv_obj_t *eos_activity_get_view(eos_activity_t *activity);

/**
 * @brief Set Activity View (internal use only)
 * @param activity Activity pointer
 * @param view View object
 * @note This function should only be called inside on_enter callback.
 *       External modification of Activity's view is prohibited to prevent view loss.
 */
void eos_activity_set_view(eos_activity_t *activity, lv_obj_t *view);

/**
 * @brief Find the owning Activity for a widget by walking up the parent tree
 * @param obj Widget object
 * @return eos_activity_t* Owning Activity, or NULL if not found
 */
eos_activity_t *eos_activity_from_widget(lv_obj_t *obj);

/**
 * @brief Get the snapshot container for an Activity
 * @param activity Activity pointer
 * @return lv_obj_t* Snapshot container, or NULL
 */
lv_obj_t *eos_activity_get_snap_container(eos_activity_t *activity);

/**
 * @brief Get root Screen
 * @return lv_obj_t* Root Screen object, returns NULL on failure
 */
lv_obj_t *eos_activity_get_root_screen(void);

/**
 * @brief Replace the root Activity (e.g., switch watchface)
 * @param new_root New root Activity to set
 * @return eos_result_t EOS_OK success, EOS_FAILED failed
 *
 * Lifecycle:
 * 1. If old root exists, call its on_destroy()
 * 2. Set new root as current root
 * 3. Call new root's on_enter()
 * 4. Display new root's view
 *
 * @note The old root Activity is destroyed and should not be used after this call.
 *       The new root Activity must have been created with eos_activity_create().
 */
eos_result_t eos_activity_replace_root(eos_activity_t *new_root);

/**
 * @brief Get current root Activity
 * @return eos_activity_t* Root Activity pointer, returns NULL if not initialized
 */
eos_activity_t *eos_activity_get_root(void);

/**
 * @brief Get Activity view snapshot
 * @param activity Activity pointer
 * @param include_header Whether to include Header (only valid when current Activity and Header is visible)
 * @return lv_obj_t* Snapshot image object (lv_image), returns NULL on failure
 * @note Image resources are automatically released when the object is deleted
 */
lv_obj_t *eos_activity_take_snapshot(eos_activity_t *activity, bool include_header);

/**
 * @brief Get Watchface Activity (deprecated, use eos_activity_get_root instead)
 * @return eos_activity_t* Watchface (root) Activity pointer, returns NULL on failure
 * @deprecated This function is kept for backward compatibility. Use eos_activity_get_root() instead.
 */
eos_activity_t *eos_activity_get_watchface(void);

/**
 * @brief Get current Activity View
 * @return lv_obj_t* Current Activity View object, returns NULL on failure
 */
lv_obj_t *eos_view_active(void);

/**
 * @brief Enter specified Activity
 * @param activity Activity pointer
 */
void eos_activity_enter(eos_activity_t *activity);

/**
 * @brief Return to previous Activity and destroy current Activity
 * @return eos_result_t EOS_OK success, EOS_FAILED failed
 */
eos_result_t eos_activity_back(void);

/**
 * @brief Return directly to root Activity and clear all stacked Activities
 * @return eos_result_t EOS_OK success, EOS_FAILED failed
 * @note This function destroys all Activities in the stack and returns to the root Activity.
 *       The root Activity's on_resume() will be called.
 */
eos_result_t eos_activity_back_to_watchface(void);

/**
 * @brief Wrapper for returning to previous Activity and destroying current Activity
 * @param e Event object
 */
void eos_activity_back_cb(lv_event_t *e);

/**
 * @brief Get current Activity
 * @return eos_activity_t* Current Activity, returns NULL on failure
 */
eos_activity_t *eos_activity_get_current(void);

/**
 * @brief Get current completed display Activity
 * @return eos_activity_t* Completed display Activity, returns NULL on failure
 */
eos_activity_t *eos_activity_get_visible(void);

/**
 * @brief Get previous Activity (used in event callbacks to get the source page)
 * @return eos_activity_t* Previous Activity, returns NULL on failure
 */
eos_activity_t *eos_activity_get_previous(void);

/**
 * @brief Check if Activity transition animation is in progress
 * @return bool true transitioning, false idle
 */
bool eos_activity_is_transition_in_progress(void);

/**
 * @brief Get bottom Activity in stack (or root Activity if stack is empty)
 * @return eos_activity_t* Bottom Activity in stack, or root Activity if stack empty, returns NULL on failure
 */
eos_activity_t *eos_activity_get_bottom(void);

/**
 * @brief Check whether an Activity has ever been entered (has_started)
 *
 * Off-stack Activities created by JS scripts (eos_activity_create(NULL))
 * are never entered and have has_started == false.  Native Activities
 * launched by the app list are entered via eos_activity_enter() and have
 * has_started == true.
 *
 * @param activity Activity pointer
 * @return true if the activity has been entered at least once
 */
bool eos_activity_has_started(eos_activity_t *activity);

/**
 * @brief Take a standalone snapshot of an Activity's view (not tied to an animation window)
 * @param activity Activity pointer
 * @param include_header Whether to include the AppHeader in the snapshot
 * @return lv_draw_buf_t* Owned RGB565 draw buffer, or NULL on failure
 * @note The caller owns the returned draw buffer and must free it via eos_draw_buf_destroy().
 */
lv_draw_buf_t *eos_activity_take_snapshot_standalone(eos_activity_t *activity, bool include_header);

/**
 * @brief Set a pre-captured snapshot on an activity for the resume transition animation
 * @param activity Activity to attach the snapshot to
 * @param snap_buf Pre-captured RGB565 draw buffer (ownership transfers), or NULL to clear
 * @note Set before _activity_switch_to. The APP_LIST→APP callback consumes and clears it.
 */
void eos_activity_set_snap_buf(eos_activity_t *activity, lv_draw_buf_t *snap_buf);

/**
 * @brief Get the pre-captured snapshot from an activity
 * @param activity Activity to query
 * @return lv_draw_buf_t* The stored snapshot, or NULL. Caller does NOT take ownership.
 */
lv_draw_buf_t *eos_activity_get_snap_buf(eos_activity_t *activity);

/**
 * @brief Register a snapshot image for automatic cleanup on animation completion
 * @param snapshot_obj lv_image to auto-delete when the current transition finishes
 * @param draw_buf Draw buffer owned by snapshot_obj (freed on delete)
 * @param owner Activity the snapshot references (held until cleanup)
 * @note Must be called inside an animation callback (active_anim_ctx is set).
 *       Mirrors the registration that eos_activity_take_snapshot performs internally.
 */
void eos_activity_register_snapshot_for_cleanup(lv_obj_t *snapshot_obj, lv_draw_buf_t *draw_buf, eos_activity_t *owner);

/**
 * @brief Detach the app sub-stack from current_activity down to the APP-type root
 * @return eos_activity_t* The sub-stack top (the previous current_activity), or NULL
 * @note Walks app_substack_next chain. Calls on_pause top-down. Removes from main stack.
 *       After call, current_activity is the activity immediately below AppRoot.
 */
eos_activity_t *eos_activity_detach_app_substack(void);

/**
 * @brief Re-attach a previously detached app sub-stack to the main activity stack
 * @param substack_top Topmost activity of the sub-stack (was current at suspend time)
 * @param snap_buf Optional snapshot draw buffer captured at suspend time. If non-NULL,
 *                 an lv_image is created from it on the view for the resume transition
 *                 animation to snapshot. Ownership transfers — the function destroys
 *                 the buffer when done. NULL to use the fallback placeholder.
 * @note Pushes activities bottom-up (AppRoot first). Calls on_resume bottom-up.
 *       After call, current_activity is substack_top.
 */
void eos_activity_reattach_app_substack(eos_activity_t *substack_top, lv_draw_buf_t *snap_buf);

/**
 * @brief Set the suspend_on_exit flag on an activity (park instead of destroy after transition)
 * @param activity Activity pointer
 * @param suspend_on_exit Whether to park the activity on exit
 */
void eos_activity_set_suspend_on_exit(eos_activity_t *activity, bool suspend_on_exit);

/**
 * @brief Check if an activity is suspended (parked in recents registry)
 * @param activity Activity pointer
 * @return true if suspended
 */
bool eos_activity_is_suspended(eos_activity_t *activity);

/**
 * @brief Mark an activity as suspended
 * @param activity Activity pointer
 * @param suspended Suspended state
 */
void eos_activity_set_suspended(eos_activity_t *activity, bool suspended);

/**
 * @brief Set the app_substack_next link (next activity toward app root, stack-down direction)
 * @param activity Activity pointer
 * @param next The next activity in the substack chain (closer to AppRoot), or NULL
 */
void eos_activity_set_app_substack_next(eos_activity_t *activity, eos_activity_t *next);

/**
 * @brief Get the app_substack_next link
 * @param activity Activity pointer
 * @return eos_activity_t* Next in substack chain, or NULL
 */
eos_activity_t *eos_activity_get_app_substack_next(eos_activity_t *activity);

/**
 * @brief Set the app_root reference for this activity
 * @param activity Activity pointer
 * @param app_root The APP-type root activity of this app
 */
void eos_activity_set_app_root(eos_activity_t *activity, eos_activity_t *app_root);

/**
 * @brief Get the app_root reference
 * @param activity Activity pointer
 * @return eos_activity_t* The app root, or NULL
 */
eos_activity_t *eos_activity_get_app_root(eos_activity_t *activity);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ACTIVITY_H */
