/**
 * @file eos_widget_data.h
 * @brief Typed per-widget framework data registry
 *
 * Provides a type-safe mechanism for storing framework-level data on
 * LVGL widgets, replacing ad-hoc use of lv_obj_set_user_data().
 * Each widget can hold multiple independent typed data entries.
 *
 * Internal storage (in lv_obj_set_user_data()):
 *   eos_wdata_container_t { magic, head, registered }
 *   +--- eos_wdata_entry_t { type, data, dtor, next }
 *   +--- eos_wdata_entry_t { type, data, dtor, next }
 *
 * A magic number (0x57445441) in the container distinguishes
 * eos_wdata containers from raw lv_obj_set_user_data() usage.
 * The script engine's sni_control_block_t is stored as a typed
 * entry (EOS_WDATA_SNI_CB) within this unified registry, avoiding
 * user_data slot conflicts between subsystems.
 */

#ifndef EOS_WIDGET_DATA_H
#define EOS_WIDGET_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"

/* Public typedefs --------------------------------------------*/

typedef enum
{
    EOS_WDATA_ACTIVITY = 0,
    EOS_WDATA_LIST_TRANSITION,
    EOS_WDATA_BUBBLE_GRID,
    EOS_WDATA_MSG_LIST,
    EOS_WDATA_MSG_LIST_ITEM,
    EOS_WDATA_TOAST_DURATION,
    EOS_WDATA_RADIO_ITEM_INDEX,
    EOS_WDATA_SLIDER_LABEL,
    EOS_WDATA_SNI_CB, /**< Script engine sni_control_block_t * (replaces raw lv_obj_set_user_data) */
    EOS_WDATA_COUNT
} eos_widget_data_type_t;

typedef void (*eos_wdata_dtor_t)(void *data);

/* Public function prototypes --------------------------------*/

/**
 * @brief Set typed data on a widget
 * @param obj  LVGL widget pointer
 * @param type Data type identifier
 * @param data Data pointer (may be NULL to clear)
 * @param dtor Destructor called when widget is deleted (may be NULL)
 * @note  Replaces existing entry of the same type; dtor is NOT
 *        called on the old value.
 */
void eos_wdata_set(lv_obj_t *obj, eos_widget_data_type_t type, void *data, eos_wdata_dtor_t dtor);

/**
 * @brief Get typed data from a widget
 * @param obj  LVGL widget pointer
 * @param type Data type identifier
 * @return Data pointer, or NULL if not set
 */
void *eos_wdata_get(lv_obj_t *obj, eos_widget_data_type_t type);

/**
 * @brief Remove typed data from a widget (calls dtor if set)
 * @param obj  LVGL widget pointer
 * @param type Data type identifier
 */
void eos_wdata_remove(lv_obj_t *obj, eos_widget_data_type_t type);

/**
 * @brief Remove all typed data from a widget (calls all dtors)
 * @param obj LVGL widget pointer
 */
void eos_wdata_clear(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WIDGET_DATA_H */
