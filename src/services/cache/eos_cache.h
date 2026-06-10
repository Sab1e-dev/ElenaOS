/**
 * @file eos_cache.h
 * @brief Generic LVGL image decode cache module
 *
 * Three modes (configured in eos_config.h):
 *   0 = cache disabled (EOS_CACHE_ENABLE=0)
 *   1 = user-managed (call eos_image_cache_init() before widgets)
 *   2 = ElenixOS-managed (auto-init via system startup)
 *
 * On SiFli with PSRAM, setting EOS_CACHE_USE_PSRAM=1 overrides the
 * LVGL image decode buffer allocator to route decoded pixel data
 * through eos_cache_psram_malloc / eos_cache_psram_free.
 * Platform ports provide the weak default; SiFli port overrides with
 * app_cache_alloc(IMAGE_CACHE_PSRAM).
 */
#ifndef EOS_CACHE_H
#define EOS_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "eos_config.h"
#include <stddef.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief Initialise LVGL image decode cache with explicit sizes
 *        and optionally override the draw-buf allocator for PSRAM.
 *
 * Safe to call multiple times — no-op after the first successful
 * init or if LVGL cache was already configured by the user.
 *
 * @param image_size  Max decoded image data to retain (bytes).
 * @param header_cnt  Max image header cache entries.
 */
void eos_image_cache_init(uint32_t image_size, uint32_t header_cnt);

/**
 * @brief Convenience — calls eos_image_cache_init with
 *        EOS_CACHE_IMAGE_SIZE and EOS_CACHE_IMAGE_HEADER_CNT
 *        from eos_config.h.
 */
void eos_image_cache_init_default(void);

/**
 * @brief Platform PSRAM allocator for decoded pixel data.
 *
 * Weak default falls back to lv_malloc / lv_free.
 * Override in a platform port (e.g. SiFli) to allocate from
 * a dedicated PSRAM memheap.
 *
 * @param size  Allocation size in bytes.
 * @return      Pointer to allocated memory, or NULL.
 */
void * eos_cache_psram_malloc(size_t size);

/**
 * @brief Free memory previously allocated by eos_cache_psram_malloc.
 * @param ptr  Pointer to free.
 */
void eos_cache_psram_free(void * ptr);

#ifdef __cplusplus
}
#endif

#endif /* EOS_CACHE_H */
