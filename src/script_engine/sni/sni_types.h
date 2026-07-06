/**
 * @file sni_types.h
 * @brief Script Native Interface - Type system and control block definitions
 *
 * Architecture overview:
 *   Handle Objects are classified as:
 *     1. Object Tree Nodes  - lifecycle tied to LVGL object tree, control block in user_data for O(1) lookup
 *     2. Managed Resources  - lifecycle managed by SNI, stored in categorized linked lists (O(n/k) lookup)
 */

#ifndef SNI_TYPES_H
#define SNI_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "jerryscript.h"
/* Public macros ----------------------------------------------*/

#define SNI_TYPE_IS_NUMBER(type) ((type) >= __SNI_TYPE_NUMBER_START && (type) <= __SNI_TYPE_NUMBER_END)
#define SNI_TYPE_IS_HANDLE(type) ((type) >= __SNI_HANDLE_START && (type) <= __SNI_HANDLE_END)
#define SNI_TYPE_IS_VALUE(type) ((type) >= __SNI_VALUE_START && (type) <= __SNI_VALUE_END)
#define SNI_TYPE_IS_TREE_NODE(type) ((type) == SNI_H_LV_OBJ)
#define SNI_TYPE_IS_MANAGED_RESOURCE(type) ((type) > __SNI_HANDLE_RESOURCE_START && (type) < __SNI_HANDLE_RESOURCE_END)

#define SNI_HANDLE_COUNT (__SNI_HANDLE_END - __SNI_HANDLE_START - 1)

#define SNI_MANAGED_RESOURCE_COUNT (__SNI_HANDLE_RESOURCE_END - __SNI_HANDLE_RESOURCE_START - 1)

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief SNI type enumeration
 */
typedef enum
{
    __SNI_TYPE_START = 0,
    SNI_T_UNKNOWN = 0,

    __SNI_TYPE_NUMBER_START,
    SNI_T_UINT8,
    SNI_T_INT8,
    SNI_T_UINT16,
    SNI_T_INT16,
    SNI_T_UINT32,
    SNI_T_INT32,
    SNI_T_DOUBLE,
    SNI_T_FLOAT,
    __SNI_TYPE_NUMBER_END,

    SNI_T_BOOL,
    SNI_T_STRING,
    SNI_T_PTR,

    __SNI_HANDLE_START,

    SNI_H_LV_OBJ,

    __SNI_HANDLE_RESOURCE_START,

    SNI_H_LV_TIMER,
    SNI_H_LV_STYLE,
    SNI_H_LV_ANIM,
    SNI_H_LV_CHART_CURSOR,
    SNI_H_LV_CHART_SERIES,
    SNI_H_INT32,
    SNI_H_LV_COLOR_FILTER_DSC,
    SNI_H_LV_DISPLAY,
    SNI_H_EOS_ACTIVITY,
    SNI_H_LV_DRAW_BUF,
    SNI_H_LV_DRAW_ARC_DSC,
    SNI_H_LV_DRAW_IMAGE_DSC,
    SNI_H_LV_DRAW_LABEL_DSC,
    SNI_H_LV_DRAW_LINE_DSC,
    SNI_H_LV_DRAW_RECT_DSC,
    SNI_H_LV_EVENT,
    SNI_H_LV_EVENT_CB,
    SNI_H_LV_EVENT_DSC,
    SNI_H_LV_FONT,
    SNI_H_LV_GRAD_DSC,
    SNI_H_LV_GROUP,
    SNI_H_LV_IMAGE_DSC,
    SNI_H_LV_LAYER,
    SNI_H_LV_OBJ_CLASS,
    SNI_H_LV_OBJ_TREE_WALK_CB,
    SNI_H_LV_OBSERVER,
    SNI_H_LV_STYLE_TRANSITION_DSC,
    SNI_H_LV_STYLE_VALUE,
    SNI_H_LV_SUBJECT,

    __SNI_HANDLE_RESOURCE_END,

    __SNI_HANDLE_END,

    __SNI_VALUE_START,
    SNI_V_LV_POINT,
    SNI_V_LV_COLOR16,
    SNI_V_LV_AREA,
    SNI_V_LV_COLOR32,
    SNI_V_LV_SQRT_RES,
    SNI_V_LV_ANIM_BEZIER3_PARA,
    SNI_V_LV_POINT_PRECISE,
    SNI_V_LV_GRAD_COLOR,
    SNI_V_LV_COLOR_HSV,
    SNI_V_LV_COLOR16A,
    SNI_V_LV_GRADIENT_STOP,
    SNI_V_LV_COLOR,
    SNI_V_LV_OPA,
    SNI_V_LV_PART,
    SNI_V_LV_STATE,
    SNI_V_LV_STYLE_PROP,
    SNI_V_LV_STYLE_SELECTOR,
    __SNI_VALUE_END,

    __SNI_TYPE_MAX
} sni_type_t;

