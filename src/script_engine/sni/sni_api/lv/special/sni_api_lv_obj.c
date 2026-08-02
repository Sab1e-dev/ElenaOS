/**
 * @file sni_api_lv_obj.c
 * @brief LVGL obj Class Special Wrapper Function Implementation
 */

#include "sni_api_lv_special.h"

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
#include "sni_api_export.h"
#include "sni_callback_runtime.h"
#include "sni_type_bridge.h"
#include "sni_types.h"
#include "eos_font.h"
#define EOS_LOG_TAG "SNI-LV-OBJ"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

#define SNI_OBJ_USER_DATA_KEY "__sni_user_data"

/* Function Implementations -----------------------------------*/

static bool sni_lv_obj_from_this(const jerry_call_info_t *call_info_p, lv_obj_t **out_obj)
{
    return sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, out_obj);
}

static bool sni_lv_obj_store_user_data(jerry_value_t js_obj, jerry_value_t value)
{
    jerry_value_t copied = jerry_value_copy(value);
    jerry_value_t result = jerry_object_set_sz(js_obj, SNI_OBJ_USER_DATA_KEY, copied);
    bool ok = !jerry_value_is_exception(result);

    jerry_value_free(result);
    jerry_value_free(copied);
    return ok;
}

static jerry_value_t sni_lv_obj_load_user_data(jerry_value_t js_obj)
{
    jerry_value_t value = jerry_object_get_sz(js_obj, SNI_OBJ_USER_DATA_KEY);
    if (jerry_value_is_exception(value))
    {
        jerry_value_free(value);
        return jerry_undefined();
    }

    return value;
}

static jerry_value_t sni_lv_obj_return_area(const lv_area_t *area)
{
    lv_area_t copy = *area;
    return sni_tb_c2js(&copy, SNI_V_LV_AREA);
}

static jerry_value_t sni_lv_obj_return_point(const lv_point_t *point)
{
    lv_point_t copy = *point;
    return sni_tb_c2js(&copy, SNI_V_LV_POINT);
}

