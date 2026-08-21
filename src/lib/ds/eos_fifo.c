/**
 * @file eos_fifo.c
 * @brief Ring buffer FIFO
 */

#include "eos_fifo.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#define EOS_LOG_DISABLE
#define EOS_LOG_TAG "FIFO"
#include "eos_log.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/

struct eos_fifo_t
{
    uint8_t *buffer;
    uint16_t capacity;
    uint16_t head;
    uint16_t tail;
    uint16_t count;

    eos_fifo_stats_t stats;
};

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

eos_fifo_t *eos_fifo_create(uint16_t capacity)
{
    if (capacity == 0)
        capacity = 16;

    eos_fifo_t *fifo = eos_malloc_zeroed(sizeof(eos_fifo_t));
    if (!fifo)
        return NULL;

    fifo->buffer = eos_malloc_zeroed(capacity);
    if (!fifo->buffer)
    {
        eos_free(fifo);
        return NULL;
    }

    fifo->capacity = capacity;
    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;

    fifo->stats.write_count = 0;
    fifo->stats.read_count = 0;
    fifo->stats.overflow_count = 0;
    fifo->stats.peak_usage = 0;

    EOS_LOG_I("fifo created, capacity: %d", capacity);
    return fifo;
}

void eos_fifo_destroy(eos_fifo_t *fifo)
{
    if (!fifo)
        return;

    if (fifo->buffer)
        eos_free(fifo->buffer);
    eos_free(fifo);

    EOS_LOG_I("fifo destroyed");
}

void eos_fifo_reset(eos_fifo_t *fifo)
{
    if (!fifo)
        return;

    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;
    fifo->stats.write_count = 0;
    fifo->stats.read_count = 0;
    fifo->stats.overflow_count = 0;
    fifo->stats.peak_usage = 0;
}

uint16_t eos_fifo_write(eos_fifo_t *fifo, const void *data, uint16_t size)
{
    if (!fifo || !data || size == 0)
        return 0;

    uint16_t written = 0;
    const uint8_t *src = (const uint8_t *)data;

    while (written < size)
    {
        if (fifo->count >= fifo->capacity)
        {
            fifo->stats.overflow_count++;
            break;
        }

        fifo->buffer[fifo->head] = src[written];
        fifo->head = (fifo->head + 1) % fifo->capacity;
        fifo->count++;
        written++;
    }

    fifo->stats.write_count += written;
    if (fifo->count > fifo->stats.peak_usage)
        fifo->stats.peak_usage = fifo->count;

    if (written > 0)
        EOS_LOG_I("fifo write %d bytes", written);

    return written;
}

void eos_fifo_drop(eos_fifo_t *fifo, uint16_t count)
{
    if (!fifo || count == 0)
        return;

    if (count > fifo->count)
        count = fifo->count;

    fifo->tail = (fifo->tail + count) % fifo->capacity;
    fifo->count -= count;
    fifo->stats.overflow_count++;

    EOS_LOG_I("fifo drop %d bytes (overflow)", count);
}

uint16_t eos_fifo_write_atomic(eos_fifo_t *fifo, const void *data, uint16_t size)
{
    if (!fifo || !data || size == 0)
        return 0;

    /* Entry larger than total capacity — can never fit */
    if (size > fifo->capacity)
    {
        EOS_LOG_E("fifo atomic write failed: size %d > capacity %d", size, fifo->capacity);
        return 0;
    }

    /* Make room by dropping oldest entries if needed */
    uint16_t free = fifo->capacity - fifo->count;
    if (free < size)
    {
        uint16_t to_drop = size - free;
        fifo->tail = (fifo->tail + to_drop) % fifo->capacity;
        fifo->count -= to_drop;
        fifo->stats.overflow_count++;
        EOS_LOG_I("fifo atomic write: dropped %d bytes to make room", to_drop);
    }

    /* Now guaranteed space — write atomically */
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < size; i++)
    {
        fifo->buffer[fifo->head] = src[i];
        fifo->head = (fifo->head + 1) % fifo->capacity;
    }
    fifo->count += size;
    fifo->stats.write_count += size;
    if (fifo->count > fifo->stats.peak_usage)
        fifo->stats.peak_usage = fifo->count;

    EOS_LOG_I("fifo atomic write %d bytes (count=%d/%d)", size, fifo->count, fifo->capacity);
    return size;
}

uint16_t eos_fifo_read(eos_fifo_t *fifo, void *buf, uint16_t size)
{
    if (!fifo || !buf || size == 0)
        return 0;

    uint16_t read_bytes = 0;
    uint8_t *dst = (uint8_t *)buf;

    while (read_bytes < size && fifo->count > 0)
    {
        dst[read_bytes] = fifo->buffer[fifo->tail];
        fifo->tail = (fifo->tail + 1) % fifo->capacity;
        fifo->count--;
        read_bytes++;
    }

    fifo->stats.read_count += read_bytes;

    if (read_bytes > 0)
        EOS_LOG_I("fifo read %d bytes", read_bytes);

    return read_bytes;
}

uint16_t eos_fifo_peek(eos_fifo_t *fifo, void *buf, uint16_t size)
{
    if (!fifo || !buf || size == 0)
        return 0;

    uint16_t peek_bytes = 0;
    uint8_t *dst = (uint8_t *)buf;
    uint16_t idx = fifo->tail;

    while (peek_bytes < size && peek_bytes < fifo->count)
    {
        dst[peek_bytes] = fifo->buffer[idx];
        idx = (idx + 1) % fifo->capacity;
        peek_bytes++;
    }

    return peek_bytes;
}

uint16_t eos_fifo_get_count(eos_fifo_t *fifo)
{
    if (!fifo)
        return 0;
    return fifo->count;
}

uint16_t eos_fifo_get_free(eos_fifo_t *fifo)
{
    if (!fifo)
        return 0;
    return fifo->capacity - fifo->count;
}

uint16_t eos_fifo_get_capacity(eos_fifo_t *fifo)
{
    if (!fifo)
        return 0;
    return fifo->capacity;
}

bool eos_fifo_is_empty(eos_fifo_t *fifo)
{
    if (!fifo)
        return true;
    return fifo->count == 0;
}

bool eos_fifo_is_full(eos_fifo_t *fifo)
{
    if (!fifo)
        return true;
    return fifo->count >= fifo->capacity;
}

void eos_fifo_get_stats(eos_fifo_t *fifo, eos_fifo_stats_t *stats)
{
    if (!fifo || !stats)
        return;
    *stats = fifo->stats;
}
