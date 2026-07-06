/**
 * @file eos_swipe_panel.h
 * @brief Swipe panel header file
 */

#ifndef EOS_SWIPE_PANEL_H
#define EOS_SWIPE_PANEL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_core.h"
#include "lvgl.h"
#include "eos_slide_widget.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
/**
 * @brief SwipePanel drag direction
 */
typedef enum
{
    EOS_SWIPE_DIR_UP = 0, // Slide up to reveal swipe_obj
    EOS_SWIPE_DIR_DOWN = 1, // Slide down to reveal swipe_obj
    EOS_SWIPE_DIR_LEFT = 2, // Slide left to reveal swipe_obj
    EOS_SWIPE_DIR_RIGHT = 3, // Slide right to reveal swipe_obj
    EOS_SWIPE_DIR_COUNT // Total number of directions
} eos_swipe_dir_t;
/**
 * @brief SwipePanel structure definition
 */
typedef struct
{
    eos_slide_widget_t *sw;
    lv_obj_t *swipe_obj;
    lv_obj_t *handle_bar;
    eos_swipe_dir_t dir;
} eos_swipe_panel_t;
/* Public function prototypes --------------------------------*/

/**
 * @brief Slide the swipe panel into screen
 * @param sp Target swipe panel
 */
void eos_swipe_panel_slide_down(eos_swipe_panel_t *sp);
/**
 * @brief Delete SwipePanel
 * @param swipe_panel Swipe panel
 */
void eos_swipe_panel_delete(eos_swipe_panel_t *swipe_panel);

/**
 * @brief Create SwipePanel
 * @param parent Parent object of the swipe panel
 * @return Pointer to the created SwipePanel
 * @note Must be deleted with eos_swipe_panel_delete when not in use, otherwise memory leak may occur
 */
eos_swipe_panel_t *eos_swipe_panel_create(lv_obj_t *parent);

/**
 * @brief Set drag direction
 * @param swipe_panel Swipe panel
 * @param dir Drag direction, e.g., EOS_SWIPE_DIR_DOWN means dragging down to reveal swipe_obj
 */
void eos_swipe_panel_set_dir(eos_swipe_panel_t *swipe_panel, const eos_swipe_dir_t dir);

/**
 * @brief Hide HandleBar (small white bar)
 * @param swipe_panel Swipe panel
 */
void eos_swipe_panel_hide_handle_bar(eos_swipe_panel_t *swipe_panel);

/**
 * @brief Show HandleBar (small white bar)
 * @param swipe_panel Swipe panel
 */
void eos_swipe_panel_show_handle_bar(eos_swipe_panel_t *swipe_panel);

/**
 * @brief Externally trigger pull-back animation, automatically pull back outside screen
 * @param swipe_panel Swipe panel
 */
void eos_swipe_panel_pull_back(eos_swipe_panel_t *swipe_panel);

/**
 * @brief Trigger pull animation, pull swipe panel to specified pixel position (absolute coordinates)
 *
 * When swipe panel is horizontal, target controls x-axis coordinate
 *
 * When swipe panel is vertical, target controls y-axis coordinate
 * @param swipe_panel Swipe panel
 * @param target Target position in pixels
 * @param anim Whether to enable animation
 */
void eos_swipe_panel_move(eos_swipe_panel_t *swipe_panel, int32_t target, bool anim);
/**
 * @brief Show swipe panel
 */
void eos_swipe_panel_show(eos_swipe_panel_t *sp);
/**
 * @brief Hide swipe panel
 */
void eos_swipe_panel_hide(eos_swipe_panel_t *sp);
#ifdef __cplusplus
}
#endif

#endif /* EOS_SWIPE_PANEL_H */
