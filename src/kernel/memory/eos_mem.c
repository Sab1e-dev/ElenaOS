/**
 * @file eos_mem.c
 * @brief ElenixOS memory allocation, tracking, and region reporting
 */

#include "eos_mem.h"

/* Includes ---------------------------------------------------*/
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_config.h"
#include "eos_log.h"
#include "eos_mem_port.h"
#include "eos_port.h"
#include "cJSON.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "Memory"
#define EOS_MEM_HEADER_MAGIC 0x454F534DU
#define EOS_MEM_HEADER_FREED 0x46524545U
#define EOS_MEM_HEADER_VERSION 1U
#define EOS_MEM_RECENT_FREE_COUNT 8U

#if defined(__GNUC__) || defined(__clang__)
#define EOS_MEM_CALLER_PC ((uintptr_t)__builtin_return_address(0))
#else
#define EOS_MEM_CALLER_PC ((uintptr_t)0U)
#endif

/* Types ------------------------------------------------------*/
typedef struct eos_mem_header eos_mem_header_t;

/* The max_align_t member keeps the user pointer correctly aligned on every
 * platform supported by the C implementation. The header is returned to the
 * provider as one allocation, so this does not require a metadata heap. */
struct eos_mem_header
{
    max_align_t alignment;
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    size_t requested_size;
    size_t reserved_size;
    uint32_t region_id;
    uint32_t reserved;
    uint64_t sequence;
    uintptr_t caller_pc;
    eos_mem_header_t *prev;
    eos_mem_header_t *next;
};

typedef struct
{
    bool used;
    const char *name;
    eos_mem_region_type_t type;
    uint32_t flags;
    uint64_t capacity_bytes;
    eos_mem_region_query_cb_t query;
    void *user_data;
    uint64_t tracked_requested_bytes;
    uint64_t tracked_reserved_bytes;
    uint64_t external_reserved_bytes;
    uint64_t peak_used_bytes;
    uint64_t allocation_count;
    uint8_t alert_level;
    bool report_valid;
    eos_mem_region_stats_t reported;
} eos_mem_region_entry_t;

/* Variables --------------------------------------------------*/
static eos_mem_stats_t s_mem_stats;
static eos_mem_region_entry_t s_regions[EOS_MEM_MAX_REGIONS];
static eos_mem_header_t *s_active_head;
static uint64_t s_sequence;
static uint32_t s_default_region_id = EOS_MEM_REGION_INVALID;
static void *s_recently_freed[EOS_MEM_RECENT_FREE_COUNT];
static uint32_t s_recently_freed_next;
static bool s_initialized;
static bool s_platform_init_called;
static bool s_cjson_initialized;

/* Forward declarations ---------------------------------------*/
static void eos_mem_ensure_initialized(void);
static uint32_t eos_mem_ensure_default_region(void);
static eos_mem_region_entry_t *eos_mem_region_find_locked(uint32_t region_id);
static void eos_mem_record_allocation_failure(size_t size,
                                              eos_mem_alloc_failure_kind_t kind,
                                              uintptr_t caller_pc,
                                              uint32_t region_id);

/* Weak platform hook -----------------------------------------*/
EOS_WEAK void eos_mem_platform_init(void)
{
}

static void *eos_mem_cjson_malloc(size_t size)
{
    return eos_malloc(size);
}

static void eos_mem_cjson_free(void *ptr)
{
    eos_free(ptr);
}

static void eos_mem_init_cjson(void)
{
    if (!s_cjson_initialized)
    {
        static cJSON_Hooks hooks = {
            .malloc_fn = eos_mem_cjson_malloc,
            .free_fn = eos_mem_cjson_free,
        };
        cJSON_InitHooks(&hooks);
        s_cjson_initialized = true;
    }
}

/* Region helpers ---------------------------------------------*/
static eos_mem_region_entry_t *eos_mem_region_find_locked(uint32_t region_id)
{
    if (region_id >= EOS_MEM_MAX_REGIONS || !s_regions[region_id].used)
        return NULL;
    return &s_regions[region_id];
}