jerry_value_t sni_api_lv_obj_add_event_cb(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_count)
{
    if (args_count != 3)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *self_obj;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_function(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    if (!jerry_value_is_number(args_p[1]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    lv_event_code_t arg_filter = sni_tb_js2c_int32(args_p[1]);
    lv_event_dsc_t *result = NULL;
    if (!sni_cb_event_add(self_obj, args_p[0], arg_filter, args_p[2], &result))
    {
        return sni_api_throw_error("Failed to register event callback");
    }

    return sni_tb_c2js(&result, SNI_H_LV_EVENT_DSC);
}

jerry_value_t sni_api_lv_obj_remove_event_cb(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args_p[],
                                             const jerry_length_t args_count)
{
    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *self_obj;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_function(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    return sni_tb_c2js_boolean(sni_cb_event_remove_by_js_cb(self_obj, args_p[0]));
}

jerry_value_t sni_api_lv_obj_remove_event_dsc(const jerry_call_info_t *call_info_p,
                                              const jerry_value_t args_p[],
                                              const jerry_length_t args_count)
{
    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *self_obj;
    lv_event_dsc_t *arg_dsc;

    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_LV_EVENT_DSC, &arg_dsc))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    return sni_tb_c2js_boolean(sni_cb_event_remove_dsc(self_obj, arg_dsc));
}

jerry_value_t sni_api_lv_obj_remove_event_cb_with_user_data(const jerry_call_info_t *call_info_p,
                                                            const jerry_value_t args_p[],
                                                            const jerry_length_t args_count)
{
    uint32_t result;

    if (args_count != 2)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *self_obj;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_function(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    result = sni_cb_event_remove_by_js_cb_user_data(self_obj, args_p[0], args_p[1]);
    return sni_tb_c2js(&result, SNI_T_UINT32);
}

jerry_value_t sni_api_lv_obj_send_event(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    lv_obj_t *self_obj;
    lv_event_code_t event_code;
    void *param = NULL;

    if (args_count < 1 || args_count > 2)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!sni_lv_obj_from_this(call_info_p, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_number(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    event_code = sni_tb_js2c_int32(args_p[0]);

    if (args_count == 2 && !jerry_value_is_null(args_p[1]) && !jerry_value_is_undefined(args_p[1]))
    {
        if (!sni_tb_js2c(args_p[1], SNI_T_PTR, &param))
        {
            return sni_api_throw_error("Failed to convert argument");
        }
    }

    lv_result_t result = lv_obj_send_event(self_obj, event_code, param);
    return sni_tb_c2js(&result, SNI_T_INT32);
}

jerry_value_t sni_api_lv_obj_set_user_data(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args_p[],
                                           const jerry_length_t args_count)
{
    lv_obj_t *self_obj;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!sni_lv_obj_from_this(call_info_p, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!sni_lv_obj_store_user_data(call_info_p->this_value, args_p[0]))
    {
        return sni_api_throw_error("Failed to store user data");
    }

    return jerry_undefined();
}

jerry_value_t sni_api_lv_obj_get_user_data(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args_p[],
                                           const jerry_length_t args_count)
{
    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    (void)args_p;
    return sni_lv_obj_load_user_data(call_info_p->this_value);
}

jerry_value_t sni_api_lv_obj_get_coords(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    lv_obj_t *self_obj;
    lv_area_t area;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    (void)args_p;
    if (!sni_lv_obj_from_this(call_info_p, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    lv_obj_get_coords(self_obj, &area);
    return sni_lv_obj_return_area(&area);
}

jerry_value_t sni_api_lv_obj_get_content_coords(const jerry_call_info_t *call_info_p,
                                                const jerry_value_t args_p[],
                                                const jerry_length_t args_count)
{
    lv_obj_t *self_obj;
    lv_area_t area;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    (void)args_p;
    if (!sni_lv_obj_from_this(call_info_p, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    lv_obj_get_content_coords(self_obj, &area);
    return sni_lv_obj_return_area(&area);
}

jerry_value_t sni_api_lv_obj_get_click_area(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    lv_obj_t *self_obj;
    lv_area_t area;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    (void)args_p;
    if (!sni_lv_obj_from_this(call_info_p, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    lv_obj_get_click_area(self_obj, &area);
    return sni_lv_obj_return_area(&area);
}

jerry_value_t sni_api_lv_obj_get_scroll_end(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    lv_obj_t *self_obj;
    lv_point_t point;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    (void)args_p;
    if (!sni_lv_obj_from_this(call_info_p, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    lv_obj_get_scroll_end(self_obj, &point);
    return sni_lv_obj_return_point(&point);
}

jerry_value_t sni_api_lv_obj_get_scrollbar_area(const jerry_call_info_t *call_info_p,
                                                const jerry_value_t args_p[],
                                                const jerry_length_t args_count)
{
    lv_obj_t *self_obj;
    lv_area_t hor;
    lv_area_t ver;
    jerry_value_t result;
    jerry_value_t hor_val;
    jerry_value_t ver_val;
    jerry_value_t set_result;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    (void)args_p;
    if (!sni_lv_obj_from_this(call_info_p, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    lv_obj_get_scrollbar_area(self_obj, &hor, &ver);

    result = jerry_object();
    hor_val = sni_lv_obj_return_area(&hor);
    ver_val = sni_lv_obj_return_area(&ver);

    set_result = jerry_object_set_sz(result, "hor", hor_val);
    jerry_value_free(hor_val);
    if (jerry_value_is_exception(set_result))
    {
        jerry_value_free(set_result);
        jerry_value_free(ver_val);
        jerry_value_free(result);
        return sni_api_throw_error("Failed to build return object");
    }
    jerry_value_free(set_result);

    set_result = jerry_object_set_sz(result, "ver", ver_val);
    jerry_value_free(ver_val);
    if (jerry_value_is_exception(set_result))
    {
        jerry_value_free(set_result);
        jerry_value_free(result);
        return sni_api_throw_error("Failed to build return object");
    }
    jerry_value_free(set_result);

    return result;
}

jerry_value_t sni_api_prop_get_obj_user_data(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args_p[],
                                             const jerry_length_t args_count)
{
    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    (void)args_p;
    return sni_lv_obj_load_user_data(call_info_p->this_value);
}

jerry_value_t sni_api_prop_set_obj_user_data(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args_p[],
                                             const jerry_length_t args_count)
{
    return sni_api_lv_obj_set_user_data(call_info_p, args_p, args_count);
}

jerry_value_t sni_api_lv_obj_delete(const jerry_call_info_t *call_info_p,
                                    const jerry_value_t args_p[],
                                    const jerry_length_t args_count)
{
    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *self_obj;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    /* Pre-clean the SNI control block before lv_obj_delete so that
     * jerry_value_free runs OUTSIDE the LV_EVENT_DELETE dispatch context.
     * This prevents JerryScript GC re-entrancy from interfering with
     * LVGL's event iteration, which could cause lv_obj_send_event to
     * return LV_RESULT_INVALID and obj_delete_core to silently abort
     * the deletion — leaving the object and all its children permanently
     * alive. */
    sni_control_block_t *cb = sni_cb_from_obj(self_obj);
    EOS_LOG_I("delete() called: obj=%p cb=%p is_alive=%d child_cnt=%u",
              (void *)self_obj, (void *)cb,
              cb ? cb->is_alive : -1,
              lv_obj_get_child_count(self_obj));
    if (cb && cb->is_alive)
    {
        cb->is_alive = false;

        /* Cascade-invalidate sub-resource handles.  LVGL destroys
         * sub-resource native objects when the parent is deleted, so
         * their pointers are about to become dangling.  Mark them dead
         * now and release their JerryScript references. */
        sni_managed_resource_node_t *sub = cb->sub_resource_head;
        while (sub)
        {
            sni_managed_resource_node_t *next = sub->next;
            sub->is_alive = false;
            sub->parent_cb = NULL;
            sub->ptr = NULL;
            if (!jerry_value_is_undefined(sub->js_obj) &&
                !jerry_value_is_null(sub->js_obj))
            {
                sni_tb_clear_resource_native_ptr(sub->js_obj);
                jerry_value_free(sub->js_obj);
                sub->js_obj = jerry_undefined();
            }
            sub = next;
        }
        cb->sub_resource_head = NULL;

        /* Release the JS reference now — GC runs here, outside the
         * LV_EVENT_DELETE dispatch that lv_obj_delete will trigger. */
        jerry_value_t js_obj = cb->js_obj;
        cb->js_obj = jerry_undefined();
        cb->ptr = NULL;
        jerry_value_free(js_obj);

        /* Remove the LV_EVENT_DELETE callback so sni_obj_deleted_cb
         * won't fire redundantly during lv_obj_delete.  It would find
         * cb->is_alive==false and bail out anyway, but removing it
         * avoids the unnecessary call. */
        lv_obj_remove_event_cb(self_obj, sni_obj_deleted_cb);
    }

    /* Cancel any pending lv_obj_delete_delayed animation on this object
     * before deleting.  lv_obj_delete_delayed creates an lv_anim_t with
     * exec_cb=NULL and completed_cb=lv_obj_delete_anim_completed_cb.
     * lv_anim_delete(var, NULL) matches and removes it, preventing a
     * dangling-pointer callback after the object is freed.
     * lv_obj_delete's own obj_delete_core already handles deleteAsync
     * cancellation via lv_async_call_cancel. */
    lv_anim_delete(self_obj, NULL);
    lv_obj_delete(self_obj);
    EOS_LOG_I("lv_obj_delete returned for obj=%p", (void *)self_obj);
    return jerry_undefined();
}

jerry_value_t sni_api_lv_obj_remove_event(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_count)
{
    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *self_obj;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    uint32_t arg_index = sni_tb_js2c_uint32(args_p[0]);

    /* Get the event descriptor first so we can clean up the SNI callback
     * context (JerryScript references + context list entry) before the
     * LVGL descriptor is removed.  This matches the pattern used by
     * removeEventDsc / removeEventCb. */
    lv_event_dsc_t *dsc = lv_obj_get_event_dsc(self_obj, arg_index);
    bool result = sni_cb_event_remove_dsc(self_obj, dsc);
    return sni_tb_c2js_boolean(result);
}

jerry_value_t sni_api_lv_obj_set_parent(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!jerry_value_is_object(call_info_p->this_value))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    lv_obj_t *self_obj;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_object(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    lv_obj_t *arg_parent;
    /* Use sni_tb_js2c_parent so both regular LVGL objects and EOS Activity
     * Views are accepted — matches the constructor's parent conversion. */
    if (!sni_tb_js2c_parent(args_p[0], (void **)&arg_parent))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    EOS_LOG_I("setParent: obj=%p old_parent=%p new_parent=%p",
              (void *)self_obj, (void *)lv_obj_get_parent(self_obj),
              (void *)arg_parent);
    lv_obj_set_parent(self_obj, arg_parent);
    EOS_LOG_I("setParent: after — obj=%p parent=%p child_cnt_of_new_parent=%u",
              (void *)self_obj, (void *)lv_obj_get_parent(self_obj),
              lv_obj_get_child_count(arg_parent));
    return jerry_undefined();
}

jerry_value_t sni_api_prop_set_obj_parent(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_count)
{
    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *self_obj;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_object(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    lv_obj_t *prop_value;
    /* Use sni_tb_js2c_parent so both regular LVGL objects and EOS Activity
     * Views are accepted — matches the constructor's parent conversion. */
    if (!sni_tb_js2c_parent(args_p[0], (void **)&prop_value))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    lv_obj_set_parent(self_obj, prop_value);
    return jerry_undefined();
}

jerry_value_t sni_api_eos_label_set_font_size(const jerry_call_info_t *call_info_p,
                                              const jerry_value_t args_p[],
                                              const jerry_length_t args_count)
{
    if (args_count != 1 || !jerry_value_is_number(args_p[0]))
    {
        return sni_api_throw_error("setFontSize: requires a numeric size argument");
    }

    lv_obj_t *self_obj;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("setFontSize: invalid object");
    }

    eos_font_size_t size = (eos_font_size_t)(uint32_t)jerry_value_as_number(args_p[0]);
    eos_label_set_font_size(self_obj, size);
    return jerry_undefined();
}
