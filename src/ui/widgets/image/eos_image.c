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
