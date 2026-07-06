/**
 * @file sni_api_lv_btnm.c
 * @brief LVGL Button Matrix SNI Handwritten Special Wrapper Function Implementation
 */

#include "sni_api_lv_special.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include "eos_mem.h"
#include "lvgl.h"
#include "sni_api_export.h"
#include "sni_type_bridge.h"
#include "sni_types.h"

/* Macros and Definitions -------------------------------------*/

/* Types ------------------------------------------------------*/

typedef struct sni_btnm_map_ctx
{
    lv_obj_t *obj;
    const char **map;
    uint32_t map_size;
    uint32_t button_count;
} sni_btnm_map_ctx_t;

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void sni_btnm_free_map(const char **map, uint32_t map_size)
{
    uint32_t i;

    if (!map)
    {
        return;
    }

    for (i = 0; i < map_size; i++)
    {
        if (map[i])
        {
            eos_free((void *)map[i]);
        }
    }

    eos_free((void *)map);
}

static void sni_btnm_release_ctx(lv_obj_t *obj)
{
    sni_control_block_t *cb = (sni_control_block_t *)lv_obj_get_user_data(obj);
    if (!cb)
    {
        return;
    }

    sni_btnm_map_ctx_t *ctx = (sni_btnm_map_ctx_t *)cb->aux;
    if (!ctx)
    {
        return;
    }

    cb->aux = NULL;
    sni_btnm_free_map(ctx->map, ctx->map_size);
    eos_free(ctx);
}

static bool sni_btnm_store_map(lv_obj_t *obj, const char **map, uint32_t map_size, uint32_t button_count)
{
    sni_control_block_t *cb = (sni_control_block_t *)lv_obj_get_user_data(obj);
    if (!cb)
    {
        return false;
    }

    sni_btnm_map_ctx_t *ctx = (sni_btnm_map_ctx_t *)cb->aux;
    if (!ctx)
    {
        ctx = eos_malloc_zeroed(sizeof(sni_btnm_map_ctx_t));
        if (!ctx)
        {
            return false;
        }
        ctx->obj = obj;
        cb->aux = ctx;
    }
    else
    {
        sni_btnm_free_map(ctx->map, ctx->map_size);
    }

    ctx->map = map;
    ctx->map_size = map_size;
    ctx->button_count = button_count;
    return true;
}

static sni_btnm_map_ctx_t *sni_btnm_find_ctx(lv_obj_t *obj)
{
    sni_control_block_t *cb = (sni_control_block_t *)lv_obj_get_user_data(obj);
    if (!cb)
    {
        return NULL;
    }
    return (sni_btnm_map_ctx_t *)cb->aux;
}

static char *sni_btnm_dup_literal(const char *literal)
{
    size_t len = strlen(literal);
    char *out = eos_malloc(len + 1);

    if (!out)
    {
        return NULL;
    }

    memcpy(out, literal, len + 1);
    return out;
}

static char *sni_btnm_dup_js_string(jerry_value_t js_value)
{
    jerry_size_t len;
    char *out;

    if (!jerry_value_is_string(js_value))
    {
        return NULL;
    }

    len = jerry_string_size(js_value, JERRY_ENCODING_UTF8);
    out = eos_malloc(len + 1);
    if (!out)
    {
        return NULL;
    }

    jerry_string_to_buffer(js_value, JERRY_ENCODING_UTF8, (jerry_char_t *)out, len);
    out[len] = '\0';
    return out;
}

static bool sni_btnm_build_map_from_2d_array(jerry_value_t js_rows,
                                             const char ***out_map,
                                             uint32_t *out_map_size,
                                             uint32_t *out_button_count)
{
    jerry_length_t row_len;
    uint32_t total_cells = 0;
    uint32_t button_count = 0;
    const char **map = NULL;
    uint32_t map_index = 0;
    uint32_t row_idx;

    if (!jerry_value_is_array(js_rows))
    {
        return false;
    }

    row_len = jerry_array_length(js_rows);
    if (row_len == 0)
    {
        return false;
    }

    for (row_idx = 0; row_idx < row_len; row_idx++)
    {
        jerry_value_t js_row = jerry_object_get_index(js_rows, row_idx);
        jerry_length_t col_len;

        if (!jerry_value_is_array(js_row))
        {
            jerry_value_free(js_row);
            return false;
        }

        col_len = jerry_array_length(js_row);
        jerry_value_free(js_row);
        if (col_len == 0)
        {
            return false;
        }

        total_cells += (uint32_t)col_len;
        button_count += (uint32_t)col_len;
        if (row_idx + 1 < row_len)
        {
            total_cells += 1;
        }
    }

    map = eos_malloc_zeroed(sizeof(char *) * (total_cells + 1));
    if (!map)
    {
        return false;
    }

    for (row_idx = 0; row_idx < row_len; row_idx++)
    {
        jerry_value_t js_row = jerry_object_get_index(js_rows, row_idx);
        jerry_length_t col_len = jerry_array_length(js_row);
        uint32_t col_idx;

        for (col_idx = 0; col_idx < col_len; col_idx++)
        {
            jerry_value_t js_text = jerry_object_get_index(js_row, col_idx);
            char *dup = sni_btnm_dup_js_string(js_text);
            jerry_value_free(js_text);

            if (!dup || dup[0] == '\0' || strcmp(dup, "\n") == 0)
            {
                if (dup)
                {
                    eos_free(dup);
                }
                jerry_value_free(js_row);
                sni_btnm_free_map(map, map_index);
                return false;
            }

            map[map_index++] = dup;
        }

        jerry_value_free(js_row);

        if (row_idx + 1 < row_len)
        {
            char *newline = sni_btnm_dup_literal("\n");
            if (!newline)
            {
                sni_btnm_free_map(map, map_index);
                return false;
            }
            map[map_index++] = newline;
        }
    }

    map[map_index] = sni_btnm_dup_literal("");
    if (!map[map_index])
    {
        sni_btnm_free_map(map, map_index);
        return false;
    }
    map_index += 1;

    *out_map = map;
    *out_map_size = map_index;
    *out_button_count = button_count;
    return true;
}