static void eos_mem_log_region_alert(uint32_t region_id, uint8_t level)
{
    const char *name = "unknown";

    if (region_id < EOS_MEM_MAX_REGIONS && s_regions[region_id].used && s_regions[region_id].name)
        name = s_regions[region_id].name;

    if (level >= 3U)
        EOS_LOG_E("Memory region exhausted: id=%lu name=%s", (unsigned long)region_id, name);
    else if (level == 2U)
        EOS_LOG_W("Memory region critical: id=%lu name=%s", (unsigned long)region_id, name);
    else
        EOS_LOG_W("Memory region high usage: id=%lu name=%s", (unsigned long)region_id, name);
}

static uint8_t eos_mem_capacity_level(uint64_t used, uint64_t capacity)
{
    uint64_t warning_80;
    uint64_t warning_90;

    if (capacity == 0U)
        return 0U;
    if (used >= capacity)
        return 3U;
    warning_80 = capacity - (capacity / 5U);
    warning_90 = capacity - (capacity / 10U);
    if (used >= warning_90)
        return 2U;
    if (used >= warning_80)
        return 1U;
    return 0U;
}

static uint64_t eos_mem_saturating_add(uint64_t lhs, uint64_t rhs)
{
    return lhs > UINT64_MAX - rhs ? UINT64_MAX : lhs + rhs;
}

static void eos_mem_region_update_alert_locked(uint32_t region_id, bool *alert, uint8_t *level)
{
    eos_mem_region_entry_t *region = eos_mem_region_find_locked(region_id);
    uint64_t used;
    uint8_t new_level;

    if (!region || !alert || !level)
        return;

    used = eos_mem_saturating_add(region->tracked_reserved_bytes, region->external_reserved_bytes);
    new_level = eos_mem_capacity_level(used, region->capacity_bytes);

    if (new_level > region->alert_level)
    {
        region->alert_level = new_level;
        *alert = true;
        *level = new_level;
    }
    else if (new_level == 0U)
    {
        region->alert_level = 0U;
    }
}

static uint32_t eos_mem_ensure_default_region(void)
{
    eos_mem_region_desc_t desc;

    if (s_default_region_id != EOS_MEM_REGION_INVALID)
        return s_default_region_id;

    memset(&desc, 0, sizeof(desc));
    desc.name = "EOS default allocator";
    desc.type = EOS_MEM_REGION_HEAP;
    desc.flags = EOS_MEM_REGION_FLAG_USED_VALID;
    s_default_region_id = eos_mem_region_register(&desc);
    return s_default_region_id;
}

static void eos_mem_ensure_initialized(void)
{
    if (s_initialized)
        return;

    s_initialized = true;
    eos_mem_init_cjson();
    if (!s_platform_init_called)
    {
        s_platform_init_called = true;
        eos_mem_platform_init();
    }
}

uint32_t eos_mem_region_register(const eos_mem_region_desc_t *desc)
{
    eos_critical_ctx_t ctx;
    uint32_t region_id = EOS_MEM_REGION_INVALID;

    if (!desc || !desc->name || desc->name[0] == '\0')
        return EOS_MEM_REGION_INVALID;

    ctx = eos_critical_enter();
    for (uint32_t i = 0U; i < EOS_MEM_MAX_REGIONS; i++)
    {
        if (!s_regions[i].used)
        {
            eos_mem_region_entry_t *region = &s_regions[i];
            memset(region, 0, sizeof(*region));
            region->used = true;
            region->name = desc->name;
            region->type = desc->type;
            region->flags = desc->flags;
            region->capacity_bytes = desc->capacity_bytes;
            region->query = desc->query;
            region->user_data = desc->user_data;
            region_id = i;
            s_mem_stats.region_count++;
            if (s_default_region_id == EOS_MEM_REGION_INVALID && desc->type != EOS_MEM_REGION_EXTERNAL)
                s_default_region_id = region_id;
            break;
        }
    }
    eos_critical_leave(ctx);

    if (region_id == EOS_MEM_REGION_INVALID)
        EOS_LOG_E("Memory region registry full: cannot register %s", desc->name);

    return region_id;
}

