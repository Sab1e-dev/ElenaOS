/**
 * @file eos_mem_auto.c
 * @brief Auto memory allocation
 */

#include "eos_config.h"

#if EOS_MEM_ALLOC_PROVIDER == EOS_MEM_PROVIDER_AUTO

#include "eos_mem_port.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_port.h"
#define EOS_LOG_TAG "MemAuto"
#include "eos_log.h"
#include "eos_config.h"
#include EOS_MEM_PROVIDER_AUTO_INCLUDE
/* Macros and Definitions -------------------------------------*/

typedef enum
{
    EOS_MEM_POOL_FAST = 0,
    EOS_MEM_POOL_LARGE = 1
} eos_mem_type_t;

typedef struct
{
    uint8_t type; /**< Memory type */
    uint8_t pad[sizeof(void *) - sizeof(uint8_t)]; /**< Memory alignment */
} eos_mem_header_t;

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

void *eos_malloc_core(size_t size)
{
    size_t total;
    eos_mem_header_t *hdr;

    if (size > SIZE_MAX - sizeof(eos_mem_header_t))
        return NULL;
    total = size + sizeof(eos_mem_header_t);

    if (size < EOS_MEM_POOL_ALLOC_THRESHOLD)
        hdr = EOS_MEM_ALLOC_FAST(total);
    else
        hdr = EOS_MEM_ALLOC_LARGE(total);

    if (!hdr)
        return NULL;

    hdr->type = (size < EOS_MEM_POOL_ALLOC_THRESHOLD) ? EOS_MEM_POOL_FAST : EOS_MEM_POOL_LARGE;

    EOS_LOG_D("Auto memory alloc: [%s]%d",
              hdr->type == EOS_MEM_POOL_FAST ? "EOS_MEM_POOL_FAST" : "EOS_MEM_POOL_LARGE",
              total);

    return (void *)(hdr + 1);
}

void *eos_malloc_zeroed_core(size_t size)
{
    size_t total;
    eos_mem_header_t *hdr;

    if (size > SIZE_MAX - sizeof(eos_mem_header_t))
        return NULL;
    total = size + sizeof(eos_mem_header_t);

    if (size < EOS_MEM_POOL_ALLOC_THRESHOLD)
        hdr = EOS_MEM_CALLOC_FAST(1, total);
    else
        hdr = EOS_MEM_CALLOC_LARGE(1, total);

    if (!hdr)
        return NULL;

    hdr->type = (size < EOS_MEM_POOL_ALLOC_THRESHOLD) ? EOS_MEM_POOL_FAST : EOS_MEM_POOL_LARGE;

    return (void *)(hdr + 1);
}

void eos_free_core(void *ptr)
{
    EOS_CHECK_PTR_RETURN(ptr);

    eos_mem_header_t *hdr = (eos_mem_header_t *)ptr - 1;

    switch (hdr->type)
    {
        case EOS_MEM_POOL_FAST:
            EOS_MEM_FREE_FAST(hdr);
            break;
        case EOS_MEM_POOL_LARGE:
            EOS_MEM_FREE_LARGE(hdr);
            break;
        default:
            EOS_LOG_E("Unknown memory type");
            break;
    }
}

void *eos_realloc_core(void *ptr, size_t new_size)
{
    EOS_CHECK_PTR_RETURN_VAL(ptr, eos_malloc_core(new_size));

    // Get original memory block info
    eos_mem_header_t *old_hdr = (eos_mem_header_t *)ptr - 1;
    eos_mem_type_t original_type = old_hdr->type; // Keep original memory type

    EOS_LOG_D("Realloc:  %zu, keep pool: %s",
              new_size,
              original_type == EOS_MEM_POOL_FAST ? "EOS_MEM_POOL_FAST" : "EOS_MEM_POOL_LARGE");

    // Always realloc in the original memory pool
    size_t total;
    eos_mem_header_t *new_hdr;

    if (new_size > SIZE_MAX - sizeof(eos_mem_header_t))
        return NULL;
    total = new_size + sizeof(eos_mem_header_t);

    if (original_type == EOS_MEM_POOL_FAST)
    {
        new_hdr = EOS_MEM_REALLOC_FAST(old_hdr, total);
    }
    else
    {
        new_hdr = EOS_MEM_REALLOC_LARGE(old_hdr, total);
    }

    if (!new_hdr)
    {
        EOS_LOG_E("Realloc failed for pool: %s",
                  original_type == EOS_MEM_POOL_FAST ? "EOS_MEM_POOL_FAST" : "EOS_MEM_POOL_LARGE");
        return NULL;
    }

    // Keep the original memory type
    new_hdr->type = original_type;

    EOS_LOG_D("Realloc success in original pool");
    return (void *)(new_hdr + 1);
}

#endif /* EOS_MEM_ALLOC_PROVIDER */
