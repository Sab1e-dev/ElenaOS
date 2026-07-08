/**
 * @file eos_service_cache.h
 * @brief Image cache service
 *
 * Cache for decoded LVGL image pixel data.  Initialised automatically
 * during system startup (see eos_core.c).
 *
 * On platforms with a dedicated memory pool (e.g. PSRAM on SiFli),
 * set EOS_CACHE_USE_DEDICATED_MEM=1 in eos_config.h and override
 * the weak eos_cache_buf_alloc / eos_cache_buf_free in the port layer.
 */
#ifndef EOS_SERVICE_CACHE_H
#define EOS_SERVICE_CACHE_H

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
 * @brief Initialise LVGL image decode cache with sizes from
 *        eos_config.h (EOS_CACHE_SIZE / EOS_CACHE_HEADER_COUNT).
 *
 * Safe to call multiple times — no-op after the first successful
 * init or if LVGL cache was already configured by the user.
 */
void eos_service_cache_init(void);

/**
 * @brief Allocate a cache pixel buffer from dedicated memory pool.
 *
 * Weak default falls back to lv_malloc.
 * Override in a platform port (e.g. SiFli PSRAM) to allocate from
 * a specialised memory heap.
 *
 * @param size  Allocation size in bytes.
 * @return      Pointer to allocated memory, or NULL.
 */
void *eos_cache_buf_alloc(size_t size);

/**
 * @brief Free a cache pixel buffer previously allocated by
 *        eos_cache_buf_alloc.
 * @param ptr  Pointer to free.
 */
void eos_cache_buf_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SERVICE_CACHE_H */