eos_result_t eos_mem_region_unregister(uint32_t region_id)
{
    eos_critical_ctx_t ctx;
    eos_mem_region_entry_t *region;

    ctx = eos_critical_enter();
    region = eos_mem_region_find_locked(region_id);
    if (!region)
    {
        eos_critical_leave(ctx);
        return EOS_ERR_NOT_FOUND;
    }

    if (region->tracked_reserved_bytes != 0U || region->external_reserved_bytes != 0U)
    {
        eos_critical_leave(ctx);
        EOS_LOG_E("Cannot unregister busy memory region: id=%lu name=%s", (unsigned long)region_id, region->name);
        return EOS_ERR_BUSY;
    }

    memset(region, 0, sizeof(*region));
    if (s_mem_stats.region_count > 0U)
        s_mem_stats.region_count--;
    if (s_default_region_id == region_id)
        s_default_region_id = EOS_MEM_REGION_INVALID;
    eos_critical_leave(ctx);
    return EOS_OK;
}

eos_result_t eos_mem_region_report(uint32_t region_id, const eos_mem_region_stats_t *stats)
{
    eos_critical_ctx_t ctx;
    eos_mem_region_entry_t *region;

    if (!stats)
        return EOS_ERR_VAR_NULL;

    ctx = eos_critical_enter();
    region = eos_mem_region_find_locked(region_id);
    if (!region)
    {
        eos_critical_leave(ctx);
        return EOS_ERR_NOT_FOUND;
    }
    region->reported = *stats;
    region->report_valid = true;
    eos_critical_leave(ctx);
    return EOS_OK;
}

eos_result_t eos_mem_region_reserve(uint32_t region_id, uint64_t bytes)
{
    eos_critical_ctx_t ctx;
    eos_mem_region_entry_t *region;
    bool alert = false;
    uint8_t alert_level = 0U;

    ctx = eos_critical_enter();
    region = eos_mem_region_find_locked(region_id);
    if (!region)
    {
        eos_critical_leave(ctx);
        return EOS_ERR_NOT_FOUND;
    }
    if (bytes > UINT64_MAX - region->external_reserved_bytes)
    {
        s_mem_stats.invariant_errors++;
        eos_critical_leave(ctx);
        EOS_LOG_E("Memory region reserve overflow: id=%lu bytes=%llu",
                  (unsigned long)region_id,
                  (unsigned long long)bytes);
        return EOS_ERR_INVALID_ARG;
    }
    region->external_reserved_bytes += bytes;
    {
        uint64_t used = eos_mem_saturating_add(region->tracked_reserved_bytes, region->external_reserved_bytes);
        if (used > region->peak_used_bytes)
            region->peak_used_bytes = used;
    }
    eos_mem_region_update_alert_locked(region_id, &alert, &alert_level);
    eos_critical_leave(ctx);

    if (alert)
        eos_mem_log_region_alert(region_id, alert_level);
    return EOS_OK;
}

eos_result_t eos_mem_region_release(uint32_t region_id, uint64_t bytes)
{
    eos_critical_ctx_t ctx;
    eos_mem_region_entry_t *region;

    ctx = eos_critical_enter();
    region = eos_mem_region_find_locked(region_id);
    if (!region)
    {
        eos_critical_leave(ctx);
        return EOS_ERR_NOT_FOUND;
    }
    if (bytes > region->external_reserved_bytes)
    {
        s_mem_stats.invariant_errors++;
        eos_critical_leave(ctx);
        EOS_LOG_E("Memory region release underflow: id=%lu name=%s bytes=%llu",
                  (unsigned long)region_id,
                  region->name,
                  (unsigned long long)bytes);
        return EOS_ERR_INVALID_ARG;
    }
    region->external_reserved_bytes -= bytes;
    if (region->capacity_bytes != 0U
        && eos_mem_capacity_level(
               eos_mem_saturating_add(region->tracked_reserved_bytes, region->external_reserved_bytes),
               region->capacity_bytes)
               == 0U)
        region->alert_level = 0U;
    eos_critical_leave(ctx);
    return EOS_OK;
}

static void eos_mem_region_fill_accounted(const eos_mem_region_entry_t *region, eos_mem_region_stats_t *stats)
{
    uint64_t used = eos_mem_saturating_add(region->tracked_reserved_bytes, region->external_reserved_bytes);

    memset(stats, 0, sizeof(*stats));
    stats->capacity_bytes = region->capacity_bytes;
    stats->used_bytes = used;
    stats->peak_used_bytes = region->peak_used_bytes > used ? region->peak_used_bytes : used;
    stats->allocation_count = region->allocation_count;
    stats->flags = EOS_MEM_REGION_FLAG_USED_VALID;
    if (region->capacity_bytes != 0U)
    {
        stats->flags |= EOS_MEM_REGION_FLAG_CAPACITY_VALID;
        stats->free_bytes = used < region->capacity_bytes ? region->capacity_bytes - used : 0U;
        stats->flags |= EOS_MEM_REGION_FLAG_FREE_VALID;
    }
}

