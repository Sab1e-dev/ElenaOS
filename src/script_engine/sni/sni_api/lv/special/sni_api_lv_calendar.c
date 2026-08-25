/**
 * @file sni_api_lv_calendar.c
 * @brief LVGL calendar SNI special wrappers
 */

#include "sni_api_lv_special.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include "eos_mem.h"
#include "lvgl.h"
#include "sni_api_export.h"
#include "sni_type_bridge.h"
#include "sni_types.h"
/* Types ------------------------------------------------------*/

typedef struct sni_calendar_ctx
{
    lv_obj_t *obj;
    char **day_names;
    uint32_t day_count;
    lv_calendar_date_t *highlighted;
    uint32_t highlighted_count;
} sni_calendar_ctx_t;

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void sni_calendar_free_day_names(char **day_names, uint32_t day_count)
{
    uint32_t i;

    if (!day_names)
    {
        return;
    }

    for (i = 0; i < day_count; i++)
    {
        if (day_names[i])
        {
            eos_free(day_names[i]);
        }
    }

    eos_free(day_names);
}

static void sni_calendar_release_ctx(lv_obj_t *obj)
{
    sni_control_block_t *cb = sni_cb_from_obj(obj);
    if (!cb)
    {
        return;
    }

    sni_calendar_ctx_t *ctx = (sni_calendar_ctx_t *)cb->aux;
    if (!ctx)
    {
        return;
    }

    cb->aux = NULL;
    sni_calendar_free_day_names(ctx->day_names, ctx->day_count);
    if (ctx->highlighted)
    {
        eos_free(ctx->highlighted);
    }
    eos_free(ctx);
}

static sni_calendar_ctx_t *sni_calendar_find_or_create_ctx(lv_obj_t *obj)
{
    sni_control_block_t *cb = sni_cb_from_obj(obj);
    if (!cb)
    {
        return NULL;
    }

    if (cb->aux)
    {
        return (sni_calendar_ctx_t *)cb->aux;
    }

    sni_calendar_ctx_t *ctx = eos_malloc_zeroed(sizeof(sni_calendar_ctx_t));
    if (!ctx)
    {
        return NULL;
    }

    ctx->obj = obj;
    cb->aux = ctx;
    return ctx;
}

static char *sni_calendar_dup_js_string(jerry_value_t js_value)
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

static bool sni_calendar_build_day_names(jerry_value_t js_array, char ***out_names)
{
    jerry_length_t len;
    uint32_t i;
    char **names;

    if (!jerry_value_is_array(js_array))
    {
        return false;
    }

    len = jerry_array_length(js_array);
    if (len != 7)
    {
        return false;
    }

    names = eos_malloc_zeroed(sizeof(char *) * 7);
    if (!names)
    {
        return false;
    }

    for (i = 0; i < 7; i++)
    {
        jerry_value_t js_name = jerry_object_get_index(js_array, i);
        names[i] = sni_calendar_dup_js_string(js_name);
        jerry_value_free(js_name);

        if (!names[i] || names[i][0] == '\0')
        {
            sni_calendar_free_day_names(names, 7);
            return false;
        }
    }

    *out_names = names;
    return true;
}

static bool sni_calendar_parse_date_object(jerry_value_t js_date, lv_calendar_date_t *out_date)
{
    jerry_value_t js_year;
    jerry_value_t js_month;
    jerry_value_t js_day;

    if (!jerry_value_is_object(js_date))
    {
        return false;
    }

    js_year = jerry_object_get_sz(js_date, "year");
    js_month = jerry_object_get_sz(js_date, "month");
    js_day = jerry_object_get_sz(js_date, "day");

    if (!jerry_value_is_number(js_year) || !jerry_value_is_number(js_month) || !jerry_value_is_number(js_day))
    {
        jerry_value_free(js_year);
        jerry_value_free(js_month);
        jerry_value_free(js_day);
        return false;
    }

    out_date->year = (uint16_t)sni_tb_js2c_uint32(js_year);
    out_date->month = (int8_t)sni_tb_js2c_int32(js_month);
    out_date->day = (int8_t)sni_tb_js2c_int32(js_day);

    jerry_value_free(js_year);
    jerry_value_free(js_month);
    jerry_value_free(js_day);

    return true;
}

