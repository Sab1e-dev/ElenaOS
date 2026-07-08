/**
 * @file sni_api_lv_anim.c
 * @brief LVGL Animation Class Special Wrapper Function Implementation
 */

#include "sni_api_lv_special.h"

/* Includes ---------------------------------------------------*/
#include "eos_mem.h"
#include "lvgl.h"
#include "sni_api_export.h"
#include "sni_callback_runtime.h"
#include "sni_type_bridge.h"
#include "sni_types.h"
#include "sni_context.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static sni_anim_callback_ctx_t *sni_anim_get_ctx(jerry_value_t this_value)
{
    sni_anim_callback_ctx_t *ctx = NULL;
    if (!sni_tb_js2c(this_value, SNI_H_LV_ANIM, &ctx))
    {
        return NULL;
    }

    return ctx;
}

jerry_value_t sni_api_ctor_anim(const jerry_call_info_t *call_info_p,
                                const jerry_value_t args_p[],
                                const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;

    if (jerry_value_is_undefined(call_info_p->new_target))
    {
        return sni_api_throw_error("Constructor must be called with new");
    }

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!sni_cb_anim_create(&ctx))
    {
        return sni_api_throw_error("Failed to create anim");
    }

    sni_context_add_anim(sni_cb_get_context(), ctx, call_info_p->this_value);

    if (!sni_tb_c2js_set_object(&ctx, SNI_H_LV_ANIM, call_info_p->this_value))
    {
        sni_context_remove_anim(sni_cb_get_context(), ctx);
        eos_free(ctx);
        return sni_api_throw_error("Failed to bind native object");
    }

    return jerry_undefined();
}

jerry_value_t sni_api_lv_anim_set_values(const jerry_call_info_t *call_info_p,
                                         const jerry_value_t args_p[],
                                         const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;

    if (args_count != 2)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_number(args_p[0]) || !jerry_value_is_number(args_p[1]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    lv_anim_set_values(sni_cb_anim_get_lv_anim(ctx), sni_tb_js2c_int32(args_p[0]), sni_tb_js2c_int32(args_p[1]));
    return jerry_undefined();
}

jerry_value_t sni_api_lv_anim_set_duration(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args_p[],
                                           const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_number(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    lv_anim_set_duration(sni_cb_anim_get_lv_anim(ctx), sni_tb_js2c_uint32(args_p[0]));
    return jerry_undefined();
}

jerry_value_t sni_api_lv_anim_set_delay(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_number(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    lv_anim_set_delay(sni_cb_anim_get_lv_anim(ctx), sni_tb_js2c_uint32(args_p[0]));
    return jerry_undefined();
}

jerry_value_t sni_api_lv_anim_set_repeat_count(const jerry_call_info_t *call_info_p,
                                               const jerry_value_t args_p[],
                                               const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_number(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    lv_anim_set_repeat_count(sni_cb_anim_get_lv_anim(ctx), sni_tb_js2c_uint32(args_p[0]));
    return jerry_undefined();
}

jerry_value_t sni_api_lv_anim_start(const jerry_call_info_t *call_info_p,
                                    const jerry_value_t args_p[],
                                    const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;
    (void)args_p;

    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!sni_cb_anim_start(ctx))
    {
        return sni_api_throw_error("Failed to start anim");
    }

    return jerry_undefined();
}

jerry_value_t sni_api_lv_anim_set_custom_exec_cb(const jerry_call_info_t *call_info_p,
                                                 const jerry_value_t args_p[],
                                                 const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_function(args_p[0]) || !sni_cb_anim_set_cb(ctx, SNI_ANIM_CB_SLOT_CUSTOM_EXEC, args_p[0]))
    {
        return sni_api_throw_error("Failed to set anim callback");
    }

    return jerry_undefined();
}

jerry_value_t sni_api_lv_anim_set_start_cb(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args_p[],
                                           const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_function(args_p[0]) || !sni_cb_anim_set_cb(ctx, SNI_ANIM_CB_SLOT_START, args_p[0]))
    {
        return sni_api_throw_error("Failed to set anim callback");
    }

    return jerry_undefined();
}

jerry_value_t sni_api_lv_anim_set_completed_cb(const jerry_call_info_t *call_info_p,
                                               const jerry_value_t args_p[],
                                               const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_function(args_p[0]) || !sni_cb_anim_set_cb(ctx, SNI_ANIM_CB_SLOT_COMPLETED, args_p[0]))
    {
        return sni_api_throw_error("Failed to set anim callback");
    }

    return jerry_undefined();
}

jerry_value_t sni_api_lv_anim_set_deleted_cb(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args_p[],
                                             const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_function(args_p[0]) || !sni_cb_anim_set_cb(ctx, SNI_ANIM_CB_SLOT_DELETED, args_p[0]))
    {
        return sni_api_throw_error("Failed to set anim callback");
    }

    return jerry_undefined();
}

jerry_value_t sni_api_lv_anim_set_get_value_cb(const jerry_call_info_t *call_info_p,
                                               const jerry_value_t args_p[],
                                               const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_function(args_p[0]) || !sni_cb_anim_set_cb(ctx, SNI_ANIM_CB_SLOT_GET_VALUE, args_p[0]))
    {
        return sni_api_throw_error("Failed to set anim callback");
    }

    return jerry_undefined();
}

jerry_value_t sni_api_lv_anim_set_path_cb(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_count)
{
    sni_anim_callback_ctx_t *ctx = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (jerry_value_is_number(args_p[0]))
    {
        if (!sni_cb_anim_set_path_builtin(ctx, (sni_anim_path_builtin_t)sni_tb_js2c_int32(args_p[0])))
        {
            return sni_api_throw_error("Failed to set anim path");
        }

        return jerry_undefined();
    }

    if (jerry_value_is_function(args_p[0]))
    {
        if (!sni_cb_anim_set_path_js(ctx, args_p[0]))
        {
            return sni_api_throw_error("Failed to set anim path");
        }

        return jerry_undefined();
    }

    return sni_api_throw_error("Invalid argument type");
}

jerry_value_t sni_api_lv_anim_set_var(const jerry_call_info_t *call_info_p,
                                      const jerry_value_t args_p[],
                                      const jerry_length_t args_count)
{
    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    sni_anim_callback_ctx_t *ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_object(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    lv_obj_t *arg_var;
    if (!sni_tb_js2c(args_p[0], SNI_H_LV_OBJ, &arg_var))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    lv_anim_set_var(sni_cb_anim_get_lv_anim(ctx), arg_var);
    return jerry_undefined();
}

jerry_value_t sni_api_prop_set_anim_var(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    sni_anim_callback_ctx_t *ctx = sni_anim_get_ctx(call_info_p->this_value);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_object(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    lv_obj_t *prop_value;
    if (!sni_tb_js2c(args_p[0], SNI_H_LV_OBJ, &prop_value))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    lv_anim_set_var(sni_cb_anim_get_lv_anim(ctx), prop_value);
    return jerry_undefined();
}
