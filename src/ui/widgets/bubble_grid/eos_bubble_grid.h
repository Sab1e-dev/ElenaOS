/**
 * @file eos_bubble_grid.h
 * @brief Bubble grid
 */

#ifndef EOS_BUBBLE_GRID_H
#define EOS_BUBBLE_GRID_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef struct
{
    uint32_t index;
    void *icon_user_data;
} eos_bubble_click_event_t;

typedef struct
{
    int16_t bubble_size_px; /**< Base bubble radius in pixels. */
    int16_t row_pitch_x_px; /**< Horizontal pitch inside a row. */
    int16_t row_pitch_y_px; /**< Vertical pitch between rows. */
    int16_t x_radius_px; /**< Horizontal radius of the central full-scale region. */
    int16_t y_radius_px; /**< Vertical radius for edge scaling decisions. */
    int16_t corner_radius_px; /**< Corner fillet radius for smooth edge transition. */
    int16_t tap_move_tolerance_px; /**< Allowed finger jitter for tap detection. */
    uint16_t press_scale_permille; /**< Scale factor when an icon is pressed. */
    lv_opa_t press_darken_lvl; /**< Bubble darkening intensity when pressed. */
    lv_opa_t press_image_darken_lvl; /**< Image-layer darkening intensity when pressed. */
    int16_t press_neighbor_pull_px; /**< Max pull displacement applied to nearby icons while pressed. */
    int16_t press_neighbor_radius_px; /**< Radius of neighboring icons affected by press pull. */
    uint16_t press_anim_in_speed_permille; /**< Per-frame approach speed for press-in animation. */
    uint16_t press_anim_out_speed_permille; /**< Per-frame recovery speed for release/cancel animation. */

    uint16_t x_drag_factor_permille; /**< Horizontal drag sensitivity. 1000 = 1.0x finger delta. */
    uint16_t x_inertia_damp_permille; /**< Horizontal inertia damping per tick. Smaller stops faster. */
    int16_t max_x_offset_px; /**< Maximum horizontal offset in pixels before clamping. */
    uint16_t max_scale_permille; /**< Maximum icon scale in permille. 1000 = 1.0x. */
    uint16_t min_scale_permille; /**< Minimum icon scale in permille for edge regions. */
    int16_t fringe_width_px; /**< Edge transition width in pixels from max to min scale. */
    int16_t y_overscroll_max_px; /**< Vertical rubber-band compression limit in pixels. */
    uint16_t y_spring_k_permille; /**< Vertical spring strength when settling. */
    uint16_t y_spring_damp_permille; /**< Vertical spring damping per tick. */
    uint16_t x_spring_k_permille; /**< Horizontal spring strength back to center. */
    uint16_t x_spring_damp_permille; /**< Horizontal spring damping per tick. */
    int16_t y_ratchet_step_px; /**< Vertical snap step in pixels. */
    uint16_t y_ratchet_pull_permille; /**< Snap attraction while dragging within valid range. */
    uint16_t y_ratchet_snap_permille; /**< Snap attraction after release near target step. */
    int16_t y_ratchet_dead_px; /**< Dead-zone threshold in pixels for spring-vs-snap mode. */
} eos_bubble_config_t;

/* Public function prototypes --------------------------------*/

/**
 * @brief Fill config with built-in default values.
 * @param config The configuration to initialize.
 */
void eos_bubble_init_config(eos_bubble_config_t *config);

/**
 * @brief Create a watch bubble component.
 * @param parent The parent object.
 * @return Returns the component root object.
 */
lv_obj_t *eos_bubble_create(lv_obj_t *parent);

/**
 * @brief Set icon source for one bubble index.
 * @param obj The watch bubble component.
 * @param index The bubble index.
 * @param src The icon source.
 */
void eos_bubble_set_icon_src(lv_obj_t *obj, uint32_t index, const void *src);

/**
 * @brief Set user data for one bubble index.
 * @param obj The watch bubble component.
 * @param index The bubble index.
 * @param user_data The user data.
 */
void eos_bubble_set_icon_user_data(lv_obj_t *obj, uint32_t index, void *user_data);

/**
 * @brief Dynamically update drag/scale/ratchet physics config.
 * @param obj The watch bubble component.
 * @param config The new configuration.
 */
void eos_bubble_set_config(lv_obj_t *obj, const eos_bubble_config_t *config);

/**
 * @brief Get current drag/scale/ratchet physics config.
 * @param obj The watch bubble component.
 * @param config Output buffer to receive current configuration.
 */
void eos_bubble_get_config(lv_obj_t *obj, eos_bubble_config_t *config);

/**
 * @brief Set bubble background color for one icon index.
 *        Creates the bubble object if needed. No image is displayed.
 * @param obj The watch bubble component.
 * @param index The bubble index.
 * @param color The background color.
 */
void eos_bubble_set_icon_color(lv_obj_t *obj, uint32_t index, lv_color_t color);

/**
 * @brief Get bubble object by icon index.
 * @param obj The watch bubble component.
 * @param index Bubble index.
 * @return Returns the bubble object of the index, or NULL if unavailable.
 */
lv_obj_t *eos_bubble_get_icon_obj(lv_obj_t *obj, uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* EOS_BUBBLE_GRID_H */
