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
/* Macros and Definitions -------------------------------------*/

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