/**
 * @brief Property structure
 *
 * Value objects are defined using property structure arrays
 *
 * Example:
 * ```c
 * const sni_val_prop_t lv_point_props[] = {
 *     {"x", SNI_T_INT32, offsetof(lv_point_t, x)},
 *     {"y", SNI_T_INT32, offsetof(lv_point_t, y)},
 * };
 * ```
 */
typedef struct
{
    const char *name; /**< Property name */
    sni_type_t type; /**< Property type */
    size_t offset; /**< Property offset in value object structure */
    uint8_t bit_width; /**< Bit width for bit field members (0 for non-bit field members) */
} sni_val_prop_t;

typedef struct
{
    sni_type_t type;
    uint16_t prop_count; /**< Property count */
    const sni_val_prop_t *props; /**< Property array pointer */
} sni_val_obj_t;

/**
 * @brief Control block for Object Tree Nodes only
 *
 * Bridges JS objects and native LVGL objects with bidirectional O(1) access.
 *
 * For Object Tree Nodes (SNI_H_LV_OBJ):
 *   - JS object -> native_ptr -> sni_control_block_t -> ptr (C object)
 *   - LVGL object -> user_data -> sni_control_block_t -> obj (JS object)
 *
 * Note: Managed Resources do NOT use control blocks. They store data directly
 * in sni_managed_resource_node_t for flattened memory layout.
 */
typedef struct sni_control_block
{
    void *ptr; /**< Pointer to native C object */
    jerry_value_t js_obj; /**< JavaScript object corresponding to the handle */
    sni_type_t type; /**< Handle type for runtime validation */
    bool is_alive; /**< Whether the native object is still alive */
    void *aux; /**< Module-private auxiliary context */
    struct sni_context *owner_ctx; /**< Owning SNI context (Realm) */
    uint32_t engine_gen; /**< Engine generation at creation time */
} sni_control_block_t;

typedef void (*sni_handle_destroy_cb_t)(void *native_ptr);

/**
 * @brief Managed resource linked list node (flattened data structure)
 *
 * Stores native pointer, JS object reference, and metadata directly without
 * indirection through control block. This eliminates redundant ptr storage
 * and simplifies memory management for managed resources.
 *
 * Used to organize managed resources by type within a Realm context.
 * Each type has its own linked list for O(n/k) lookup where k is the number
 * of resource types (much smaller than total resource count).
 */
typedef struct sni_managed_resource_node
{
    void *ptr; /**< Pointer to native resource */
    jerry_value_t js_obj; /**< JavaScript object (was in control block) */
    sni_type_t type; /**< Resource type (was in control block) */
    bool is_alive; /**< Lifecycle status (was in control block) */
    struct sni_managed_resource_node *next; /**< Next node in type-specific list */
} sni_managed_resource_node_t;

/**
 * @brief Per-Realm SNI context
 *
 * Maintains type-indexed linked lists of managed resources for lifecycle management.
 * Object Tree Nodes are NOT stored here - they use LVGL's user_data mechanism.
 *
 * Array index = type - __SNI_HANDLE_RESOURCE_START - 1
 */
typedef struct sni_context
{
    sni_managed_resource_node_t *resource_heads[SNI_MANAGED_RESOURCE_COUNT];
    int resource_counts[SNI_MANAGED_RESOURCE_COUNT];
    void *event_ctx_list;
    struct script_program *owner;
    bool paused;
} sni_context_t;

#ifdef __cplusplus
}
#endif

#endif /* SNI_TYPES_H */
