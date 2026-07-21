/**
 * @file sni_api_lv_canvas.c
 * @brief LVGL Canvas Class Special Wrapper Function Implementation
 */

#include "sni_api_lv_special.h"

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
#include "widgets/canvas/lv_canvas_private.h"
#include "sni_api_export.h"
#include "sni_type_bridge.h"
#include "sni_types.h"
#include "eos_mem.h"
#include "eos_basic_widgets.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

typedef struct sni_canvas_buf_ctx
{
    lv_obj_t *obj;
    lv_draw_buf_t *owned_draw_buf;
} sni_canvas_buf_ctx_t;

/* Function Implementations -----------------------------------*/

static bool sni_lv_canvas_get_self(const jerry_call_info_t *call_info_p, lv_obj_t **out_obj)
{
    if (!jerry_value_is_object(call_info_p->this_value))
    {
        return false;
    }

    return sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, out_obj);
}

static bool sni_lv_canvas_has_draw_buf(lv_obj_t *obj)
{
    return lv_canvas_get_draw_buf(obj) != NULL;
}

static sni_canvas_buf_ctx_t *sni_canvas_ctx_find(lv_obj_t *obj)
{
    sni_control_block_t *cb = sni_cb_from_obj(obj);
    if (!cb)
    {
        return NULL;
    }
    return (sni_canvas_buf_ctx_t *)cb->aux;
}

static void sni_canvas_ctx_remove(lv_obj_t *obj)
{
    sni_control_block_t *cb = sni_cb_from_obj(obj);
    if (!cb)
    {
        return;
    }

    sni_canvas_buf_ctx_t *ctx = (sni_canvas_buf_ctx_t *)cb->aux;
    if (!ctx)
    {
        return;
    }

    cb->aux = NULL;
    eos_free(ctx);
}

static void sni_canvas_ctx_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    sni_control_block_t *cb = sni_cb_from_obj(obj);
    if (!cb)
    {
        return;
    }

    sni_canvas_buf_ctx_t *ctx = (sni_canvas_buf_ctx_t *)cb->aux;
    if (!ctx)
    {
        return;
    }

    if (ctx->owned_draw_buf)
    {
        eos_draw_buf_destroy(ctx->owned_draw_buf);
        ctx->owned_draw_buf = NULL;
    }

    sni_canvas_ctx_remove(obj);
}

static sni_canvas_buf_ctx_t *sni_canvas_ctx_get_or_create(lv_obj_t *obj)
{
    sni_control_block_t *cb = sni_cb_from_obj(obj);
    if (!cb)
    {
        return NULL;
    }

    if (cb->aux)
    {
        return (sni_canvas_buf_ctx_t *)cb->aux;
    }

    sni_canvas_buf_ctx_t *ctx = eos_malloc_zeroed(sizeof(sni_canvas_buf_ctx_t));
    if (!ctx)
    {
        return NULL;
    }
    ctx->obj = obj;
    cb->aux = ctx;

    lv_obj_add_event_cb(obj, sni_canvas_ctx_delete_cb, LV_EVENT_DELETE, NULL);
    return ctx;
}

jerry_value_t sni_api_lv_canvas_init_buffer(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    if (args_count != 3 && args_count != 4)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *self_obj;
    if (!sni_lv_canvas_get_self(call_info_p, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_number(args_p[0]) || !jerry_value_is_number(args_p[1]) || !jerry_value_is_number(args_p[2]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    int32_t w = sni_tb_js2c_int32(args_p[0]);
    int32_t h = sni_tb_js2c_int32(args_p[1]);
    lv_color_format_t cf = sni_tb_js2c_int32(args_p[2]);

    if (w <= 0 || h <= 0)
    {
        return sni_api_throw_error("Invalid canvas size");
    }

    uint32_t stride = LV_STRIDE_AUTO;
    if (args_count == 4)
    {
        if (!jerry_value_is_number(args_p[3]))
        {
            return sni_api_throw_error("Invalid argument type");
        }
        stride = (uint32_t)sni_tb_js2c_int32(args_p[3]);
    }

    sni_canvas_buf_ctx_t *ctx = sni_canvas_ctx_get_or_create(self_obj);
    if (!ctx)
    {
        return sni_api_throw_error("Out of memory");
    }

    if (ctx->owned_draw_buf)
    {
        eos_draw_buf_destroy(ctx->owned_draw_buf);
        ctx->owned_draw_buf = NULL;
    }

    lv_draw_buf_t *buf = eos_draw_buf_create((uint32_t)w, (uint32_t)h, cf, stride);
    if (!buf)
    {
        return sni_api_throw_error("Failed to create draw buffer");
    }

    lv_canvas_set_draw_buf(self_obj, buf);
    ctx->owned_draw_buf = buf;

    return sni_tb_c2js(&buf, SNI_H_LV_DRAW_BUF);
}

jerry_value_t sni_api_lv_canvas_free_buffer(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    (void)args_p;
    if (args_count != 0)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *self_obj;
    if (!sni_lv_canvas_get_self(call_info_p, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    sni_canvas_buf_ctx_t *ctx = sni_canvas_ctx_find(self_obj);
    if (ctx && ctx->owned_draw_buf)
    {
        eos_draw_buf_destroy(ctx->owned_draw_buf);
        ctx->owned_draw_buf = NULL;
    }

    lv_canvas_t *canvas = (lv_canvas_t *)self_obj;
    canvas->draw_buf = NULL;
    lv_image_set_src(self_obj, NULL);
    lv_obj_invalidate(self_obj);

    return jerry_undefined();
}

jerry_value_t sni_api_lv_canvas_set_px(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args_p[],
                                       const jerry_length_t args_count)
{
    if (args_count != 4)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *self_obj;
    if (!sni_lv_canvas_get_self(call_info_p, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!sni_lv_canvas_has_draw_buf(self_obj))
    {
        return sni_api_throw_error("Canvas draw_buf is null, call setBuffer/setDrawBuf first");
    }

    if (!jerry_value_is_number(args_p[0]) || !jerry_value_is_number(args_p[1]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    int32_t arg_x = sni_tb_js2c_int32(args_p[0]);
    int32_t arg_y = sni_tb_js2c_int32(args_p[1]);

    lv_color_t arg_color;
    if (!sni_tb_js2c(args_p[2], SNI_V_LV_COLOR, &arg_color))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_number(args_p[3]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    lv_opa_t arg_opa = sni_tb_js2c_uint32(args_p[3]);
    lv_canvas_set_px(self_obj, arg_x, arg_y, arg_color, arg_opa);
    return jerry_undefined();
}

jerry_value_t sni_api_lv_canvas_get_px(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args_p[],
                                       const jerry_length_t args_count)
{
    if (args_count != 2)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    lv_obj_t *self_obj;
    if (!sni_lv_canvas_get_self(call_info_p, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!sni_lv_canvas_has_draw_buf(self_obj))
    {
        return sni_api_throw_error("Canvas draw_buf is null, call setBuffer/setDrawBuf first");
    }

    if (!jerry_value_is_number(args_p[0]) || !jerry_value_is_number(args_p[1]))
    {
        return sni_api_throw_error("Invalid argument type");
    }

    int32_t arg_x = sni_tb_js2c_int32(args_p[0]);
    int32_t arg_y = sni_tb_js2c_int32(args_p[1]);

    lv_color32_t result = lv_canvas_get_px(self_obj, arg_x, arg_y);
    return sni_tb_c2js(&result, SNI_V_LV_COLOR32);
}
