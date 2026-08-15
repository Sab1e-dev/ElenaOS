/**
 * @file eos_image.c
 * @brief Image display
 */

#include "eos_image.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#define EOS_LOG_TAG "Image"
#include "eos_log.h"
#include "eos_mem.h"
#include "eos_font.h"
/* Macros and Definitions -------------------------------------*/
LV_FONT_DECLARE(EOS_FONT_ICON);

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

void eos_img_set_size(lv_obj_t *img_obj, uint32_t w, uint32_t h)
{
    EOS_CHECK_PTR_RETURN(img_obj);

    const void *src = lv_image_get_src(img_obj);
    if (!src)
    {
        EOS_LOG_E("Image src is NULL");
        return;
    }

    lv_image_header_t header;
    if (lv_image_decoder_get_info(src, &header) != LV_RESULT_OK)
    {
        EOS_LOG_E("Failed to get image info");
        return;
    }

    if (header.w == 0 || header.h == 0)
    {
        EOS_LOG_E("Invalid image size");
        return;
    }

    lv_obj_set_size(img_obj, w, h);

    lv_image_set_scale_x(img_obj, (w << 8) / header.w); // 256 = 1<<8
    lv_image_set_scale_y(img_obj, (h << 8) / header.h);
}

lv_obj_t *eos_circle_image_create(lv_obj_t *parent, const void *src, lv_coord_t size)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    /* Circular clipping container */
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, size, size);
    lv_obj_set_style_radius(container, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(container, true, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    if (!src)
    {
        return container;
    }

    /* Inner image */
    lv_obj_t *img = lv_image_create(container);
    lv_image_set_src(img, src);
    lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(img, 0, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_SCROLLABLE);

    /* Scale image to fill the circular area (cover mode) */
    lv_image_header_t header;
    if (lv_image_decoder_get_info(src, &header) == LV_RESULT_OK && header.w > 0 && header.h > 0)
    {
        /* Use the larger scale dimension so the image covers the entire circle */
        uint32_t scale_x = (size << 8) / header.w;
        uint32_t scale_y = (size << 8) / header.h;
        uint32_t scale = LV_MAX(scale_x, scale_y);

        lv_obj_set_size(img, size, size);
        lv_image_set_scale_x(img, scale);
        lv_image_set_scale_y(img, scale);
    }
    else
    {
        lv_obj_set_size(img, size, size);
    }

    lv_obj_center(img);

    /* Bubble events up so the parent button/label receives clicks */
    lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(container, LV_OBJ_FLAG_EVENT_BUBBLE);

    return container;
}

/* Icon glyph image -------------------------------------------*/

/** @brief Per-image state: owns the A8 pixel buffer and the image descriptor */
typedef struct
{
    lv_image_dsc_t dsc; /**< Image descriptor referencing the glyph A8 pixels */
    lv_draw_buf_t *draw_buf; /**< Draw buffer that owns the A8 pixels */
} _icon_glyph_ctx_t;

/**
 * @brief Decode the first UTF-8 code point of a string (icon glyphs are single chars)
 */
static uint32_t _utf8_decode_first(const char *s)
{
    const uint8_t *p = (const uint8_t *)s;
    if (p[0] < 0x80)
    {
        return p[0];
    }
    else if ((p[0] & 0xE0) == 0xC0)
    {
        return ((uint32_t)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    }
    else if ((p[0] & 0xF0) == 0xE0)
    {
        return ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    }
    else if ((p[0] & 0xF8) == 0xF0)
    {
        return ((uint32_t)(p[0] & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) | ((uint32_t)(p[2] & 0x3F) << 6)
               | (p[3] & 0x3F);
    }
    return 0;
}

static void _icon_glyph_delete_cb(lv_event_t *e)
{
    _icon_glyph_ctx_t *ctx = (_icon_glyph_ctx_t *)lv_event_get_user_data(e);
    if (!ctx)
    {
        return;
    }
    if (ctx->draw_buf)
    {
        lv_draw_buf_destroy(ctx->draw_buf);
    }
    eos_free(ctx);
}

lv_obj_t *eos_icon_glyph_image_create(lv_obj_t *parent, const void *icon_src, lv_coord_t size)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);
    EOS_CHECK_PTR_RETURN_VAL(icon_src, NULL);

    uint32_t codepoint = _utf8_decode_first((const char *)icon_src);

    lv_font_glyph_dsc_t glyph;
    if (!lv_font_get_glyph_dsc(&EOS_FONT_ICON, &glyph, codepoint, 0))
    {
        EOS_LOG_W("Icon glyph not found: U+%04X", (unsigned)codepoint);
        return NULL;
    }

    if (glyph.box_w == 0 || glyph.box_h == 0)
    {
        EOS_LOG_W("Icon glyph has empty bitmap: U+%04X", (unsigned)codepoint);
        return NULL;
    }

    /* Render the glyph into an A8 (alpha-only) buffer */
    lv_draw_buf_t *draw_buf = lv_draw_buf_create(glyph.box_w, glyph.box_h, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
    if (!draw_buf)
    {
        EOS_LOG_E("Failed to allocate icon glyph buffer");
        return NULL;
    }
    lv_font_get_glyph_bitmap(&glyph, draw_buf);

    _icon_glyph_ctx_t *ctx = (_icon_glyph_ctx_t *)eos_malloc(sizeof(_icon_glyph_ctx_t));
    if (!ctx)
    {
        lv_draw_buf_destroy(draw_buf);
        return NULL;
    }

    ctx->draw_buf = draw_buf;
    ctx->dsc = (lv_image_dsc_t){
        .header =
            {
                .magic = LV_IMAGE_HEADER_MAGIC,
                .cf = LV_COLOR_FORMAT_A8,
                .w = glyph.box_w,
                .h = glyph.box_h,
                .stride = draw_buf->header.stride,
            },
        .data_size = draw_buf->data_size,
        .data = draw_buf->data,
        .reserved = NULL,
    };

    lv_obj_t *img = lv_image_create(parent);
    lv_obj_set_size(img, size, size);
    lv_image_set_src(img, &ctx->dsc);
    lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CENTER);
    /* A8 is alpha-only: fill the RGB channels with white so the glyph renders white */
    lv_obj_set_style_image_recolor(img, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(img, 0, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_user_data(img, ctx);
    lv_obj_add_event_cb(img, _icon_glyph_delete_cb, LV_EVENT_DELETE, NULL);

    return img;
}
