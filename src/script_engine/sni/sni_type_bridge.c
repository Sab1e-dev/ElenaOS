/**
 * @file sni_type_bridge.c
 * @brief Type bridge with Control Block architecture
 *
 * Architecture:
 *   Object Tree Nodes (SNI_H_LV_OBJ):
 *     - Control block stored in lv_obj_t->user_data for O(1) C→JS lookup
 *     - JS native_ptr points to same control block for O(1) JS→C lookup
 *     - Lifecycle: tied to LVGL object tree, DELETE event marks handle dead
 *
 *   Managed Resources (all other handle types):
 *     - NO control block allocated (flattened memory layout)
 *     - Data stored directly in sni_managed_resource_node_t in type-indexed lists
 *     - JS native_ptr points to resource node for JS→C lookup
 *     - Lifecycle: managed by SNI, cleaned up on Realm destroy
 */

#include "sni_type_bridge.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "sni_types.h"
#include "sni_context.h"
#include "eos_mem.h"
#include "script_engine_core.h"
#include "jerryscript.h"
#include "sni_lv_types.h"
#include "sni_callback_runtime.h"
#define EOS_LOG_TAG "SNI-Bridge"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

static sni_val_obj_t sni_val_objs[__SNI_TYPE_MAX] = {0};
static void sni_control_block_free_cb(void *native_p, struct jerry_object_native_info_t *info_p);
static void sni_obj_deleted_cb(lv_event_t *e);
static void sni_resource_node_free_cb(void *native_p, struct jerry_object_native_info_t *info_p);
static const jerry_object_native_info_t sni_native_info = {
    .free_cb = sni_control_block_free_cb,
    .number_of_references = 0,
    .offset_of_references = 0,
};
static const jerry_object_native_info_t sni_resource_native_info = {
    .free_cb = sni_resource_node_free_cb,
    .number_of_references = 0,
    .offset_of_references = 0,
};
static sni_handle_destroy_cb_t sni_handle_destroy_cb[SNI_HANDLE_COUNT] = {0};

static int sni_tb_get_destroy_cb_index(sni_type_t type)
{
    if (!SNI_TYPE_IS_HANDLE(type))
    {
        return -1;
    }
    return (int)(type - __SNI_HANDLE_START - 1);
}

static sni_context_t *sni_get_current_context(void)
{
    return sni_cb_get_context();
}

void sni_tb_unregister_handle(sni_control_block_t *cb)
{
    if (!cb)
    {
        return;
    }

    if (cb->owner_ctx && cb->ptr)
    {
        if (SNI_TYPE_IS_TREE_NODE(cb->type))
        {
            sni_context_remove_resource(cb->owner_ctx, cb->ptr, cb->type);
        }
    }

    jerry_value_t tmp = cb->js_obj;
    cb->js_obj = jerry_undefined();
    cb->ptr = NULL;
    cb->is_alive = false;

    if (!jerry_value_is_undefined(tmp) && !jerry_value_is_null(tmp))
    {
        jerry_value_free(tmp);
    }
}

static uint32_t sni_tb_read_bitfield(void *ptr, uint32_t bit_offset, uint32_t bit_width)
{
    uint8_t *bytes = (uint8_t *)ptr;
    uint32_t result = 0;
    for (uint32_t i = 0; i < bit_width; i++)
    {
        uint32_t total_bit = bit_offset + i;
        uint32_t byte_idx = total_bit / 8;
        uint32_t bit_idx = total_bit % 8;
        if (bytes[byte_idx] & (1u << bit_idx))
        {
            result |= (1u << i);
        }
    }
    return result;
}

static void sni_tb_write_bitfield(void *ptr, uint32_t bit_offset, uint32_t bit_width, uint32_t value)
{
    uint8_t *bytes = (uint8_t *)ptr;
    for (uint32_t i = 0; i < bit_width; i++)
    {
        uint32_t total_bit = bit_offset + i;
        uint32_t byte_idx = total_bit / 8;
        uint32_t bit_idx = total_bit % 8;
        if (value & (1u << i))
        {
            bytes[byte_idx] |= (1u << bit_idx);
        }
        else
        {
            bytes[byte_idx] &= ~(1u << bit_idx);
        }
    }
}

static sni_control_block_t *sni_cb_from_obj(void *ptr)
{
    if (!ptr)
        return NULL;
    return (sni_control_block_t *)lv_obj_get_user_data((lv_obj_t *)ptr);
}

static void sni_obj_deleted_cb(lv_event_t *e);

