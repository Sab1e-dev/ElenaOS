/**
 * @file eos_card_stack.h
 * @brief Reusable vertically stacked card pager
 */

#ifndef EOS_CARD_STACK_H
#define EOS_CARD_STACK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

/* Public macros ----------------------------------------------*/
#define EOS_CARD_STACK_MAX_SLOTS 8

/* Public typedefs --------------------------------------------*/

typedef struct eos_card_stack_t eos_card_stack_t;
typedef struct eos_card_stack_item_t eos_card_stack_item_t;

/**
 * @brief One visual depth slot in a card stack
 */
typedef struct
{
    lv_coord_t y; /**< Top coordinate of the rendered card */
    uint16_t scale; /**< LVGL transform scale, 256 = 100%% */
    lv_opa_t opacity; /**< Card opacity */
} eos_card_stack_slot_t;

/**
 * @brief Card stack geometry and animation configuration
 */
typedef struct
{
    lv_coord_t card_width; /**< Card width */
    lv_coord_t card_height; /**< Card height */
    lv_coord_t wrapper_width; /**< Wrapper width, zero uses card_width */
    lv_coord_t card_x; /**< Card X inside its wrapper */
    lv_coord_t top; /**< Fallback Y coordinate of the focused card */
    lv_coord_t step; /**< Fallback vertical distance between slots */
    uint8_t slot_count; /**< Number of configured slots, zero uses top/step */
    eos_card_stack_slot_t slots[EOS_CARD_STACK_MAX_SLOTS]; /**< Slot geometry, front slot first */
    eos_card_stack_slot_t previous_slot; /**< Partial slot for the card immediately before focus */
    eos_card_stack_slot_t exit_slot; /**< Off-screen slot used by cards leaving the front */
    uint16_t vertical_drag_factor; /**< Vertical drag damping, 256 = direct, 192 = 75%% */
    uint32_t animation_duration; /**< Settle animation duration in milliseconds */
} eos_card_stack_config_t;

/**
 * @brief Callback invoked after the focused card changes
 * @param stack Card stack
 * @param focus_index New focused card index
 * @param user_data Callback user data
 */
typedef void (*eos_card_stack_focus_changed_cb_t)(eos_card_stack_t *stack, uint32_t focus_index, void *user_data);

/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a card stack container
 * @param parent Parent LVGL object
 * @param config Stack geometry and animation configuration
 * @return New card stack, or NULL on allocation failure
 */
eos_card_stack_t *eos_card_stack_create(lv_obj_t *parent, const eos_card_stack_config_t *config);

/**
 * @brief Add a card to the stack
 * @param stack Card stack
 * @param card Card object; it is reparented into a stack item wrapper
 * @param touch_obj Object receiving stack gestures, usually card itself or a child touch surface
 * @return New stack item, or NULL on failure
 */
eos_card_stack_item_t *eos_card_stack_add(eos_card_stack_t *stack, lv_obj_t *card, lv_obj_t *touch_obj);

/**
 * @brief Remove a card item and animate the remaining cards into place
 * @param item Item to remove; its wrapper and card are deleted asynchronously
 */
void eos_card_stack_remove(eos_card_stack_item_t *item);

/**
 * @brief Delete the stack manager while leaving its parent object intact
 * @param stack Card stack
 */
void eos_card_stack_delete(eos_card_stack_t *stack);

/**
 * @brief Get the stack container
 * @param stack Card stack
 * @return Stack container, or NULL
 */
lv_obj_t *eos_card_stack_get_container(eos_card_stack_t *stack);

/**
 * @brief Get the transparent scroll target used by crown input
 * @param stack Card stack
 * @return Scroll proxy object, or NULL
 */
lv_obj_t *eos_card_stack_get_crown_target(eos_card_stack_t *stack);

/**
 * @brief Get an item's wrapper container
 * @param item Stack item
 * @return Wrapper container, or NULL
 */
lv_obj_t *eos_card_stack_item_get_container(eos_card_stack_item_t *item);

/**
 * @brief Get an item's card object
 * @param item Stack item
 * @return Card object, or NULL
 */
lv_obj_t *eos_card_stack_item_get_card(eos_card_stack_item_t *item);

/**
 * @brief Get the current focused index
 * @param stack Card stack
 * @return Focused card index
 */
uint32_t eos_card_stack_get_focus(eos_card_stack_t *stack);

/**
 * @brief Check whether an item is the current focused card
 * @param stack Card stack
 * @param item Stack item
 * @return true when the item is attached to the stack and focused
 */
bool eos_card_stack_item_is_focused(eos_card_stack_t *stack, eos_card_stack_item_t *item);

/**
 * @brief Check whether the stack is resting exactly in a slot
 * @param stack Card stack
 * @return true when no slot transition is in progress
 */
bool eos_card_stack_is_settled(eos_card_stack_t *stack);

/**
 * @brief Set the focus changed callback
 * @param stack Card stack
 * @param cb Callback, or NULL to clear it
 * @param user_data Callback user data
 */
void eos_card_stack_set_focus_changed_cb(eos_card_stack_t *stack,
                                         eos_card_stack_focus_changed_cb_t cb,
                                         void *user_data);

/**
 * @brief Enable or disable vertical gestures for the stack
 * @param stack Card stack
 * @param enable true to allow vertical slot gestures
 */
void eos_card_stack_set_vertical_gesture_enabled(eos_card_stack_t *stack, bool enable);

/**
 * @brief Set the card and progress for a horizontal side reveal
 * @param stack Card stack
 * @param item Card being revealed horizontally
 * @param progress Progress from 0 to 256
 *
 * The revealed card keeps its slot. Cards before it move toward the exit
 * slot, while cards behind it move one slot deeper and fade out. The value
 * is reversible and does not change the focused index.
 */
void eos_card_stack_set_side_reveal(eos_card_stack_t *stack, eos_card_stack_item_t *item, uint16_t progress);

#ifdef __cplusplus
}
#endif

#endif /* EOS_CARD_STACK_H */
