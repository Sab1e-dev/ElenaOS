/**
 * @file eos_anim.h
 * @brief Animation library
 * @details
 *
 * # Animation System
 *
 * An animation library wrapped based on LVGL animation, providing unified and easy-to-use animation interfaces for ElenixOS.
 * This library summarizes common animation effects and standardizes them, representing them with the enum type `eos_anim`,
 * facilitating consistent creation and usage of standard animations throughout the system.
 *
 * ## Usage
 *
 * ### Create different types of animations using dedicated functions, such as scale animation and fade animation:
 *
 * ```c
 * eos_anim_t *anim = eos_anim_scale_create(obj, w_start, w_end, h_start, h_end, duration);
 * eos_anim_t *fade_anim = eos_anim_fade_create(obj, opa_start, opa_end, duration);
 * ```
 *
 * ### Callbacks triggered when animation playback completes:
 *
 * ```c
 * eos_anim_add_cb(anim, user_cb, user_data);
 * ```
 *
 * ### Start animation
 *
 * ```c
 * eos_anim_start(anim);
 * ```
 *
 * ### Shortcuts (create and play directly)
 *
 * ```c
 * eos_anim_scale_start(obj, w_start, w_end, h_start, h_end, duration);
 * ```
 *
 *  - No need to manually manage animation objects.
 *  - Automatically released after completion.
 *
 * ### Delete animation
 *
 * If you want to stop early or manually clean up the animation:
 *
 * ```c
 * eos_anim_del(anim);
 * ```
 *
 * ### Snapshot Backend
 *
 * For complex widget trees, use the snapshot backend to improve animation performance.
 * The engine takes a one-time raster snapshot of the widget, then animates the flat image.
 * If memory is insufficient, it silently falls back to direct animation.
 *
 * ```c
 * eos_anim_t *anim = eos_anim_scale_create(complex_page, 0, 390, 0, 450, 300, false);
 * eos_anim_set_backend(anim, EOS_ANIM_BACKEND_SNAPSHOT);
 * eos_anim_start(anim);
 * ```
 *
 * ### Repeat and Playback
 *
 * ```c
 * eos_anim_t *anim = eos_anim_move_create(obj, -12, 0, 12, 0, 60, false);
 * eos_anim_set_repeat_count(anim, 3);
 * eos_anim_set_playback_time(anim, 60);
 * eos_anim_start(anim);
 * ```
 *
 * ## Notes
 *
 *  - Animation object eos_anim_t will be automatically released.
 *  - Supports multiple animations in parallel, managed by anim_count for sub-animation completion.
 *  - Extensible for different types of animations (scale, opacity, size, position, etc.).
 *  - Multiple animations can start simultaneously, but same type animations may have race conditions.
 */

#ifndef EOS_ANIM_H
#define EOS_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/**
 * @brief All animation types, can continue to add more.
 */
typedef enum
{
    EOS_ANIM_SCALE, /**< Scale animation */
    EOS_ANIM_FADE, /**< Opacity fade animation */
    EOS_ANIM_MOVE, /**< Position move animation */
    EOS_ANIM_TRANSFORM_SCALE, /**< Scale animation, supports Label */
    EOS_ANIM_IMAGE_SCALE, /**< Pixel-level image scale animation */
    EOS_ANIM_RESIZE /**< Width/height resize (independent axis) */
} eos_anim;

/**
 * @brief Animation backend type
 */
typedef enum
{
    EOS_ANIM_BACKEND_DIRECT = 0, /**< Animate the widget directly (default) */
    EOS_ANIM_BACKEND_SNAPSHOT, /**< Take snapshot, animate the flat image instead */
} eos_anim_backend_type_t;

typedef struct eos_anim_t eos_anim_t; // Forward declaration
typedef struct eos_anim_group_t eos_anim_group_t; // Forward declaration

/**
 * @brief Callback function type definition for individual animation completion
 */
typedef void (*eos_anim_cb_t)(eos_anim_t *a);

/**
 * @brief Callback function type definition for animation group completion
 */
typedef void (*eos_anim_group_cb_t)(void *user_data);

/**
 * @brief Animation group - tracks completion across multiple eos_anim objects
 */
struct eos_anim_group_t
{
    uint32_t expected; /**< Total expected animations in the group */
    uint32_t completed; /**< Number of animations that have completed */
    eos_anim_group_cb_t callback; /**< Group completion callback */
    void *user_data; /**< User data for group callback */
};