static void *sni_node_from_native(void *ptr, sni_type_t type)
{
    if (!ptr)
        return NULL;

    if (SNI_TYPE_IS_TREE_NODE(type))
    {
        sni_control_block_t *cb = sni_cb_from_obj(ptr);
        if (!cb)
            return NULL;
        /* Stale control block from a previous engine session — the
         * cb->js_obj handle refers to a destroyed JerryScript heap.
         * Clear the LVGL link, remove stale callbacks, free the old
         * control block, and return NULL so the caller creates a
         * fresh wrapper on the current engine generation. */
        if (cb->engine_gen != script_engine_get_gen())
        {
            lv_obj_remove_event_cb((lv_obj_t *)ptr, sni_obj_deleted_cb);
            lv_obj_set_user_data((lv_obj_t *)ptr, NULL);
            eos_free(cb);
            return NULL;
        }
        return cb;
    }

    sni_context_t *ctx = sni_get_current_context();
    return sni_context_find_resource(ctx, ptr, type);
}

static void sni_cb_embed_obj(void *ptr, sni_control_block_t *cb)
{
    lv_obj_t *obj = (lv_obj_t *)ptr;
    lv_obj_set_user_data(obj, cb);
    lv_obj_add_event_cb(obj, sni_obj_deleted_cb, LV_EVENT_DELETE, cb);
}

static void sni_handle_embed(void *ptr, sni_type_t type, jerry_value_t js_obj)
{
    sni_context_t *ctx = sni_get_current_context();
    if (!ctx)
    {
        return;
    }

    if (SNI_TYPE_IS_TREE_NODE(type))
    {
        sni_control_block_t *cb = (sni_control_block_t *)jerry_object_get_native_ptr(js_obj, &sni_native_info);
        if (cb)
        {
            sni_cb_embed_obj(ptr, cb);
        }
        return;
    }

    sni_context_add_resource(ctx, ptr, js_obj, type);
}

static void sni_obj_deleted_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    sni_control_block_t *cb;

    if (!obj)
    {
        return;
    }

    cb = (sni_control_block_t *)lv_event_get_user_data(e);
    if (!cb)
    {
        return;
    }

    /* Stale control block from a previous engine session — the
     * cb->js_obj handle refers to a destroyed JerryScript heap.
     * Clear the LVGL link, free the stale CB, and bail out without
     * touching JerryScript to prevent both memory leaks and
     * refcount assertions. */
    if (cb->engine_gen != script_engine_get_gen())
    {
        cb->ptr = NULL;
        lv_obj_set_user_data(obj, NULL);
        eos_free(cb);
        return;
    }

    /* Mark dead and clear ptr BEFORE jerry_value_free, which may
       trigger GC and sni_control_block_free_cb. This prevents
       free_cb from attempting LVGL cleanup on this object and
       prevents use-after-free of cb after it is freed by free_cb. */
    cb->is_alive = false;

    /* Cascade-invalidate sub-resource handles.  LVGL destroys
       sub-resource native objects when the parent is deleted
       (e.g., chart series/cursors disappear with the chart), so
       their pointers are now dangling.  Mark them dead so future
       JS access is rejected. */
    sni_managed_resource_node_t *sub = cb->sub_resource_head;
    while (sub)
    {
        sni_managed_resource_node_t *next = sub->next;
        sub->is_alive = false;
        sub->parent_cb = NULL;
        sub->ptr = NULL;
        if (!jerry_value_is_undefined(sub->js_obj) && !jerry_value_is_null(sub->js_obj))
        {
            jerry_object_set_native_ptr(sub->js_obj, &sni_resource_native_info, NULL);
            jerry_value_free(sub->js_obj);
            sub->js_obj = jerry_undefined();
        }
        sub = next;
    }
    cb->sub_resource_head = NULL;

    jerry_value_t js_obj = cb->js_obj;
    cb->js_obj = jerry_undefined();

    cb->ptr = NULL;

    jerry_value_free(js_obj);
}

