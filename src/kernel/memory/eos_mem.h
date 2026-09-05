/**
 * @file eos_mem.h
 * @brief ElenixOS memory allocation, tracking, and region reporting
 */

#ifndef EOS_MEM_H
#define EOS_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_error.h"

/* Configuration defaults -------------------------------------*/
#ifndef EOS_MEM_MAX_REGIONS
#define EOS_MEM_MAX_REGIONS 16U
#endif

#define EOS_MEM_REGION_INVALID UINT32_MAX

/* Public typedefs --------------------------------------------*/

/**
 * @brief Memory allocation operation that failed
 */
typedef enum
{
    EOS_MEM_ALLOC_FAILURE_MALLOC = 1,
    EOS_MEM_ALLOC_FAILURE_ZEROED,
    EOS_MEM_ALLOC_FAILURE_REALLOC
} eos_mem_alloc_failure_kind_t;

/**
 * @brief Memory region category
 */
typedef enum
{
    EOS_MEM_REGION_HEAP = 0,
    EOS_MEM_REGION_POOL,
    EOS_MEM_REGION_PSRAM,
    EOS_MEM_REGION_DMA,
    EOS_MEM_REGION_FRAMEBUFFER,
    EOS_MEM_REGION_EXTERNAL
} eos_mem_region_type_t;

/**
 * @brief Capabilities and semantics of a region snapshot
 */
typedef enum
{
    EOS_MEM_REGION_FLAG_NONE = 0U,
    EOS_MEM_REGION_FLAG_CAPACITY_VALID = 1U << 0,
    EOS_MEM_REGION_FLAG_USED_VALID = 1U << 1,
    EOS_MEM_REGION_FLAG_FREE_VALID = 1U << 2,
    EOS_MEM_REGION_FLAG_LARGEST_FREE_VALID = 1U << 3,
    EOS_MEM_REGION_FLAG_EXACT = 1U << 4,
    EOS_MEM_REGION_FLAG_HEADROOM_ONLY = 1U << 5,
    EOS_MEM_REGION_FLAG_EXTERNAL = 1U << 6
} eos_mem_region_flags_t;

/**
 * @brief Point-in-time statistics for one memory region
 *
 * A zero capacity means the provider does not expose a total capacity.
 * Values are never inferred as exact unless EOS_MEM_REGION_FLAG_EXACT is set.
 */
typedef struct
{
    uint64_t capacity_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    uint64_t largest_free_bytes;
    uint64_t peak_used_bytes;
    uint64_t allocation_count;
    uint32_t flags;
} eos_mem_region_stats_t;

/**
 * @brief Query callback implemented by a platform/provider
 *
 * The callback must not allocate memory. It should return false when the
 * provider cannot obtain a snapshot at this time.
 */
typedef bool (*eos_mem_region_query_cb_t)(uint32_t region_id, eos_mem_region_stats_t *stats, void *user_data);

/**
 * @brief Region registration descriptor
 */
typedef struct
{
    const char *name;
    eos_mem_region_type_t type;
    uint32_t flags;
    uint64_t capacity_bytes;
    eos_mem_region_query_cb_t query;
    void *user_data;
} eos_mem_region_desc_t;

/**
 * @brief Global EOS-owned allocation statistics
 */
typedef struct
{
    uint64_t current_requested_bytes;
    uint64_t current_reserved_bytes;
    uint64_t peak_requested_bytes;
    uint64_t peak_reserved_bytes;
    uint64_t current_block_count;
    uint64_t total_alloc_count;
    uint64_t total_free_count;
    uint64_t failed_alloc_count;
    uint64_t invalid_free_count;
    uint64_t double_free_count;
    uint64_t untracked_count;
    uint32_t region_count;
    uint32_t invariant_errors;
} eos_mem_stats_t;

/**
 * @brief Register a platform or external memory region
 * @return Region ID, or EOS_MEM_REGION_INVALID when the registry is full
 */
uint32_t eos_mem_region_register(const eos_mem_region_desc_t *desc);

/**
 * @brief Unregister a memory region
 * @note A region with active EOS allocations cannot be unregistered.
 */
eos_result_t eos_mem_region_unregister(uint32_t region_id);

/**
 * @brief Report usage for an externally managed region
 */
eos_result_t eos_mem_region_report(uint32_t region_id, const eos_mem_region_stats_t *stats);

/**
 * @brief Account an external reservation/release in a region
 */
eos_result_t eos_mem_region_reserve(uint32_t region_id, uint64_t bytes);
eos_result_t eos_mem_region_release(uint32_t region_id, uint64_t bytes);

/**
 * @brief Get a consistent global EOS allocation snapshot
 */
void eos_mem_get_stats(eos_mem_stats_t *stats);

/**
 * @brief Get a consistent snapshot for one region
 */
eos_result_t eos_mem_region_get_stats(uint32_t region_id, eos_mem_region_stats_t *stats);

/**
 * @brief Platform hook called when LVGL initializes its allocator
 *
 * A platform can provide a strong implementation and register its regions
 * before normal EOS allocations begin. The hook must not allocate memory.
 */
void eos_mem_platform_init(void);

/* Public function prototypes ---------------------------------*/

/**
 * @brief Memory allocation function
 * @param size Memory size, unit: bytes
 * @return Memory address on success, otherwise NULL
 */
void *eos_malloc(size_t size);

/**
 * @brief Report a failed allocation to the platform diagnostic layer
 * @note Implementations must not allocate memory, lock, or depend on logging.
 */
void eos_mem_alloc_failed(size_t size, eos_mem_alloc_failure_kind_t kind, uintptr_t caller_pc);

/**
 * @brief Create a copy of the given string
 */
char *eos_strdup(const char *s);

/**
 * @brief Allocate a zero-filled block
 */
void *eos_malloc_zeroed(size_t size);

/**
 * @brief Free an EOS-owned block
 */
void eos_free(void *ptr);

/**
 * @brief Reallocate an EOS-owned block
 *
 * On failure, the original block remains valid, following the C realloc
 * contract. realloc(ptr, 0) releases ptr and returns NULL.
 */
void *eos_realloc(void *ptr, size_t new_size);

/**
 * @brief Get current EOS-requested bytes
 */
size_t eos_mem_get_used_bytes(void);

/**
 * @brief Get the platform's legacy free-memory estimate
 *
 * This returns the platform-reported value only. It is not the cumulative
 * amount of memory previously freed.
 */
size_t eos_mem_get_free_bytes(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_MEM_H */