/**
 * @brief ElenixOS animation object structure
 */
struct eos_anim_t
{
    eos_anim type; /**< Animation type */
    uint32_t anim_count; /**< Total animation count for this type */
    uint32_t anim_completed_count; /**< Count of completed sub-anims (detects when all finished) */
    eos_anim_cb_t user_cb; /**< User-defined callback function */
    lv_obj_t *tar_obj;
    bool auto_delete_obj; /**< Automatically delete bound object when animation completes */
    void *user_data; /**< User data */
    uint32_t delay; /**< Delay before animation starts (ms) */
    eos_anim_group_t *group; /**< Parent group (NULL = standalone) */
    bool no_blocker; /**< Disable blocker overlay management for this anim */
    bool preserve_layout; /**< Keep widget visible (opa=0) instead of hidden for snapshot, preserving layout */
    lv_opa_t saved_orig_opa; /**< Original widget opacity to restore after snapshot */
    eos_anim_backend_type_t backend_type; /**< Requested animation backend */
    lv_draw_buf_t *snap_buf; /**< Snapshot backend: raster buffer */
    lv_obj_t *snap_image; /**< Snapshot backend: lv_image for animating */
    uint16_t repeat_count; /**< Repeat count (0 = play once) */
    uint32_t playback_time; /**< Playback (reverse) duration in ms (0 = no playback) */
    union
    { /**< Union for storing animation objects */
        struct
        {
            lv_anim_t a_width; /**< Scale animation width animation object */
            lv_anim_t a_height; /**< Scale animation height animation object */
        } scale;
        struct
        {
            lv_anim_t a_opa;
        } fade;
        struct
        {
            lv_anim_t a_x; /**< X-axis position animation */
            lv_anim_t a_y; /**< Y-axis position animation */
        } move;
        struct
        {
            lv_anim_t a_scale;
        } transform_scale;
        struct
        {
            lv_anim_t a_scale;
        } image_scale;
        struct
        {
            lv_anim_t a_w;
            lv_anim_t a_h;
        } resize;
    } anim;
    union
    { /**< Union for storing configuration */
        struct
        {
            bool layered; /**< Whether to use layered opacity */
            bool main_opa; /**< Whether to use main opacity (lv_obj_set_style_opa) */
        } fade;
        struct
        {
            bool disable_x;
            bool disable_y;
        } move;
        struct
        {
            bool disable_w;
            bool disable_h;
        } resize;
    } cfg;
};
/* Public function prototypes --------------------------------*/

/************************** Common **************************/

void eos_anim_set_auto_delete(eos_anim_t *anim);
bool eos_anim_start(eos_anim_t *anim);
void eos_anim_add_cb(eos_anim_t *anim, eos_anim_cb_t user_cb, void *user_data);
void *eos_anim_get_user_data(eos_anim_t *anim);
void eos_anim_del(eos_anim_t *anim);
void eos_anim_blocker_show(void);
void eos_anim_blocker_hide(void);
void eos_anim_set_backend(eos_anim_t *anim, eos_anim_backend_type_t type);
void eos_anim_set_delay(eos_anim_t *anim, uint32_t delay);

/**
 * @brief Set repeat count (0 = play once, default)
 */
void eos_anim_set_repeat_count(eos_anim_t *anim, uint16_t count);

/**
 * @brief Set playback (reverse) time in ms (0 = no reverse, default)
 */
void eos_anim_set_playback_time(eos_anim_t *anim, uint32_t time_ms);

/**
 * @brief Disable blocker overlay management for this animation
 */
void eos_anim_set_no_blocker(eos_anim_t *anim, bool no_blocker);

/**
 * @brief Keep original widget in layout during snapshot animation.
 * Instead of hiding (LV_OBJ_FLAG_HIDDEN), sets opacity to 0 so flex/grid containers won't re-flow.
 */
void eos_anim_set_preserve_layout(eos_anim_t *anim, bool preserve);

/**
 * @brief Override the default easing path for this animation
 */
void eos_anim_set_path(eos_anim_t *anim, lv_anim_path_cb_t path_cb);

/************************** Animation Group **************************/