static void sni_control_block_free_cb(void *native_p, struct jerry_object_native_info_t *info_p)
{
    sni_control_block_t *cb = (sni_control_block_t *)native_p;

    (void)info_p;

    if (!cb)
    {
        return;
    }

    cb->is_alive = false;

    if (cb->ptr != NULL)
    {
        switch (cb->type)
        {
            case SNI_H_LV_OBJ:
            {
                lv_obj_t *obj = (lv_obj_t *)cb->ptr;
                if (lv_obj_is_valid(obj))
                {
                    /* Do NOT call lv_obj_remove_event_cb here.
                   The event descriptor lifecycle is managed by
                   LVGL's obj_delete_core → lv_event_remove_all.
                   If this free_cb is triggered during
                   lv_obj_send_event(LV_EVENT_DELETE) → sni_obj_deleted_cb
                   → jerry_value_free → GC, the descriptor has
                   already been freed by lv_event_remove_all,
                   causing a double-free in lv_tlsf_free. */
                    lv_obj_set_user_data(obj, NULL);
                }
                break;
            }
            default:
                break;
        }
    }

    if (!jerry_value_is_undefined(cb->js_obj) && !jerry_value_is_null(cb->js_obj))
    {
        jerry_value_free(cb->js_obj);
        cb->js_obj = jerry_undefined();
    }

    eos_free(cb);
}

static void sni_resource_node_free_cb(void *native_p, struct jerry_object_native_info_t *info_p)
{
    sni_managed_resource_node_t *node = (sni_managed_resource_node_t *)native_p;

    (void)info_p;

    if (!node)
    {
        return;
    }

    node->is_alive = false;

    if (!jerry_value_is_undefined(node->js_obj) && !jerry_value_is_null(node->js_obj))
    {
        jerry_value_free(node->js_obj);
        node->js_obj = jerry_undefined();
    }

    eos_free(node);
}

/************************** Type bridge functions **************************/

const char *sni_tb_js2c_string(jerry_value_t js_val)
{
    if (!jerry_value_is_string(js_val))
    {
        return NULL;
    }

    jerry_size_t str_len = jerry_string_size(js_val, JERRY_ENCODING_UTF8);

    char *string = eos_malloc(str_len + 1);
    if (!string)
    {
        return NULL;
    }

    jerry_string_to_buffer(js_val, JERRY_ENCODING_UTF8, (jerry_char_t *)string, str_len);

    string[str_len] = '\0';
    return string;
}