static void eos_mem_region_merge_provider(eos_mem_region_stats_t *dst, const eos_mem_region_stats_t *src)
{
    uint32_t provider_flags = src->flags;

    if (provider_flags & EOS_MEM_REGION_FLAG_CAPACITY_VALID)
    {
        dst->capacity_bytes = src->capacity_bytes;
        dst->flags |= EOS_MEM_REGION_FLAG_CAPACITY_VALID;
    }
    if (provider_flags & EOS_MEM_REGION_FLAG_USED_VALID)
    {
        dst->used_bytes = src->used_bytes;
        dst->flags |= EOS_MEM_REGION_FLAG_USED_VALID;
    }
    if (provider_flags & EOS_MEM_REGION_FLAG_FREE_VALID)
    {
        dst->free_bytes = src->free_bytes;
        dst->flags |= EOS_MEM_REGION_FLAG_FREE_VALID;
    }
    if (provider_flags & EOS_MEM_REGION_FLAG_LARGEST_FREE_VALID)
    {
        dst->largest_free_bytes = src->largest_free_bytes;
        dst->flags |= EOS_MEM_REGION_FLAG_LARGEST_FREE_VALID;
    }
    if (provider_flags & EOS_MEM_REGION_FLAG_EXACT)
        dst->flags |= EOS_MEM_REGION_FLAG_EXACT;
    if (provider_flags & EOS_MEM_REGION_FLAG_HEADROOM_ONLY)
        dst->flags |= EOS_MEM_REGION_FLAG_HEADROOM_ONLY;
    if (src->peak_used_bytes > dst->peak_used_bytes)
        dst->peak_used_bytes = src->peak_used_bytes;
    if (src->allocation_count > dst->allocation_count)
        dst->allocation_count = src->allocation_count;
}

eos_result_t eos_mem_region_get_stats(uint32_t region_id, eos_mem_region_stats_t *stats)
{
    eos_critical_ctx_t ctx;
    eos_mem_region_entry_t copy;
    eos_mem_region_entry_t *region;
    eos_mem_region_stats_t provider_stats;
    bool provider_ok = false;

    if (!stats)
        return EOS_ERR_VAR_NULL;

    memset(&copy, 0, sizeof(copy));
    ctx = eos_critical_enter();
    region = eos_mem_region_find_locked(region_id);
    if (region)
        copy = *region;
    eos_critical_leave(ctx);

    if (!region)
        return EOS_ERR_NOT_FOUND;

    eos_mem_region_fill_accounted(&copy, stats);
    if (copy.report_valid)
        eos_mem_region_merge_provider(stats, &copy.reported);
    if (copy.query)
    {
        memset(&provider_stats, 0, sizeof(provider_stats));
        provider_ok = copy.query(region_id, &provider_stats, copy.user_data);
        if (provider_ok)
            eos_mem_region_merge_provider(stats, &provider_stats);
    }
    return EOS_OK;
}

void eos_mem_get_stats(eos_mem_stats_t *stats)
{
    eos_critical_ctx_t ctx;

    if (!stats)
        return;
    ctx = eos_critical_enter();
    *stats = s_mem_stats;
    eos_critical_leave(ctx);
}

/* Allocation tracking ----------------------------------------*/
static bool eos_mem_header_valid(const eos_mem_header_t *header)
{
    return header && header->magic == EOS_MEM_HEADER_MAGIC && header->version == EOS_MEM_HEADER_VERSION;
}

static eos_mem_header_t *eos_mem_find_active_locked(const void *ptr)
{
    eos_mem_header_t *header = s_active_head;

    while (header)
    {
        if ((const void *)(header + 1) == ptr)
            return header;
        header = header->next;
    }
    return NULL;
}

