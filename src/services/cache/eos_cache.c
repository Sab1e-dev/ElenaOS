/**
 * @file eos_cache.c
 * @brief Generic LVGL image decode cache module
 */

#include "eos_cache.h"

/* Includes ---------------------------------------------------*/
#include "eos_port.h"
#include "lvgl.h"
#include "src/misc/cache/lv_image_cache.h"
#include "src/misc/cache/lv_image_header_cache.h"
#include "src/draw/lv_draw_buf.h"
#include "src/core/lv_global.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

/* ── Weak default PSRAM allocator (falls back to lv_malloc) ─ */

EOS_WEAK void * eos_cache_psram_malloc(size_t size)
{
    return lv_malloc(size);
}

EOS_WEAK void eos_cache_psram_free(void * ptr)
{
    lv_free(ptr);
}

/* ── Custom draw buffer callbacks for PSRAM ───────────────── */

static void * psram_buf_malloc(size_t size_bytes, lv_color_format_t color_format)
{
    LV_UNUSED(color_format);
    size_bytes += LV_DRAW_BUF_ALIGN - 1;
    return eos_cache_psram_malloc(size_bytes);
}

static void psram_buf_free(void * buf)
{
    eos_cache_psram_free(buf);
}

static void * eos_buf_align(void * buf, lv_color_format_t color_format)
{
    LV_UNUSED(color_format);
    uint8_t * buf_u8 = buf;
    if(buf_u8) {
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

/* ── Public API ───────────────────────────────────────────── */

void eos_image_cache_init(uint32_t image_size, uint32_t header_cnt)
{
#if EOS_CACHE_ENABLE
    if(lv_image_cache_is_enabled()) return;

    if(image_size > 0) {
        lv_image_cache_resize(image_size, false);
    }
    if(header_cnt > 0) {
        lv_image_header_cache_resize(header_cnt, false);
    }

#if EOS_CACHE_USE_PSRAM
    {
        lv_draw_buf_handlers_t * handlers =
            &LV_GLOBAL_DEFAULT()->image_cache_draw_buf_handlers;

        lv_draw_buf_handlers_init(handlers,
                                  psram_buf_malloc,
                                  psram_buf_free,
                                  eos_buf_align,
                                  NULL,    /* invalidate_cache */
                                  NULL,    /* flush_cache */
                                  eos_width_to_stride);
    }
#endif /* EOS_CACHE_USE_PSRAM */
#endif /* EOS_CACHE_ENABLE */
}

void eos_image_cache_init_default(void)
{
    eos_image_cache_init(EOS_CACHE_IMAGE_SIZE, EOS_CACHE_IMAGE_HEADER_CNT);
}