eos_anim_group_t *eos_anim_group_create(eos_anim_group_cb_t cb, void *user_data);
void eos_anim_group_del(eos_anim_group_t *group);
void eos_anim_group_attach(eos_anim_t *anim, eos_anim_group_t *group);

/************************** Animation **************************/

eos_anim_t *eos_anim_scale_create(lv_obj_t *tar_obj,
                                  int32_t w_start,
                                  int32_t w_end,
                                  int32_t h_start,
                                  int32_t h_end,
                                  uint32_t duration,
                                  bool auto_delete);
void eos_anim_scale_start(lv_obj_t *tar_obj,
                          int32_t w_start,
                          int32_t w_end,
                          int32_t h_start,
                          int32_t h_end,
                          uint32_t duration,
                          bool auto_delete);

eos_anim_t *eos_anim_fade_create(lv_obj_t *tar_obj,
                                 int32_t opa_start,
                                 int32_t opa_end,
                                 uint32_t duration,
                                 bool auto_delete);
void eos_anim_fade_start(lv_obj_t *tar_obj, int32_t opa_start, int32_t opa_end, uint32_t duration, bool auto_delete);
void eos_anim_fade_set_layered(eos_anim_t *a, bool layered);
void eos_anim_fade_set_main_opa(eos_anim_t *a, bool enabled);

eos_anim_t *eos_anim_move_create(lv_obj_t *tar_obj,
                                 int32_t start_x,
                                 int32_t start_y,
                                 int32_t end_x,
                                 int32_t end_y,
                                 uint32_t duration,
                                 bool auto_delete);
void eos_anim_move_start(lv_obj_t *tar_obj,
                         int32_t start_x,
                         int32_t start_y,
                         int32_t end_x,
                         int32_t end_y,
                         uint32_t duration,
                         bool auto_delete);

eos_anim_t *eos_anim_transform_scale_create(lv_obj_t *tar_obj,
                                            int32_t scale_start,
                                            int32_t scale_end,
                                            uint32_t duration,
                                            bool auto_delete);
void eos_anim_transform_scale_start_ex(lv_obj_t *tar_obj,
                                       int32_t scale_start,
                                       int32_t scale_end,
                                       uint32_t duration,
                                       uint32_t playback_time,
                                       uint16_t repeat_count,
                                       bool auto_delete);
void eos_anim_transform_scale_start(lv_obj_t *tar_obj,
                                    int32_t scale_start,
                                    int32_t scale_end,
                                    uint32_t duration,
                                    bool auto_delete);

/**
 * @brief Create image scale animation (pixel-level, via lv_image_set_scale)
 */
eos_anim_t *eos_anim_image_scale_create(lv_obj_t *tar_obj,
                                        int32_t scale_start,
                                        int32_t scale_end,
                                        uint32_t duration,
                                        bool auto_delete);
void eos_anim_image_scale_start(lv_obj_t *tar_obj,
                                int32_t scale_start,
                                int32_t scale_end,
                                uint32_t duration,
                                bool auto_delete);

/**
 * @brief Create resize animation (width/height independently)
 */
eos_anim_t *eos_anim_resize_create(lv_obj_t *tar_obj,
                                    int32_t w_start,
                                    int32_t w_end,
                                    int32_t h_start,
                                    int32_t h_end,
                                    uint32_t duration,
                                    bool auto_delete);
void eos_anim_resize_start(lv_obj_t *tar_obj,
                            int32_t w_start,
                            int32_t w_end,
                            int32_t h_start,
                            int32_t h_end,
                            uint32_t duration,
                            bool auto_delete);

/************************** Snapshot Batch **************************/

/**
 * @brief Begin batch-snapshot mode for snapshot-backend animations.
 *
 * When batch mode is active, _snapshot_backend_prepare will create and
 * position the snapshot image, but defer hiding the original widget
 * and defer the lv_refr_now() call.
 *
 * Call eos_anim_snapshot_batch_flush() after all snapshot-backend
 * animations have been eos_anim_start()'ed.  This hides all original
 * widgets at once, does a single lv_refr_now(), and lets the animations
 * proceed naturally.
 *
 * Use with eos_list_transition_play (forward direction) to prevent
 * per-item lv_refr_now interleaving which causes black-flash on
 * SiFli hardware with partial-refresh displays.
 */
void eos_anim_snapshot_batch_begin(void);
void eos_anim_snapshot_batch_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ANIM_H */