static bool eos_mem_recent_free_contains_locked(const void *ptr)
{
    for (uint32_t i = 0U; i < EOS_MEM_RECENT_FREE_COUNT; i++)
    {
        if (s_recently_freed[i] == ptr)
            return true;
    }
    return false;
}

static void eos_mem_recent_free_add_locked(void *ptr)
{
    s_recently_freed[s_recently_freed_next] = ptr;
    s_recently_freed_next = (s_recently_freed_next + 1U) % EOS_MEM_RECENT_FREE_COUNT;
}

static void eos_mem_recent_free_remove_locked(const void *ptr)
{
    for (uint32_t i = 0U; i < EOS_MEM_RECENT_FREE_COUNT; i++)
    {
        if (s_recently_freed[i] == ptr)
            s_recently_freed[i] = NULL;
    }
}

static void eos_mem_unlink_locked(eos_mem_header_t *header)
{
    if (header->prev)
        header->prev->next = header->next;
    else
        s_active_head = header->next;
    if (header->next)
        header->next->prev = header->prev;
    header->prev = NULL;
    header->next = NULL;
}

static void eos_mem_link_locked(eos_mem_header_t *header)
{
    header->prev = NULL;
    header->next = s_active_head;
    if (s_active_head)
        s_active_head->prev = header;
    s_active_head = header;
}

static void eos_mem_record_allocation_failure(size_t size,
                                              eos_mem_alloc_failure_kind_t kind,
                                              uintptr_t caller_pc,
                                              uint32_t region_id)
{
    eos_critical_ctx_t ctx = eos_critical_enter();
    s_mem_stats.failed_alloc_count++;
    eos_critical_leave(ctx);

    eos_mem_alloc_failed(size, kind, caller_pc);
    EOS_LOG_E("Allocation failed: region=%lu size=%lu kind=%d pc=0x%lx",
              (unsigned long)region_id,
              (unsigned long)size,
              (int)kind,
              (unsigned long)caller_pc);
    if (region_id != EOS_MEM_REGION_INVALID)
        eos_mem_log_region_alert(region_id, 3U);
}

static void *eos_mem_alloc_internal(size_t size, bool zeroed, eos_mem_alloc_failure_kind_t kind, uintptr_t caller_pc)
{
    void *raw;
    eos_mem_header_t *header;
    uint32_t region_id;
    size_t total;
    eos_critical_ctx_t ctx;
    eos_mem_region_entry_t *region;
    bool alert = false;
    uint8_t alert_level = 0U;

    eos_mem_ensure_initialized();
    region_id = eos_mem_ensure_default_region();
    if (size > SIZE_MAX - sizeof(eos_mem_header_t))
    {
        eos_mem_record_allocation_failure(size, kind, caller_pc, region_id);
        return NULL;
    }
    total = sizeof(eos_mem_header_t) + size;
    raw = zeroed ? eos_malloc_zeroed_core(total) : eos_malloc_core(total);
    if (!raw)
    {
        eos_mem_record_allocation_failure(size, kind, caller_pc, region_id);
        return NULL;
    }

    header = (eos_mem_header_t *)raw;
    header->magic = EOS_MEM_HEADER_MAGIC;
    header->version = EOS_MEM_HEADER_VERSION;
    header->flags = 0U;
    header->requested_size = size;
    header->reserved_size = total;
    header->region_id = region_id;
    header->reserved = 0U;
    header->caller_pc = caller_pc;
    header->prev = NULL;
    header->next = NULL;

    ctx = eos_critical_enter();
    region = eos_mem_region_find_locked(region_id);
    if (size > UINT64_MAX - s_mem_stats.current_requested_bytes
        || total > UINT64_MAX - s_mem_stats.current_reserved_bytes
        || (region
            && (size > UINT64_MAX - region->tracked_requested_bytes
                || total > UINT64_MAX - region->tracked_reserved_bytes)))
    {
        s_mem_stats.invariant_errors++;
        eos_critical_leave(ctx);
        eos_free_core(header);
        EOS_LOG_E("Memory statistics overflow while tracking allocation: size=%lu", (unsigned long)size);
        return NULL;
    }
    header->sequence = ++s_sequence;
    eos_mem_recent_free_remove_locked((void *)(header + 1));
    eos_mem_link_locked(header);
    s_mem_stats.current_requested_bytes += size;
    s_mem_stats.current_reserved_bytes += total;
    s_mem_stats.peak_requested_bytes = s_mem_stats.peak_requested_bytes > s_mem_stats.current_requested_bytes
                                           ? s_mem_stats.peak_requested_bytes
                                           : s_mem_stats.current_requested_bytes;
    s_mem_stats.peak_reserved_bytes = s_mem_stats.peak_reserved_bytes > s_mem_stats.current_reserved_bytes
                                          ? s_mem_stats.peak_reserved_bytes
                                          : s_mem_stats.current_reserved_bytes;
    s_mem_stats.current_block_count++;
    s_mem_stats.total_alloc_count++;
    if (region)
    {
        region->tracked_requested_bytes += size;
        region->tracked_reserved_bytes += total;
        region->allocation_count++;
        {
            uint64_t used = eos_mem_saturating_add(region->tracked_reserved_bytes, region->external_reserved_bytes);
            if (used > region->peak_used_bytes)
                region->peak_used_bytes = used;
        }
        eos_mem_region_update_alert_locked(region_id, &alert, &alert_level);
    }
    else
    {
        s_mem_stats.untracked_count++;
    }
    eos_critical_leave(ctx);

    if (alert)
        eos_mem_log_region_alert(region_id, alert_level);
    return (void *)(header + 1);
}

