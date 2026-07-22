/**
 * @file eos_mem.c
 * @brief Memory allocation
 */

#include "eos_mem.h"

/* Includes ---------------------------------------------------*/
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EOS_LOG_TAG "Memory"
#include "eos_log.h"
#include "eos_config.h"
#include "eos_port.h"
#include "eos_mem_port.h"
/* Macros and Definitions -------------------------------------*/
#if EOS_MEM_TRACK_ENABLE
#ifndef EOS_MEM_TRACK_MAX
#define EOS_MEM_TRACK_MAX EOS_MEM_TRACK_TABLE_SIZE
#endif
typedef struct
{
    void *ptr;
    size_t size;
} eos_mem_track_t;
#endif /* EOS_MEM_TRACK_ENABLE */
/* Variables --------------------------------------------------*/
#if EOS_MEM_TRACK_ENABLE
static lv_mem_monitor_t mon;
static eos_mem_track_t _mem_track[EOS_MEM_TRACK_MAX];
#endif /* EOS_MEM_TRACK_ENABLE */
/* Function Implementations -----------------------------------*/

#if EOS_MEM_TRACK_ENABLE

/* Memory Tracking Functions ----------------------------------*/

static size_t eos_mem_track_find(void *ptr)
{
    if (!ptr)
        return 0;
    for (int i = 0; i < EOS_MEM_TRACK_MAX; i++)
    {
        if (_mem_track[i].ptr == ptr)
            return _mem_track[i].size;
    }
    return 0;
}

static void eos_mem_track_add(void *ptr, size_t size)
{
    for (int i = 0; i < EOS_MEM_TRACK_MAX; i++)
    {
        if (_mem_track[i].ptr == NULL)
        {
            _mem_track[i].ptr = ptr;
            _mem_track[i].size = size; // Update LVGL memory monitor
            mon.total_size += size;
            mon.used_cnt++;
            mon.max_used =
                mon.total_size - mon.free_size > mon.max_used ? mon.total_size - mon.free_size : mon.max_used;
            return;
        }
    }
    EOS_LOG_W("Memory track table full");
}
static size_t eos_mem_track_remove(void *ptr)
{
    if (!ptr)
        return 0;
    for (int i = 0; i < EOS_MEM_TRACK_MAX; i++)
    {
        if (_mem_track[i].ptr == ptr)
        {
            size_t sz = _mem_track[i].size;
            _mem_track[i].ptr = NULL;
            _mem_track[i].size = 0;
            mon.free_cnt++;
            mon.free_size += sz;
            mon.used_cnt--;
            return sz;
        }
    }
    return 0;
}
#else
// Find pointer size
static size_t eos_mem_track_find(void *ptr)
{
    LV_UNUSED(ptr);
    return 0;
}

// Add tracking record
static void eos_mem_track_add(void *ptr, size_t size)
{
    LV_UNUSED(ptr);
    LV_UNUSED(size);
    return;
}

// Remove tracking record
static size_t eos_mem_track_remove(void *ptr)
{
    LV_UNUSED(ptr);
    return 0;
}
#endif /* EOS_MEM_TRACK_ENABLE */

/* ElneaOS ----------------------------------------------------*/

void *eos_malloc(size_t size)
{
    void *p = eos_malloc_core(size);
    if (!p)
        return NULL;

    eos_mem_track_add(p, size);
    return p;
}

char *eos_strdup(const char *s)
{
    if (!s)
        return NULL;

    size_t len = strlen(s) + 1;
    char *copy = eos_malloc(len);
    if (copy)
    {
        memcpy(copy, s, len);
    }

    return copy;
}

void eos_free(void *ptr)
{
    if (!ptr)
        return;

    eos_mem_track_remove(ptr);
    eos_free_core(ptr);
}

void *eos_realloc(void *ptr, size_t new_size)
{
    size_t old_size = eos_mem_track_find(ptr);

    void *np = eos_realloc_core(ptr, new_size);
    if (!np)
        return NULL;

    eos_mem_track_remove(ptr);
    eos_mem_track_add(np, new_size);

    return np;
}

void *eos_malloc_zeroed(size_t size)
{
    void *p = eos_malloc_zeroed_core(size);
    if (!p)
        return NULL;

    eos_mem_track_add(p, size);
    return p;
}

/* LVGL -------------------------------------------------------*/

#if EOS_OVERRIDE_LVGL_STDLIB_MALLOC_ENABLE

void lv_mem_init(void)
{
#if EOS_MEM_TRACK_ENABLE
    memset(_mem_track, 0, sizeof(_mem_track));
    memset(&mon, 0, sizeof(mon));
#endif /* EOS_MEM_TRACK_ENABLE */
}

void lv_mem_deinit(void)
{
    /* Not supported */
}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
    EOS_LOG_W("Not supported");
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    EOS_LOG_W("Not supported");
    LV_UNUSED(pool);
    return;
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

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
#if EOS_MEM_TRACK_ENABLE
    if (!mon_p)
        return;
    *mon_p = mon;
#else
    LV_UNUSED(mon_p);
#endif /* EOS_MEM_TRACK_ENABLE */
    return;
}

lv_result_t lv_mem_test_core(void)
{
    /* Not supported */
    return LV_RESULT_OK;
}

#endif /* EOS_OVERRIDE_LVGL_STDLIB_MALLOC_ENABLE */
