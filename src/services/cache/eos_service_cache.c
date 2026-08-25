/**
 * @file eos_service_cache.c
 * @brief Image cache service
 */

#include "eos_service_cache.h"

/* Includes ---------------------------------------------------*/
#include "eos_port.h"
#include "lvgl.h"
#include "lvgl_private.h"
#include "core/lv_global.h"
#include "draw/lv_draw_buf.h"
#include "misc/cache/instance/lv_image_cache.h"
#include "misc/cache/instance/lv_image_header_cache.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

EOS_WEAK void *eos_cache_buf_alloc(size_t size)
{
    return lv_malloc(size);
}

EOS_WEAK void eos_cache_buf_free(void *ptr)
{
    lv_free(ptr);
}

static void *dedicated_buf_malloc(size_t size_bytes, lv_color_format_t color_format)
{
    LV_UNUSED(color_format);
    size_bytes += LV_DRAW_BUF_ALIGN - 1;
    return eos_cache_buf_alloc(size_bytes);
}

static void dedicated_buf_free(void *buf)
{
    eos_cache_buf_free(buf);
}

static void *eos_buf_align(void *buf, lv_color_format_t color_format)
{
    LV_UNUSED(color_format);
    uint8_t *buf_u8 = buf;
    if (buf_u8)
    {
        buf_u8 = (uint8_t *)LV_ROUND_UP((lv_uintptr_t)buf_u8, LV_DRAW_BUF_ALIGN);
    }
    return buf_u8;
}

static uint32_t eos_width_to_stride(uint32_t w, lv_color_format_t color_format)
{
    uint32_t width_byte;
    width_byte = w * lv_color_format_get_bpp(color_format);
    width_byte = (width_byte + 7) >> 3;
    return width_byte;
}

void eos_service_cache_init(void)
{
#if EOS_CACHE_ENABLE
    if (lv_image_cache_is_enabled())
        return;

    lv_image_cache_resize(EOS_CACHE_SIZE, false);
    lv_image_header_cache_resize(EOS_CACHE_HEADER_COUNT, false);

#if EOS_CACHE_USE_DEDICATED_MEM
    {
        lv_draw_buf_handlers_t *handlers = &LV_GLOBAL_DEFAULT()->image_cache_draw_buf_handlers;

        lv_draw_buf_handlers_init(handlers,
                                  dedicated_buf_malloc,
                                  dedicated_buf_free,
                                  NULL, /* buf_copy */
                                  eos_buf_align,
                                  NULL, /* invalidate_cache */
                                  NULL, /* flush_cache */
                                  eos_width_to_stride);
    }
#endif /* EOS_CACHE_USE_DEDICATED_MEM */
#endif /* EOS_CACHE_ENABLE */
}