static bool eos_mem_release_internal(void *ptr, bool log_invalid)
{
    eos_mem_header_t *header;
    eos_mem_region_entry_t *region;
    eos_critical_ctx_t ctx;
    size_t requested_size;
    size_t reserved_size;
    uint32_t region_id;

    if (!ptr)
        return true;

    ctx = eos_critical_enter();
    header = eos_mem_find_active_locked(ptr);
    if (!eos_mem_header_valid(header))
    {
        s_mem_stats.invalid_free_count++;
        if (eos_mem_recent_free_contains_locked(ptr))
            s_mem_stats.double_free_count++;
        eos_critical_leave(ctx);
        if (log_invalid)
            EOS_LOG_E("Invalid or duplicate eos_free: ptr=%p", ptr);
        return false;
    }

    requested_size = header->requested_size;
    reserved_size = header->reserved_size;
    region_id = header->region_id;
    eos_mem_unlink_locked(header);
    header->magic = EOS_MEM_HEADER_FREED;
    eos_mem_recent_free_add_locked(ptr);

    if (s_mem_stats.current_requested_bytes < requested_size || s_mem_stats.current_reserved_bytes < reserved_size
        || s_mem_stats.current_block_count == 0U)
    {
        s_mem_stats.invariant_errors++;
        s_mem_stats.current_requested_bytes = 0U;
        s_mem_stats.current_reserved_bytes = 0U;
        s_mem_stats.current_block_count = 0U;
    }
    else
    {
        s_mem_stats.current_requested_bytes -= requested_size;
        s_mem_stats.current_reserved_bytes -= reserved_size;
        s_mem_stats.current_block_count--;
    }
    s_mem_stats.total_free_count++;

    region = eos_mem_region_find_locked(region_id);
    if (region)
    {
        if (region->tracked_requested_bytes < requested_size || region->tracked_reserved_bytes < reserved_size)
        {
            s_mem_stats.invariant_errors++;
            region->tracked_requested_bytes = 0U;
            region->tracked_reserved_bytes = 0U;
        }
        else
        {
            region->tracked_requested_bytes -= requested_size;
            region->tracked_reserved_bytes -= reserved_size;
        }
        if (region->capacity_bytes != 0U
            && eos_mem_capacity_level(
                   eos_mem_saturating_add(region->tracked_reserved_bytes, region->external_reserved_bytes),
                   region->capacity_bytes)
                   == 0U)
            region->alert_level = 0U;
    }
    eos_critical_leave(ctx);

    eos_free_core(header);
    return true;
}

/* ElenixOS allocation API ------------------------------------*/
void *eos_malloc(size_t size)
{
    return eos_mem_alloc_internal(size, false, EOS_MEM_ALLOC_FAILURE_MALLOC, EOS_MEM_CALLER_PC);
}

