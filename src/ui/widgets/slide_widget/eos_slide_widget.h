/**
 * @file eos_slide_widget.h
 * @brief Slide widget - A reusable sliding gesture controller
 *
 * This widget handles touch gestures and animates target objects accordingly.
 * It supports both vertical and horizontal sliding with configurable thresholds.
 */

#ifndef EOS_SLIDE_WIDGET_H
#define EOS_SLIDE_WIDGET_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_utils.h"

/* Public macros ----------------------------------------------*/
#define EOS_THRESHOLD_SCALE 256
/* Public typedefs --------------------------------------------*/

typedef uint32_t eos_threshold_t;

enum
{
    EOS_THRESHOLD_0 = 0,
    EOS_THRESHOLD_10 = 25,
    EOS_THRESHOLD_20 = 51,
    EOS_THRESHOLD_30 = 76,
    EOS_THRESHOLD_40 = 102,
    EOS_THRESHOLD_50 = 127,
    EOS_THRESHOLD_60 = 153,
    EOS_THRESHOLD_70 = 178,
    EOS_THRESHOLD_80 = 204,
    EOS_THRESHOLD_100 = 255,
    EOS_THRESHOLD_INFINITE = INT_MAX,
};

typedef enum
{
    EOS_SLIDE_DIR_VER,
    EOS_SLIDE_DIR_HOR,
} eos_slide_widget_dir_t;

typedef enum
{
    EOS_SLIDE_WIDGET_STATE_IDLE = 0, /**< Idle, not sliding */
    EOS_SLIDE_WIDGET_STATE_DRAGGING, /**< Currently sliding (gesture drag) */
    EOS_SLIDE_WIDGET_STATE_THRESHOLD, /**< Exceeded threshold, slide confirmed (execute expand/trigger operation) */
    EOS_SLIDE_WIDGET_STATE_REVERTING, /**< Reverting (auto return when threshold not exceeded) */
    EOS_SLIDE_WIDGET_STATE_ANIMATING, /**< Manually triggered animation */
    EOS_SLIDE_WIDGET_STATE_OPEN, /**< Panel fully open */
} eos_slide_widget_state_t;

/**
 * @brief Slide widget configuration structure
 */
typedef struct
{
    lv_coord_t target_base; /**< Base position for target object */
    lv_coord_t target_target; /**< Target position for target object */
    lv_coord_t touch_base; /**< Base position for touch object (auto-sync) */
    lv_coord_t touch_target; /**< Target position for touch object (auto-sync) */
    eos_threshold_t threshold; /**< Threshold for triggering slide */
    bool sync_touch_obj; /**< Whether to auto-sync touch object position */
} eos_slide_widget_config_t;

typedef struct eos_slide_widget_t eos_slide_widget_t;

/* Public function prototypes --------------------------------*/

/**
 * @brief Initialize slide widget component (register event IDs)
 * @note Should be called during system initialization
 */
void eos_slide_widget_init(void);

/*============================ Creation & Deletion ============================*/

/**
 * @brief Create slide widget
 * @param parent        Parent object of the touch object
 * @param target_obj    Target object (will slide following the touch area)
 * @param dir           Slide direction (only supports vertical `EOS_SLIDE_DIR_VER` and horizontal `EOS_SLIDE_DIR_HOR`)
 * @param target        Target coordinate point (`target` refers to y-axis when `dir` is vertical; x-axis when horizontal)
 *
 * When the threshold `abs(current_coord - touch_start_coord)/abs(target-base)` is exceeded,
 * the full pull-out animation is triggered, automatically pulling from the target object's current position to target position.
 * @param threshold Threshold (0~255) see `EOS_THRESHOLD_XXX`
 * @return eos_slide_widget_t* Returns slide widget object on success, NULL on failure
 * @warning Do not use `lv_obj_center` on the target object, otherwise coordinate movement will be chaotic.
 */
eos_slide_widget_t *eos_slide_widget_create(lv_obj_t *parent,
                                            lv_obj_t *target_obj,
                                            eos_slide_widget_dir_t dir,
                                            lv_coord_t target,
                                            eos_threshold_t threshold);

/**
 * @brief Create slide widget (without creating touch object internally)
 * @param touch_obj Touch object
 */
eos_slide_widget_t *eos_slide_widget_create_with_touch(lv_obj_t *touch_obj,
                                                       lv_obj_t *target_obj,
                                                       eos_slide_widget_dir_t dir,
                                                       lv_coord_t target,
                                                       eos_threshold_t threshold);

/**
 * @brief Delete slide widget
 * @param sw Slide widget to delete
 */
void eos_slide_widget_delete(eos_slide_widget_t *sw);

/*============================ Configuration ============================*/

/**
 * @brief Configure slide widget with a configuration structure
 * @param sw Slide widget
 * @param config Configuration structure pointer
 */
void eos_slide_widget_configure(eos_slide_widget_t *sw, const eos_slide_widget_config_t *config);

/**
 * @brief Set the target object for slide widget
 * @param sw Slide widget
 * @param target_obj New target object
 */
void eos_slide_widget_set_target_obj(eos_slide_widget_t *sw, lv_obj_t *target_obj);