static bool sni_calendar_build_highlighted_dates(jerry_value_t js_dates,
                                                 uint32_t expected_count,
                                                 lv_calendar_date_t **out_dates)
{
    jerry_length_t len;
    uint32_t i;
    lv_calendar_date_t *dates;

    if (!jerry_value_is_array(js_dates))
    {
        return false;
    }

    len = jerry_array_length(js_dates);
    if ((uint32_t)len < expected_count)
    {
        return false;
    }

    dates = eos_malloc_zeroed(sizeof(lv_calendar_date_t) * expected_count);
    if (!dates)
    {
        return false;
    }

    for (i = 0; i < expected_count; i++)
    {
        jerry_value_t js_date = jerry_object_get_index(js_dates, i);
        bool ok = sni_calendar_parse_date_object(js_date, &dates[i]);
        jerry_value_free(js_date);

        if (!ok)
        {
            eos_free(dates);
            return false;
        }
    }

    *out_dates = dates;
    return true;
}

static void sni_calendar_obj_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    sni_calendar_release_ctx(obj);
}

jerry_value_t sni_api_lv_calendar_set_day_names(const jerry_call_info_t *call_info_p,
                                                const jerry_value_t args_p[],
                                                const jerry_length_t args_count)
{
    lv_obj_t *self_obj = NULL;
    sni_calendar_ctx_t *ctx = NULL;
    char **day_names = NULL;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!sni_calendar_build_day_names(args_p[0], &day_names))
    {
        return sni_api_throw_error("Invalid day names, expected string[7]");
    }

    ctx = sni_calendar_find_or_create_ctx(self_obj);
    if (!ctx)
    {
        sni_calendar_free_day_names(day_names, 7);
        return sni_api_throw_error("Out of memory");
    }

    sni_calendar_free_day_names(ctx->day_names, ctx->day_count);
    ctx->day_names = day_names;
    ctx->day_count = 7;

    lv_obj_add_event_cb(self_obj, sni_calendar_obj_delete_cb, LV_EVENT_DELETE, NULL);
    lv_calendar_set_day_names(self_obj, (const char **)ctx->day_names);
    return jerry_undefined();
}

jerry_value_t sni_api_lv_calendar_set_highlighted_dates(const jerry_call_info_t *call_info_p,
                                                        const jerry_value_t args_p[],
                                                        const jerry_length_t args_count)
{
    lv_obj_t *self_obj = NULL;
    sni_calendar_ctx_t *ctx = NULL;
    uint32_t date_num;
    lv_calendar_date_t *dates = NULL;

    if (args_count != 2)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_number(args_p[1]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    date_num = sni_tb_js2c_uint32(args_p[1]);
    if (date_num == 0)
    {
        return sni_api_throw_error("Invalid date count");
    }

    if (!sni_calendar_build_highlighted_dates(args_p[0], date_num, &dates))
    {
        return sni_api_throw_error("Invalid highlighted dates");
    }

    ctx = sni_calendar_find_or_create_ctx(self_obj);
    if (!ctx)
    {
        eos_free(dates);
        return sni_api_throw_error("Out of memory");
    }

    if (ctx->highlighted)
    {
        eos_free(ctx->highlighted);
    }
    ctx->highlighted = dates;
    ctx->highlighted_count = date_num;

    lv_obj_add_event_cb(self_obj, sni_calendar_obj_delete_cb, LV_EVENT_DELETE, NULL);
    lv_calendar_set_highlighted_dates(self_obj, ctx->highlighted, ctx->highlighted_count);
    return jerry_undefined();
}

jerry_value_t sni_api_lv_calendar_set_chinese_mode(const jerry_call_info_t *call_info_p,
                                                   const jerry_value_t args_p[],
                                                   const jerry_length_t args_count)
{
    lv_obj_t *self_obj = NULL;
    bool enabled;

    if (args_count != 1)
    {
        return sni_api_throw_error("Invalid argument count");
    }

    if (!sni_tb_js2c(call_info_p->this_value, SNI_H_LV_OBJ, &self_obj))
    {
        return sni_api_throw_error("Failed to convert argument");
    }

    if (!jerry_value_is_boolean(args_p[0]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    enabled = sni_tb_js2c_boolean(args_p[0]);

#if LV_USE_CALENDAR && LV_USE_CALENDAR_CHINESE
    lv_calendar_set_chinese_mode(self_obj, enabled);
    return jerry_undefined();
#else
    (void)enabled;
    return sni_api_throw_error("Calendar Chinese mode is disabled");
#endif
}
