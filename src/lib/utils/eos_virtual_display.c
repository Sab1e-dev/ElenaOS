/**
 * @file eos_virtual_display.c
 * @brief Virtual display
 */

#include "eos_virtual_display.h"

#if EOS_USE_VIRTUAL_DISPLAY

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EOS_LOG_TAG "VirtualDisplay"
#include "eos_log.h"
#include "eos_port.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/

/* Toggle to log flush areas (virtual display only) */
#define EOS_VIRTUAL_DISP_FLUSH_DIAG 0

struct eos_virtual_display_t
{
    lv_obj_t *canvas;
    lv_color_t *canvas_buf;
    lv_color_t *disp_buf;
    lv_coord_t hor_res;
    lv_coord_t ver_res;
    lv_color_format_t cf;
    lv_display_t *disp;
    /* Touch device */
    lv_indev_t *indev;
    int32_t touch_x;
    int32_t touch_y;
    bool pressed;
};

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _virtual_display_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    eos_virtual_display_t *vd = lv_display_get_driver_data(disp);
    lv_coord_t w = lv_area_get_width(area);
    lv_coord_t h = lv_area_get_height(area);

#if EOS_VIRTUAL_DISP_FLUSH_DIAG
    EOS_LOG_E("[FLUSH_DIAG] FLUSH area=%d,%d-%d,%d (w=%d h=%d)", area->x1, area->y1, area->x2, area->y2, w, h);
#endif

    uint32_t px_size = lv_color_format_get_size(vd->cf);
    uint32_t canvas_stride = lv_draw_buf_width_to_stride(vd->hor_res, vd->cf);
    uint32_t px_map_stride = lv_draw_buf_width_to_stride(w, vd->cf);

    uint8_t *canvas_ptr = (uint8_t *)vd->canvas_buf + (area->y1 * canvas_stride + area->x1 * px_size);

    for (int32_t y = 0; y < h; y++)
    {
        memcpy(canvas_ptr, px_map, w * px_size);
        canvas_ptr += canvas_stride;
        px_map += px_map_stride;
    }
    lv_obj_invalidate(vd->canvas);
    lv_display_flush_ready(disp);
}

static void _canvas_event_cb(lv_event_t *e)
{
    eos_virtual_display_t *vd = (eos_virtual_display_t *)lv_event_get_user_data(e);
    if (!vd)
        return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *act_indev = lv_indev_get_act();
    lv_point_t pt = {0, 0};
    if (act_indev)
    {
        lv_indev_get_point(act_indev, &pt);
    }
    else
    {
        if (code == LV_EVENT_RELEASED)
            vd->pressed = false;
        return;
    }

    if (code == LV_EVENT_RELEASED)
    {
        vd->pressed = false;
        return;
    }

    lv_area_t canvas_area;
    lv_obj_get_coords(vd->canvas, &canvas_area);

    int32_t x = pt.x - canvas_area.x1;
    int32_t y = pt.y - canvas_area.y1;

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x >= vd->hor_res)
        x = vd->hor_res - 1;
    if (y >= vd->ver_res)
        y = vd->ver_res - 1;

    vd->touch_x = x;
    vd->touch_y = y;
    vd->pressed = (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING);
}

static void _virtual_input_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    eos_virtual_display_t *vd = (eos_virtual_display_t *)lv_indev_get_user_data(indev);
    if (!vd)
    {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = 0;
        data->point.y = 0;
        return;
    }

    data->point.x = vd->touch_x;
    data->point.y = vd->touch_y;
    data->state = vd->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

lv_display_t *eos_virtual_display_create(lv_obj_t *parent, lv_coord_t hor_res, lv_coord_t ver_res)
{
    eos_virtual_display_t *vd = eos_malloc_zeroed(sizeof(eos_virtual_display_t));
    if (!vd)
        return NULL;

    vd->hor_res = hor_res;
    vd->ver_res = ver_res;
    vd->cf = LV_COLOR_FORMAT_NATIVE;
    vd->touch_x = 0;
    vd->touch_y = 0;
    vd->pressed = false;
    vd->indev = NULL;

    // Allocate canvas buffer
    vd->canvas_buf = eos_malloc_zeroed(lv_draw_buf_width_to_stride(hor_res, vd->cf) * ver_res);
    if (!vd->canvas_buf)
    {
        eos_free(vd);
        return NULL;
    }

    // Create canvas
    vd->canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(vd->canvas, vd->canvas_buf, hor_res, ver_res, vd->cf);
    lv_obj_center(vd->canvas);
    lv_obj_add_flag(vd->canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(vd->canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(vd->canvas, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_event_cb(vd->canvas, _canvas_event_cb, LV_EVENT_PRESSED, vd);
    lv_obj_add_event_cb(vd->canvas, _canvas_event_cb, LV_EVENT_PRESSING, vd);
    lv_obj_add_event_cb(vd->canvas, _canvas_event_cb, LV_EVENT_RELEASED, vd);

    // Create LVGL display
    vd->disp = lv_display_create(hor_res, ver_res);
    if (!vd->disp)
    {
        eos_free(vd->canvas_buf);
        eos_free(vd);
        return NULL;
    }

    // Allocate display buffer
    size_t buf_size = hor_res * ver_res * sizeof(lv_color_t);
    vd->disp_buf = eos_malloc_zeroed(buf_size);
    if (!vd->disp_buf)
    {
        lv_display_delete(vd->disp);
        eos_free(vd->canvas_buf);
        eos_free(vd);
        return NULL;
    }

    lv_display_set_buffers(vd->disp, vd->disp_buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_driver_data(vd->disp, vd);
    lv_display_set_flush_cb(vd->disp, _virtual_display_flush_cb);

    // Create input device
    vd->indev = lv_indev_create();
    if (!vd->indev)
    {
        EOS_LOG_E("failed to create indev");
        lv_display_delete(vd->disp);
        eos_free(vd->disp_buf);
        eos_free(vd->canvas_buf);
        eos_free(vd);
        return NULL;
    }

    lv_indev_set_type(vd->indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(vd->indev, _virtual_input_read);
    lv_indev_set_user_data(vd->indev, vd);
    lv_indev_set_display(vd->indev, vd->disp);
    return vd->disp;
}

#endif /* EOS_USE_VIRTUAL_DISPLAY */