bool sni_tb_js2c(jerry_value_t js_val, sni_type_t type, void *out_obj)
{
    if (out_obj == NULL)
    {
        return false;
    }

    if (jerry_value_is_undefined(js_val) || jerry_value_is_null(js_val))
    {
        return false;
    }

    switch (type)
    {
        case SNI_T_UINT8:
            if (!jerry_value_is_number(js_val))
            {
                return false;
            }
            *(uint8_t *)out_obj = (uint8_t)jerry_value_as_number(js_val);
            return true;

        case SNI_T_INT8:
            if (!jerry_value_is_number(js_val))
            {
                return false;
            }
            *(int8_t *)out_obj = (int8_t)jerry_value_as_number(js_val);
            return true;

        case SNI_T_UINT16:
            if (!jerry_value_is_number(js_val))
            {
                return false;
            }
            *(uint16_t *)out_obj = (uint16_t)jerry_value_as_number(js_val);
            return true;

        case SNI_T_INT16:
            if (!jerry_value_is_number(js_val))
            {
                return false;
            }
            *(int16_t *)out_obj = (int16_t)jerry_value_as_number(js_val);
            return true;

        case SNI_T_UINT32:
            if (!jerry_value_is_number(js_val))
            {
                return false;
            }
            *(uint32_t *)out_obj = (uint32_t)jerry_value_as_number(js_val);
            return true;

        case SNI_T_INT32:
            if (!jerry_value_is_number(js_val))
            {
                return false;
            }
            *(int32_t *)out_obj = jerry_value_as_int32(js_val);
            return true;

        case SNI_T_DOUBLE:
            if (!jerry_value_is_number(js_val))
            {
                return false;
            }
            *(double *)out_obj = jerry_value_as_number(js_val);
            return true;

        case SNI_T_FLOAT:
            if (!jerry_value_is_number(js_val))
            {
                return false;
            }
            *(float *)out_obj = (float)jerry_value_as_number(js_val);
            return true;

        case SNI_T_BOOL:
            *(bool *)out_obj = jerry_value_to_boolean(js_val);
            return true;

        case SNI_T_STRING:
            if (!jerry_value_is_string(js_val))
            {
                return false;
            }
            *(const char **)out_obj = sni_tb_js2c_string(js_val);
            return (*(const char **)out_obj != NULL);

        case SNI_T_PTR:
            if (!jerry_value_is_number(js_val))
            {
                return false;
            }
            *(void **)out_obj = (void *)(uintptr_t)jerry_value_as_number(js_val);
            return true;

        default:
            break;
    }

    if (type == SNI_V_LV_COLOR)
    {
        if (jerry_value_is_number(js_val))
        {
            *(lv_color_t *)out_obj = lv_color_hex(sni_tb_js2c_uint32(js_val));
            return true;
        }
    }

    if (SNI_TYPE_IS_VALUE(type))
    {
        if (!jerry_value_is_object(js_val))
        {
            return false;
        }

        const sni_val_obj_t *val_obj = &sni_val_objs[type];
        if (val_obj->prop_count == 0 || val_obj->props == NULL)
        {
            return false;
        }

        uint8_t current_bit_offset = 0;
        size_t last_offset = 0;

        for (uint16_t i = 0; i < val_obj->prop_count; i++)
        {
            const sni_val_prop_t *prop = &val_obj->props[i];

            jerry_value_t js_prop = jerry_object_get_sz(js_val, prop->name);

            if (jerry_value_is_exception(js_prop))
            {
                jerry_value_free(js_prop);
                return false;
            }

            void *field_ptr = (uint8_t *)out_obj + prop->offset;

            if (prop->offset != last_offset)
            {
                current_bit_offset = 0;
                last_offset = prop->offset;
            }

            bool ok;
            if (prop->bit_width > 0)
            {
                if (!jerry_value_is_number(js_prop))
                {
                    jerry_value_free(js_prop);
                    return false;
                }
                uint32_t value = (uint32_t)jerry_value_as_number(js_prop);
                sni_tb_write_bitfield(field_ptr, current_bit_offset, prop->bit_width, value);
                current_bit_offset += prop->bit_width;
                ok = true;
            }
            else
            {
                ok = sni_tb_js2c(js_prop, prop->type, field_ptr);
            }

            jerry_value_free(js_prop);

            if (!ok)
            {
                return false;
            }
        }

        return true;
    }

    if (SNI_TYPE_IS_HANDLE(type))
    {
        if (jerry_value_is_null(js_val) || jerry_value_is_undefined(js_val))
        {
            *(void **)out_obj = NULL;
            return true;
        }

        if (!jerry_value_is_object(js_val))
        {
            return false;
        }

        if (SNI_TYPE_IS_TREE_NODE(type))
        {
            sni_control_block_t *cb = jerry_object_get_native_ptr(js_val, &sni_native_info);

            if (!cb || !cb->is_alive || cb->type != type)
            {
                return false;
            }

            *(void **)out_obj = cb->ptr;
            return true;
        }

        if (SNI_TYPE_IS_MANAGED_RESOURCE(type))
        {
            sni_managed_resource_node_t *node = jerry_object_get_native_ptr(js_val, &sni_resource_native_info);

            if (!node || !node->is_alive || node->type != type)
            {
                return false;
            }

            *(void **)out_obj = node->ptr;
            return true;
        }

        return false;
    }

    return false;
}

bool sni_tb_js2c_any_handle(jerry_value_t js_val, void *out_obj, sni_type_t *out_type)
{
    if (out_obj == NULL)
    {
        return false;
    }

    if (jerry_value_is_null(js_val) || jerry_value_is_undefined(js_val))
    {
        *(void **)out_obj = NULL;
        if (out_type)
        {
            *out_type = SNI_T_UNKNOWN;
        }
        return true;
    }

    if (!jerry_value_is_object(js_val))
    {
        return false;
    }

    sni_control_block_t *cb = jerry_object_get_native_ptr(js_val, &sni_native_info);
    if (cb && cb->is_alive)
    {
        *(void **)out_obj = cb->ptr;
        if (out_type)
        {
            *out_type = cb->type;
        }
        return true;
    }

    sni_managed_resource_node_t *node = jerry_object_get_native_ptr(js_val, &sni_resource_native_info);
    if (node && node->is_alive)
    {
        *(void **)out_obj = node->ptr;
        if (out_type)
        {
            *out_type = node->type;
        }
        return true;
    }

    return false;
}

