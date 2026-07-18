/**
 * @file eos_corner_radius.c
 * @brief Arbitrary corner radius background widget
 */

#include "eos_corner_radius.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#define EOS_LOG_TAG "CornerRadius"
#include "eos_log.h"
#include "eos_mem.h"
#include "eos_service_cache.h"
#include "eos_config.h"

/* Macros and Definitions -------------------------------------*/

#define _MAX_CANVAS_SIZE EOS_DISPLAY_WIDTH *EOS_DISPLAY_HEIGHT *lv_color_format_get_size(LV_COLOR_FORMAT_ARGB8888)

/* Variables --------------------------------------------------*/

static uint32_t _corner_radius_event_id = 0;

/* Function Implementations -----------------------------------*/

static void _corner_radius_event_init(void)
{
    if (_corner_radius_event_id == 0)
    {
        _corner_radius_event_id = lv_event_register_id();
    }
}

static void _corner_radius_buffer_free(void *user_data)
{
    lv_image_dsc_t *dsc = user_data;
    EOS_CHECK_PTR_RETURN(dsc);
    if (dsc->data)
    {
        eos_cache_buf_free((void *)dsc->data);
        dsc->data = NULL;
    }
    eos_free(dsc);
    EOS_LOG_I("Rounded corner buffer cleared");
}

static void _obj_corner_radius_canvas_buffer_delete_cb(lv_event_t *e)
{
    lv_image_dsc_t *dsc = lv_event_get_user_data(e);
    _corner_radius_buffer_free(dsc);
}

void eos_obj_set_corner_radius_bg(lv_obj_t *obj, eos_corner_round_t corners, lv_coord_t radius, lv_color_t color)
{
    EOS_CHECK_PTR_RETURN(obj);
    _corner_radius_event_init();

    lv_obj_update_layout(obj);
    lv_coord_t obj_w = lv_obj_get_width(obj);
    lv_coord_t obj_h = lv_obj_get_height(obj);

    if (obj_w <= 0 || obj_h <= 0)
    {
        EOS_LOG_E("Invalid object size: %dx%d", obj_w, obj_h);
        return;
    }

    lv_coord_t max_r = LV_MIN(obj_w, obj_h) / 2;
    radius = LV_MIN(radius, max_r);

    if (radius == 0)
    {
        EOS_LOG_W("Invalid object radius: %d", radius);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(obj, 0, 0);
        lv_obj_set_style_bg_color(obj, color, 0);
        lv_obj_set_style_shadow_width(obj, 0, 0);
        lv_obj_set_style_bg_image_src(obj, NULL, 0);
        return;
    }

    static const uint32_t cf_bytes = 4;
    const uint32_t canvas_buf_size = (uint32_t)obj_w * obj_h * cf_bytes;

    if (canvas_buf_size > _MAX_CANVAS_SIZE)
    {
        EOS_LOG_E("Canvas buffer too large: %u bytes", canvas_buf_size);
        return;
    }

    uint8_t *canvas_buf = eos_cache_buf_alloc(canvas_buf_size);
    if (!canvas_buf)
    {
        EOS_LOG_E("Failed to allocate canvas buffer: %u bytes", canvas_buf_size);
        return;
    }

    lv_obj_t *canvas = lv_canvas_create(lv_screen_active());
    EOS_CHECK_PTR_RETURN_FREE(canvas, canvas_buf);

    lv_obj_remove_style_all(canvas);
    lv_canvas_set_buffer(canvas, canvas_buf, obj_w, obj_h, LV_COLOR_FORMAT_ARGB8888);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_rect_dsc_t rect_main;
    lv_draw_rect_dsc_init(&rect_main);
    rect_main.radius = radius;
    rect_main.bg_opa = LV_OPA_COVER;
    rect_main.bg_color = color;

    lv_area_t coords = {0, 0, obj_w - 1, obj_h - 1};
    lv_draw_rect(&layer, &rect_main, &coords);

    if (radius > 0)
    {
        lv_draw_rect_dsc_t corner_rect_dsc;
        lv_draw_rect_dsc_init(&corner_rect_dsc);
        corner_rect_dsc.bg_opa = LV_OPA_COVER;
        corner_rect_dsc.bg_color = color;
        corner_rect_dsc.radius = 0;

        const lv_area_t corners_areas[] = {{0, 0, radius, radius},
                                           {obj_w - radius, 0, obj_w - 1, radius},
                                           {obj_w - radius, obj_h - radius, obj_w - 1, obj_h - 1},
                                           {0, obj_h - radius, radius, obj_h - 1}};

        const eos_corner_round_t corner_flags[] = {EOS_ROUND_TOP_LEFT,
                                                   EOS_ROUND_TOP_RIGHT,
                                                   EOS_ROUND_BOTTOM_RIGHT,
                                                   EOS_ROUND_BOTTOM_LEFT};

        for (int i = 0; i < 4; i++)
        {
            if (!(corners & corner_flags[i]))
            {
                lv_draw_rect(&layer, &corner_rect_dsc, &corners_areas[i]);
            }
        }
    }

    lv_canvas_finish_layer(canvas, &layer);
    lv_obj_delete_async(canvas);

    lv_image_dsc_t *dsc = eos_malloc_zeroed(sizeof(lv_image_dsc_t));
    EOS_CHECK_PTR_RETURN_FREE(dsc, canvas_buf);

    *dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
                                       .cf = LV_COLOR_FORMAT_ARGB8888,
                                       .flags = 0,
                                       .w = obj_w,
                                       .h = obj_h,
                                       .stride = obj_w * cf_bytes},
                            .data = canvas_buf,
                            .data_size = canvas_buf_size};

    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);

    lv_obj_send_event(obj, _corner_radius_event_id, NULL);

    lv_obj_remove_event_cb(obj, _obj_corner_radius_canvas_buffer_delete_cb);

    if (lv_obj_add_event_cb(obj, _obj_corner_radius_canvas_buffer_delete_cb, LV_EVENT_DELETE, dsc) == NULL)
    {
        eos_free(dsc);
        eos_free(canvas_buf);
        return;
    }

    if (lv_obj_add_event_cb(obj, _obj_corner_radius_canvas_buffer_delete_cb, _corner_radius_event_id, dsc) == NULL)
    {
        eos_free(dsc);
        eos_free(canvas_buf);
        return;
    }

    lv_obj_set_style_bg_image_src(obj, dsc, 0);
}

void eos_obj_remove_corner_radius_bg(lv_obj_t *obj)
{
    EOS_CHECK_PTR_RETURN(obj);
    _corner_radius_event_init();
    lv_obj_send_event(obj, _corner_radius_event_id, NULL);
    lv_obj_remove_event_cb(obj, _obj_corner_radius_canvas_buffer_delete_cb);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_image_src(obj, NULL, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
}