char *eos_strdup(const char *s)
{
    size_t len;
    char *copy;

    if (!s)
        return NULL;
    len = strlen(s);
    if (len == SIZE_MAX)
    {
        eos_mem_record_allocation_failure(len,
                                          EOS_MEM_ALLOC_FAILURE_MALLOC,
                                          EOS_MEM_CALLER_PC,
                                          eos_mem_ensure_default_region());
        return NULL;
    }
    copy = eos_malloc(len + 1U);
    if (copy)
        memcpy(copy, s, len + 1U);
    return copy;
}

void eos_free(void *ptr)
{
    (void)eos_mem_release_internal(ptr, true);
}

void *eos_realloc(void *ptr, size_t new_size)
{
    eos_mem_header_t *old_header;
    eos_mem_header_t *new_header;
    eos_mem_region_entry_t *region;
    eos_critical_ctx_t ctx;
    size_t old_requested;
    size_t old_reserved;
    size_t new_reserved;
    uint32_t region_id;
    uintptr_t caller_pc = EOS_MEM_CALLER_PC;
    bool alert = false;
    uint8_t alert_level = 0U;

    if (!ptr)
        return eos_mem_alloc_internal(new_size, false, EOS_MEM_ALLOC_FAILURE_REALLOC, caller_pc);
    if (new_size == 0U)
    {
        eos_free(ptr);
        return NULL;
    }
    if (new_size > SIZE_MAX - sizeof(eos_mem_header_t))
    {
        eos_mem_record_allocation_failure(new_size, EOS_MEM_ALLOC_FAILURE_REALLOC, caller_pc, EOS_MEM_REGION_INVALID);
        return NULL;
    }

    ctx = eos_critical_enter();
    old_header = eos_mem_find_active_locked(ptr);
    if (!eos_mem_header_valid(old_header))
    {
        s_mem_stats.invalid_free_count++;
        eos_critical_leave(ctx);
        EOS_LOG_E("Invalid eos_realloc pointer: ptr=%p", ptr);
        return NULL;
    }
    old_requested = old_header->requested_size;
    old_reserved = old_header->reserved_size;
    region_id = old_header->region_id;
    eos_critical_leave(ctx);

    new_reserved = sizeof(eos_mem_header_t) + new_size;
    new_header = (eos_mem_header_t *)eos_realloc_core(old_header, new_reserved);
    if (!new_header)
    {
        eos_mem_record_allocation_failure(new_size, EOS_MEM_ALLOC_FAILURE_REALLOC, caller_pc, region_id);
        return NULL;
    }

    ctx = eos_critical_enter();
    if (new_header != old_header)
    {
        if (new_header->prev)
            new_header->prev->next = new_header;
        else
            s_active_head = new_header;
        if (new_header->next)
            new_header->next->prev = new_header;
    }
    new_header->magic = EOS_MEM_HEADER_MAGIC;
    new_header->version = EOS_MEM_HEADER_VERSION;
    new_header->requested_size = new_size;
    new_header->reserved_size = new_reserved;
    new_header->caller_pc = caller_pc;

    if (new_size >= old_requested)
        s_mem_stats.current_requested_bytes += new_size - old_requested;
    else
        s_mem_stats.current_requested_bytes -= old_requested - new_size;
    if (new_reserved >= old_reserved)
        s_mem_stats.current_reserved_bytes += new_reserved - old_reserved;
    else
        s_mem_stats.current_reserved_bytes -= old_reserved - new_reserved;
    s_mem_stats.peak_requested_bytes = s_mem_stats.peak_requested_bytes > s_mem_stats.current_requested_bytes
                                           ? s_mem_stats.peak_requested_bytes
                                           : s_mem_stats.current_requested_bytes;
    s_mem_stats.peak_reserved_bytes = s_mem_stats.peak_reserved_bytes > s_mem_stats.current_reserved_bytes
                                          ? s_mem_stats.peak_reserved_bytes
                                          : s_mem_stats.current_reserved_bytes;

    region = eos_mem_region_find_locked(region_id);
    if (region)
    {
        if (new_size >= old_requested)
            region->tracked_requested_bytes += new_size - old_requested;
        else
            region->tracked_requested_bytes -= old_requested - new_size;
        if (new_reserved >= old_reserved)
            region->tracked_reserved_bytes += new_reserved - old_reserved;
        else
            region->tracked_reserved_bytes -= old_reserved - new_reserved;
        {
            uint64_t used = eos_mem_saturating_add(region->tracked_reserved_bytes, region->external_reserved_bytes);
            if (used > region->peak_used_bytes)
                region->peak_used_bytes = used;
        }
        eos_mem_region_update_alert_locked(region_id, &alert, &alert_level);
    }
    eos_critical_leave(ctx);

    if (alert)
        eos_mem_log_region_alert(region_id, alert_level);
    return (void *)(new_header + 1);
}