jerry_value_t sni_tb_c2js(void *c_val, sni_type_t type)
{
    if (c_val == NULL)
    {
        return jerry_undefined();
    }

    switch (type)
    {
        case SNI_T_UINT8:
            return jerry_number((double)*(uint8_t *)c_val);

        case SNI_T_INT8:
            return jerry_number((double)*(int8_t *)c_val);

        case SNI_T_UINT16:
            return jerry_number((double)*(uint16_t *)c_val);

        case SNI_T_INT16:
            return jerry_number((double)*(int16_t *)c_val);

        case SNI_T_UINT32:
            return jerry_number((double)*(uint32_t *)c_val);

        case SNI_T_INT32:
            return jerry_number((double)*(int32_t *)c_val);

        case SNI_T_DOUBLE:
            return jerry_number(*(double *)c_val);

        case SNI_T_FLOAT:
            return jerry_number((double)*(float *)c_val);

        case SNI_T_BOOL:
            return jerry_boolean(*(bool *)c_val);

        case SNI_T_STRING:
            return jerry_string_sz(*(const char **)c_val);

        case SNI_T_PTR:
            return jerry_number((double)(uintptr_t)*(void **)c_val);

        default:
            break;
    }

    if (SNI_TYPE_IS_VALUE(type))
    {
        jerry_value_t js_obj = jerry_object();

        if (!sni_tb_c2js_set_object(c_val, type, js_obj))
        {
            jerry_value_free(js_obj);
            return jerry_undefined();
        }

        return js_obj;
    }

    if (SNI_TYPE_IS_HANDLE(type))
    {
        void *ptr = *(void **)c_val;
        if (!ptr)
        {
            return jerry_null();
        }

        void *existing = sni_node_from_native(ptr, type);
        if (existing)
        {
            if (SNI_TYPE_IS_TREE_NODE(type))
            {
                sni_control_block_t *cb = (sni_control_block_t *)existing;
                return jerry_value_copy(cb->js_obj);
            }
            else
            {
                sni_managed_resource_node_t *node = (sni_managed_resource_node_t *)existing;
                return jerry_value_copy(node->js_obj);
            }
        }

        jerry_value_t js_obj = jerry_object();

        if (SNI_TYPE_IS_TREE_NODE(type))
        {
            sni_control_block_t *new_cb = eos_malloc_zeroed(sizeof(sni_control_block_t));
            if (!new_cb)
            {
                jerry_value_free(js_obj);
                return jerry_undefined();
            }

            new_cb->ptr = ptr;
            new_cb->js_obj = jerry_value_copy(js_obj);
            new_cb->type = type;
            new_cb->is_alive = true;
            new_cb->engine_gen = script_engine_get_gen();
            new_cb->owner_ctx = sni_get_current_context();

            jerry_object_set_native_ptr(js_obj, &sni_native_info, new_cb);
            sni_handle_embed(ptr, type, js_obj);
            return js_obj;
        }
        else
        {
            sni_context_t *ctx = sni_get_current_context();
            sni_context_add_resource(ctx, ptr, js_obj, type);
            sni_managed_resource_node_t *node = sni_context_find_resource(ctx, ptr, type);
            if (node)
            {
                jerry_object_set_native_ptr(js_obj, &sni_resource_native_info, node);
                return js_obj;
            }
            jerry_value_free(js_obj);
            return jerry_undefined();
        }
    }

    return jerry_undefined();
}

