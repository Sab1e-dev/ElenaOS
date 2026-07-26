/**
 * @file sni_type_bridge.h
 * @brief Type bridge
 */

#ifndef SNI_TYPE_BRIDGE_H
#define SNI_TYPE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "jerryscript.h"
#include "sni_types.h"
/* Public macros ----------------------------------------------*/

#define sni_tb_c2js_number jerry_number
#define sni_tb_js2c_number jerry_value_as_number
#define sni_tb_js2c_integer jerry_value_as_integer
#define sni_tb_js2c_int32 jerry_value_as_int32
#define sni_tb_js2c_uint32 jerry_value_as_uint32

#define sni_tb_c2js_boolean jerry_boolean
#define sni_tb_js2c_boolean jerry_value_to_boolean

#define sni_tb_c2js_string jerry_string_sz

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Convert JerryScript string to C string
 * @param js_val JerryScript string value
 * @return const char* C string pointer
 * @note Caller must use `eos_free` to free the returned string memory
 */
const char *sni_tb_js2c_string(jerry_value_t js_val);

/**
 * @brief Convert JerryScript value to C value or value object
 * @param js_val JerryScript value
 * @param type Target type
 * @param out_obj Output value pointer (if output value is a pointer, should pass its address, i.e., double pointer)
 * @return bool Whether conversion was successful
 */
bool sni_tb_js2c(jerry_value_t js_val, sni_type_t type, void *out_obj);

/**
 * @brief Extract raw pointer and actual type from any Handle object
 * @param js_val JerryScript handle object
 * @param out_obj Output raw pointer
 * @param out_type Output handle actual type, can be NULL
 * @return bool Whether extraction was successful
 */
bool sni_tb_js2c_any_handle(jerry_value_t js_val, void *out_obj, sni_type_t *out_type);

/**
 * @brief Convert C value or value object to JerryScript value
 * @param c_val C value pointer (if input value is a pointer, should pass its address, i.e., double pointer)
 * @param type Source type
 * @return jerry_value_t JerryScript value
 */
jerry_value_t sni_tb_c2js(void *c_val, sni_type_t type);

/**
 * @brief Write C value object or handle to existing JerryScript object
 * @param c_val C value pointer (if input value is a pointer, should pass its address, i.e., double pointer)
 * @param type Source type
 * @param js_obj Target JerryScript object
 * @return bool Whether writing was successful
 */
bool sni_tb_c2js_set_object(void *c_val, sni_type_t type, jerry_value_t js_obj);

/**
 * @brief Register value object
 * @param val_obj Value object pointer
 */
void sni_tb_register_val_obj(const sni_val_obj_t *val_obj);

/**
 * @brief Register handle destroy callback (supports External/Realm lifecycle handles)
 * @param type Handle type (must be Handle type)
 * @param destroy_cb Destroy callback
 */
void sni_tb_register_handle_destroy_cb(sni_type_t type, sni_handle_destroy_cb_t destroy_cb);

/**
 * @brief Unregister a native handle from its owning context and mark it dead
 * @param handle Target handle
 */
void sni_tb_unregister_handle(sni_control_block_t *handle);

void sni_tb_clear_resource_native_ptr(jerry_value_t obj);

/**
 * @brief Link a sub-resource handle to its parent tree-node's control block.
 *
 * After a sub-resource is created (e.g., lv_chart_add_series), call this
 * to register it under the parent object so that when the parent is deleted
 * the sub-resource handle is cascadingly marked dead.
 *
 * @param parent_ptr Raw pointer to the parent LVGL object (tree node)
 * @param sub_ptr   Raw pointer to the sub-resource
 * @param sub_type  SNI handle type of the sub-resource
 */
void sni_tb_link_sub_resource(void *parent_ptr, void *sub_ptr, sni_type_t sub_type);

/**
 * @brief Unlink a sub-resource handle from its parent after explicit deletion.
 *
 * Called when the sub-resource is explicitly removed (e.g., lv_chart_remove_series)
 * before the parent is destroyed.
 *
 * @param sub_ptr  Raw pointer to the sub-resource
 * @param sub_type SNI handle type of the sub-resource
 */
void sni_tb_unlink_sub_resource(void *sub_ptr, sni_type_t sub_type);

void sni_tb_init(void);

/**
 * @brief Retrieve the sni_control_block_t from an LVGL object via the
 *        unified eos_wdata registry (EOS_WDATA_SNI_CB).
 *
 * Replaces the previous raw lv_obj_get_user_data() cast, which could
 * collide with eos_wdata containers stored on the same object.
 *
 * @param ptr Raw pointer to an LVGL object (tree node)
 * @return sni_control_block_t* or NULL if none is registered
 */
sni_control_block_t *sni_cb_from_obj(void *ptr);

/**
 * @brief Get the current SNI context for the active script realm
 * @return sni_context_t* or NULL if no script is running
 */
struct sni_context *sni_get_current_context(void);

/**
 * @brief Convert JS value to a parent LVGL object pointer
 *
 * Tries SNI_H_LV_OBJ first (regular lv_obj_t*), then falls back to
 * SNI_H_EOS_VIEW (Activity View wrapper). Use this for constructor
 * parent arguments, setParent, alignTo, and swap so that both
 * regular LVGL objects and Activity Views are accepted.
 *
 * @param val JerryScript value (parent argument)
 * @param out Output lv_obj_t* pointer
 * @return bool Whether conversion was successful
 */
bool sni_tb_js2c_parent(jerry_value_t val, void **out);

#ifdef __cplusplus
}
#endif

#endif /* SNI_TYPE_BRIDGE_H */