void *eos_malloc_zeroed(size_t size)
{
    return eos_mem_alloc_internal(size, true, EOS_MEM_ALLOC_FAILURE_ZEROED, EOS_MEM_CALLER_PC);
}

/* Legacy queries ---------------------------------------------*/
size_t eos_mem_get_used_bytes(void)
{
    eos_mem_stats_t stats;
    eos_mem_get_stats(&stats);
    return stats.current_requested_bytes > SIZE_MAX ? SIZE_MAX : (size_t)stats.current_requested_bytes;
}

size_t eos_mem_get_free_bytes(void)
{
    /* The old implementation added cumulative freed bytes to platform
     * headroom. Keep this API as a platform estimate only. */
    return eos_port_get_free_mem();
}

/* LVGL adapter -----------------------------------------------*/
void lv_mem_init(void)
{
    eos_mem_ensure_initialized();
}

void lv_mem_deinit(void)
{
}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    EOS_LOG_W("LVGL memory pools are not supported by the EOS allocator");
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    LV_UNUSED(pool);
    EOS_LOG_W("LVGL memory pools are not supported by the EOS allocator");
}

void *lv_malloc_core(size_t size)
{
    return eos_malloc(size);
}

void *lv_realloc_core(void *p, size_t new_size)
{
    return eos_realloc(p, new_size);
}

void lv_free_core(void *p)
{
    eos_free(p);
}

static size_t eos_mem_to_size_t(uint64_t value)
{
    return value > (uint64_t)SIZE_MAX ? SIZE_MAX : (size_t)value;
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    eos_mem_stats_t global_stats;
    eos_mem_region_stats_t region_stats;
    uint32_t region_id;
    uint64_t total;
    uint64_t free_bytes;
    uint64_t used;

    if (!mon_p)
        return;

    memset(mon_p, 0, sizeof(*mon_p));
    eos_mem_get_stats(&global_stats);
    region_id = eos_mem_ensure_default_region();

    if (eos_mem_region_get_stats(region_id, &region_stats) != EOS_OK)
    {
        memset(&region_stats, 0, sizeof(region_stats));
        region_stats.used_bytes = global_stats.current_reserved_bytes;
    }

    used = region_stats.used_bytes;
    free_bytes = (region_stats.flags & EOS_MEM_REGION_FLAG_FREE_VALID) ? region_stats.free_bytes : 0U;
    if (region_stats.flags & EOS_MEM_REGION_FLAG_CAPACITY_VALID)
        total = region_stats.capacity_bytes;
    else
        total = used + free_bytes;

    if (total < used)
        total = used;
    mon_p->total_size = eos_mem_to_size_t(total);
    mon_p->free_size = eos_mem_to_size_t(free_bytes);
    mon_p->free_biggest_size = eos_mem_to_size_t(region_stats.largest_free_bytes);
    mon_p->used_cnt =
        global_stats.current_block_count > UINT32_MAX ? UINT32_MAX : (uint32_t)global_stats.current_block_count;
    mon_p->max_used = eos_mem_to_size_t(global_stats.peak_reserved_bytes);

    if (total != 0U)
    {
        uint64_t used_pct = (used * 100U) / total;
        mon_p->used_pct = used_pct > UINT8_MAX ? UINT8_MAX : (uint8_t)used_pct;
    }
    if ((region_stats.flags & EOS_MEM_REGION_FLAG_FREE_VALID) && free_bytes != 0U
        && (region_stats.flags & EOS_MEM_REGION_FLAG_LARGEST_FREE_VALID))
    {
        uint64_t frag_pct = ((free_bytes - region_stats.largest_free_bytes) * 100U) / free_bytes;
        mon_p->frag_pct = frag_pct > UINT8_MAX ? UINT8_MAX : (uint8_t)frag_pct;
    }
}