bool sni_tb_c2js_set_object(void *c_val, sni_type_t type, jerry_value_t js_obj)
{
    if (c_val == NULL || !jerry_value_is_object(js_obj))
    {
        return false;
    }

    if (SNI_TYPE_IS_VALUE(type))
    {
        const sni_val_obj_t *val_obj = &sni_val_objs[type];
        if (val_obj->prop_count == 0 || val_obj->props == NULL)
        {
            return false;
        }

        uint8_t current_bit_offset = 0;
        size_t last_offset = 0;

        for (uint16_t i = 0; i < val_obj->prop_count; i++)
        {
            const sni_val_prop_t *prop = &val_obj->props[i];
            void *field_ptr = (uint8_t *)c_val + prop->offset;

            if (prop->offset != last_offset)
            {
                current_bit_offset = 0;
                last_offset = prop->offset;
            }

            jerry_value_t js_prop;
            if (prop->bit_width > 0)
            {
                uint32_t value = sni_tb_read_bitfield(field_ptr, current_bit_offset, prop->bit_width);
                js_prop = jerry_number((double)value);
                current_bit_offset += prop->bit_width;
            }
            else
            {
                js_prop = sni_tb_c2js(field_ptr, prop->type);
            }

            if (jerry_value_is_undefined(js_prop) || jerry_value_is_exception(js_prop))
            {
                jerry_value_free(js_prop);
                return false;
            }

            jerry_value_t set_result = jerry_object_set_sz(js_obj, prop->name, js_prop);
            jerry_value_free(js_prop);

            if (jerry_value_is_exception(set_result))
            {
                jerry_value_free(set_result);
                return false;
            }

            jerry_value_free(set_result);
        }

        return true;
    }

    if (SNI_TYPE_IS_HANDLE(type))
    {
        void *ptr = *(void **)c_val;
        if (!ptr)
        {
            return false;
        }

        void *existing = sni_node_from_native(ptr, type);
        if (existing)
        {
            if (SNI_TYPE_IS_TREE_NODE(type))
            {
                sni_control_block_t *cb = (sni_control_block_t *)existing;
                jerry_object_set_native_ptr(js_obj, &sni_native_info, cb);
                return true;
            }
            else
            {
                sni_managed_resource_node_t *node = (sni_managed_resource_node_t *)existing;
                jerry_object_set_native_ptr(js_obj, &sni_resource_native_info, node);
                return true;
            }
        }

        if (SNI_TYPE_IS_TREE_NODE(type))
        {
            sni_control_block_t *new_cb = eos_malloc_zeroed(sizeof(sni_control_block_t));
            if (!new_cb)
            {
                return false;
            }

            new_cb->ptr = ptr;
            new_cb->js_obj = jerry_value_copy(js_obj);
            new_cb->type = type;
            new_cb->is_alive = true;
            new_cb->engine_gen = script_engine_get_gen();
            new_cb->owner_ctx = sni_get_current_context();

            jerry_object_set_native_ptr(js_obj, &sni_native_info, new_cb);
            sni_handle_embed(ptr, type, js_obj);
            return true;
        }
        else
        {
            sni_context_t *ctx = sni_get_current_context();
            sni_context_add_resource(ctx, ptr, js_obj, type);
            sni_managed_resource_node_t *node = sni_context_find_resource(ctx, ptr, type);
            if (node)
            {
                jerry_object_set_native_ptr(js_obj, &sni_resource_native_info, node);
                return true;
            }
            return false;
        }
    }

    return false;
}

void sni_tb_register_val_obj(const sni_val_obj_t *val_obj)
{
    if (val_obj->type < __SNI_VALUE_START || val_obj->type >= __SNI_VALUE_END)
    {
        return;
    }

    sni_val_objs[val_obj->type] = *val_obj;
    sni_val_objs[val_obj->type].props = val_obj->props;
}

void sni_tb_register_handle_destroy_cb(sni_type_t type, sni_handle_destroy_cb_t destroy_cb)
{
    int index = sni_tb_get_destroy_cb_index(type);
    if (index >= 0)
    {
        sni_handle_destroy_cb[index] = destroy_cb;
    }
}

void sni_tb_clear_resource_native_ptr(jerry_value_t obj)
{
    jerry_object_set_native_ptr(obj, &sni_resource_native_info, NULL);
}

void sni_tb_link_sub_resource(void *parent_ptr, void *sub_ptr, sni_type_t sub_type)
{
    if (!parent_ptr || !sub_ptr)
    {
        return;
    }

    sni_control_block_t *parent_cb = sni_cb_from_obj(parent_ptr);
    if (!parent_cb || !parent_cb->is_alive)
    {
        return;
    }

    sni_context_t *ctx = sni_get_current_context();
    if (!ctx)
    {
        return;
    }

    sni_managed_resource_node_t *sub_node = sni_context_find_resource(ctx, sub_ptr, sub_type);
    if (!sub_node)
    {
        return;
    }

    /* Guard: if already linked to the same parent, skip to avoid cycles */
    if (sub_node->parent_cb == parent_cb)
    {
        return;
    }

    sub_node->parent_cb = parent_cb;
    sub_node->next = parent_cb->sub_resource_head;
    parent_cb->sub_resource_head = sub_node;
}

void sni_tb_unlink_sub_resource(void *sub_ptr, sni_type_t sub_type)
{
    if (!sub_ptr)
    {
        return;
    }

    sni_context_t *ctx = sni_get_current_context();
    if (!ctx)
    {
        return;
    }

    sni_managed_resource_node_t *sub_node = sni_context_find_resource(ctx, sub_ptr, sub_type);
    if (!sub_node)
    {
        return;
    }

    sni_control_block_t *parent_cb = sub_node->parent_cb;
    if (!parent_cb)
    {
        return;
    }

    sni_managed_resource_node_t **prev = &parent_cb->sub_resource_head;
    while (*prev)
    {
        if (*prev == sub_node)
        {
            *prev = sub_node->next;
            break;
        }
        prev = &(*prev)->next;
    }

    sub_node->parent_cb = NULL;
}

void sni_tb_init(void)
{
    sni_lv_types_init();
}
