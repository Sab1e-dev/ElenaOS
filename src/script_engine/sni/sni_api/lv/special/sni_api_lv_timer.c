/**
 * @file sni_api_lv_timer.c
 * @brief LVGL Timer Class Special Wrapper Function Implementation
 */

#include "sni_api_lv_special.h"

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
#include "sni_api_export.h"
#include "sni_callback_runtime.h"
#include "sni_type_bridge.h"
#include "sni_types.h"
#include "sni_context.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

jerry_value_t sni_api_ctor_timer(const jerry_call_info_t *call_info_p,
                                 const jerry_value_t args_p[],
                                 const jerry_length_t args_count)
{
    if (jerry_value_is_undefined(call_info_p->new_target))
    {
        return sni_api_throw_error("Constructor must be called with new");
    }

    if (args_count != 3)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!jerry_value_is_function(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    if (!jerry_value_is_number(args_p[1]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    uint32_t arg_period = sni_tb_js2c_uint32(args_p[1]);
    lv_timer_t *native_timer = NULL;
    if (!sni_cb_timer_create(args_p[0], arg_period, &native_timer))
    {
        return sni_api_throw_error("Failed to create timer");
    }

    sni_context_add_timer(sni_cb_get_context(), native_timer, call_info_p->this_value);

    if (!sni_tb_c2js_set_object(&native_timer, SNI_H_LV_TIMER, call_info_p->this_value))
    {
        sni_context_remove_timer(sni_cb_get_context(), native_timer);
        return sni_api_throw_error("Failed to bind native object");
    }

    return jerry_undefined();
}

jerry_value_t sni_api_lv_timer_set_cb(const jerry_call_info_t *call_info_p,
                                      const jerry_value_t args_p[],
                                      const jerry_length_t args_count)
{
    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_timer_t *self_timer;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_TIMER, &self_timer))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_function(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    return sni_tb_c2js_boolean(sni_cb_timer_set_cb(self_timer, args_p[0]));
}

jerry_value_t sni_api_lv_timer_delete(const jerry_call_info_t *call_info_p,
                                      const jerry_value_t args_p[],
                                      const jerry_length_t args_count)
{
    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_timer_t *self_timer;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_TIMER, &self_timer))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    sni_timer_callback_ctx_t *cb_ctx = (sni_timer_callback_ctx_t *)lv_timer_get_user_data(self_timer);
    if (cb_ctx && cb_ctx->owner_ctx)
    {
        sni_context_delete_timer_sync(cb_ctx->owner_ctx, self_timer);
    }

    return jerry_undefined();
}

jerry_value_t sni_api_prop_set_timer_cb(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_timer_t *self_timer;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_TIMER, &self_timer))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_function(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    if (!sni_cb_timer_set_cb(self_timer, args_p[0]))
    {
        return sni_api_throw_error("Failed to set timer callback");
    }

    return jerry_undefined();
}

jerry_value_t sni_api_lv_timer_set_auto_delete(const jerry_call_info_t *call_info_p,
                                               const jerry_value_t args_p[],
                                               const jerry_length_t args_count)
{
    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_timer_t *self_timer;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_TIMER, &self_timer))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_boolean(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    bool auto_delete = sni_tb_js2c_boolean(args_p[0]);
    if (!sni_cb_timer_set_auto_delete(self_timer, auto_delete))
    {
        return sni_api_throw_error("Failed to set timer autoDelete");
    }

    return jerry_undefined();
}

jerry_value_t sni_api_prop_set_timer_auto_delete(const jerry_call_info_t *call_info_p,
                                                 const jerry_value_t args_p[],
                                                 const jerry_length_t args_count)
{
    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_timer_t *self_timer;
    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_TIMER, &self_timer))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_boolean(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    bool auto_delete = sni_tb_js2c_boolean(args_p[0]);
    if (!sni_cb_timer_set_auto_delete(self_timer, auto_delete))
    {
        return sni_api_throw_error("Failed to set timer autoDelete");
    }

    return jerry_undefined();
}