static bool sni_btnm_build_ctrl_map(jerry_value_t js_ctrl, uint32_t expected_len, lv_buttonmatrix_ctrl_t **out_ctrl)
{
    jerry_length_t ctrl_len;
    lv_buttonmatrix_ctrl_t *ctrl_map;
    uint32_t idx;

    if (!jerry_value_is_array(js_ctrl))
    {
        return false;
    }

    ctrl_len = jerry_array_length(js_ctrl);
    if (ctrl_len != expected_len)
    {
        return false;
    }

    ctrl_map = eos_malloc(sizeof(lv_buttonmatrix_ctrl_t) * expected_len);
    if (!ctrl_map)
    {
        return false;
    }

    for (idx = 0; idx < expected_len; idx++)
    {
        jerry_value_t js_val = jerry_object_get_index(js_ctrl, idx);

        if (!jerry_value_is_number(js_val))
        {
            jerry_value_free(js_val);
            eos_free(ctrl_map);
            return false;
        }

        ctrl_map[idx] = (lv_buttonmatrix_ctrl_t)sni_tb_js2c_uint32(js_val);
        jerry_value_free(js_val);
    }

    *out_ctrl = ctrl_map;
    return true;
}

static uint32_t sni_btnm_count_buttons_from_map(const char *const *map)
{
    uint32_t count = 0;
    uint32_t idx = 0;

    if (!map)
    {
        return 0;
    }

    while (map[idx])
    {
        const char *entry = map[idx];
        if (entry[0] == '\0')
        {
            break;
        }

        if (strcmp(entry, "\n") != 0)
        {
            count++;
        }

        idx++;
    }

    return count;
}

static void sni_btnm_obj_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    sni_btnm_release_ctx(obj);
}

jerry_value_t sni_api_ctor_buttonmatrix(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    lv_obj_t *arg_parent = NULL;
    lv_obj_t *native_obj = NULL;

    if (jerry_value_is_undefined(call_info_p->new_target))
    {
        return sni_api_throw_error("Constructor must be called with new");
    }

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!sni_tb_js2c(args_p[0], SNI_H_LV_OBJ, &arg_parent))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    native_obj = lv_buttonmatrix_create(arg_parent);
    if (!native_obj)
    {
        return sni_api_throw_error("Failed to create buttonmatrix");
    }

    if (!sni_tb_c2js_set_object(&native_obj, SNI_H_LV_OBJ, call_info_p->this_value))
    {
        return sni_api_throw_error("Failed to bind native object");
    }

    lv_obj_add_event_cb(native_obj, sni_btnm_obj_delete_cb, LV_EVENT_DELETE, NULL);
    return jerry_undefined();
}

jerry_value_t sni_api_lv_buttonmatrix_set_map(const jerry_call_info_t *call_info_p,
                                              const jerry_value_t args_p[],
                                              const jerry_length_t args_count)
{
    lv_obj_t *self_obj = NULL;
    sni_btnm_map_ctx_t *ctx = NULL;
    const char **map = NULL;
    uint32_t map_size = 0;
    uint32_t button_count = 0;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!sni_btnm_build_map_from_2d_array(args_p[0], &map, &map_size, &button_count))
    {
        return sni_api_throw_error("Invalid map data, expected string[][]");
    }

    if (!sni_btnm_store_map(self_obj, map, map_size, button_count))
    {
        sni_btnm_free_map(map, map_size);
        return sni_api_throw_error("Out of memory");
    }

    ctx = sni_btnm_find_ctx(self_obj);
    if (!ctx)
    {
        return sni_api_throw_error("Failed to resolve map context");
    }

    lv_obj_add_event_cb(self_obj, sni_btnm_obj_delete_cb, LV_EVENT_DELETE, NULL);
    lv_buttonmatrix_set_map(self_obj, ctx->map);
    return jerry_undefined();
}

jerry_value_t sni_api_lv_buttonmatrix_set_ctrl_map(const jerry_call_info_t *call_info_p,
                                                   const jerry_value_t args_p[],
                                                   const jerry_length_t args_count)
{
    lv_obj_t *self_obj = NULL;
    uint32_t expected_len;
    const char *const *map = NULL;
    lv_buttonmatrix_ctrl_t *ctrl_map = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    map = lv_buttonmatrix_get_map(self_obj);
    expected_len = sni_btnm_count_buttons_from_map(map);
    if (expected_len == 0)
    {
        return sni_api_throw_error("Buttonmatrix has no buttons");
    }

    if (!sni_btnm_build_ctrl_map(args_p[0], expected_len, &ctrl_map))
    {
        return sni_api_throw_error("Invalid ctrl map data");
    }

    lv_buttonmatrix_set_ctrl_map(self_obj, ctrl_map);
    eos_free(ctrl_map);
    return jerry_undefined();
}