/**
 * @brief Set the slide range for target object
 * @param sw Slide widget
 * @param base Base position (starting point)
 * @param target Target position (ending point)
 */
void eos_slide_widget_set_range(eos_slide_widget_t *sw, lv_coord_t base, lv_coord_t target);

/**
 * @brief Set the position range for touch object (for auto-sync)
 * @param sw Slide widget
 * @param base Base position for touch object
 * @param target Target position for touch object
 * @note When set, touch object position will be automatically synced during animation
 */
void eos_slide_widget_set_touch_range(eos_slide_widget_t *sw, lv_coord_t base, lv_coord_t target);

/**
 * @brief Enable or disable touch object position sync
 * @param sw Slide widget
 * @param enable true to enable auto-sync, false to disable
 */
void eos_slide_widget_set_sync_touch_obj(eos_slide_widget_t *sw, bool enable);

/**
 * @brief Set threshold for slide trigger
 * @param sw Slide widget
 * @param threshold Threshold value (0~255), use EOS_THRESHOLD_XXX macros
 */
void eos_slide_widget_set_threshold(eos_slide_widget_t *sw, eos_threshold_t threshold);

/**
 * @brief Set whether to support bidirectional sliding
 * @param sw Slide widget
 * @param enable true to enable bidirectional sliding
 */
void eos_slide_widget_set_bidirectional(eos_slide_widget_t *sw, bool enable);

/**
 * @brief Set whether to move to foreground when pressed
 * @param sw Slide widget
 * @param enable true to move to foreground on press
 */
void eos_slide_widget_set_move_foreground_on_pressed(eos_slide_widget_t *sw, bool enable);

/**
 * @brief Set animation transition states
 * @param sw Slide widget
 * @param transit_state Transition state during animation
 * @param settle_state Final state after animation completes
 */
void eos_slide_widget_set_anim_transition(eos_slide_widget_t *sw,
                                          eos_slide_widget_state_t transit_state,
                                          eos_slide_widget_state_t settle_state);

/*============================ State Query ============================*/

/**
 * @brief Get current state of slide widget
 * @param sw Slide widget
 * @return Current state
 */
eos_slide_widget_state_t eos_slide_widget_get_state(eos_slide_widget_t *sw);

/**
 * @brief Get last touch displacement
 * @param sw Slide widget
 * @return Last touch displacement value
 */
lv_coord_t eos_slide_widget_get_displacement(eos_slide_widget_t *sw);

/**
 * @brief Get current position of target object
 * @param sw Slide widget
 * @return Current position
 */
lv_coord_t eos_slide_widget_get_current_pos(eos_slide_widget_t *sw);

/**
 * @brief Get base position
 * @param sw Slide widget
 * @return Base position
 */
lv_coord_t eos_slide_widget_get_base(eos_slide_widget_t *sw);

/**
 * @brief Get target position
 * @param sw Slide widget
 * @return Target position
 */
lv_coord_t eos_slide_widget_get_target(eos_slide_widget_t *sw);

/**
 * @brief Check if slide widget is reversed
 * @param sw Slide widget
 * @return true if reversed, false otherwise
 */
bool eos_slide_widget_is_reversed(eos_slide_widget_t *sw);

/**
 * @brief Get touch object
 * @param sw Slide widget
 * @return Touch object pointer
 */
lv_obj_t *eos_slide_widget_get_touch_obj(eos_slide_widget_t *sw);

/**
 * @brief Get target object
 * @param sw Slide widget
 * @return Target object pointer
 */
lv_obj_t *eos_slide_widget_get_target_obj(eos_slide_widget_t *sw);

/**
 * @brief Get slide direction
 * @param sw Slide widget
 * @return Slide direction
 */
eos_slide_widget_dir_t eos_slide_widget_get_dir(eos_slide_widget_t *sw);

/*============================ Actions ============================*/

/**
 * @brief Move from start position to end position
 * @param sw Target slide widget
 * @param start Start coordinate
 * @param end End coordinate
 * @param duration Duration, set to 0 for no animation
 */
void eos_slide_widget_move(eos_slide_widget_t *sw, lv_coord_t start, lv_coord_t end, uint32_t duration);

/**
 * @brief Reverse the `target` and `base` positions of the target object
 * @param sw Target slide widget
 */
void eos_slide_widget_reverse(eos_slide_widget_t *sw);

/**
 * @brief Sync touch object position based on current target object position
 * @param sw Slide widget
 * @note This is automatically called during animation if sync_touch_obj is enabled
 */
void eos_slide_widget_sync_touch_obj(eos_slide_widget_t *sw);

/*============================ Event Callbacks ============================*/

/**
 * @brief Register callback for slide widget events
 * @param sw Slide widget
 * @param cb Callback function (lv_event_cb_t)
 * @param user_data User data passed to callback
 */
void eos_slide_widget_add_event_cb_reached_threshold(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data);
void eos_slide_widget_add_event_cb_reverted(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data);
void eos_slide_widget_add_event_cb_moving(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data);
void eos_slide_widget_add_event_cb_done(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data);
void eos_slide_widget_add_event_cb_opened(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data);
void eos_slide_widget_add_event_cb_closed(eos_slide_widget_t *sw, lv_event_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SLIDE_WIDGET_H */
