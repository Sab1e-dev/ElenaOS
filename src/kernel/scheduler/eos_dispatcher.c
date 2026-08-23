/**
 * @file eos_dispatcher.c
 * @brief Task dispatcher
 */

#include "eos_dispatcher.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "eos_port.h"
#include "eos_config.h"
#include "eos_mem.h"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/
typedef struct
{
    eos_dispatcher_cb_t cb;
    void *user_data;
} eos_dispatcher_item_t;
/* Variables --------------------------------------------------*/
static eos_dispatcher_item_t *s_queue = NULL;
static volatile int s_head = 0;
static volatile int s_tail = 0;
static int s_capacity = 0;
#if EOS_COMPILE_MODE == EOS_DEBUG
static bool async_initialized = false;
#endif /* EOS_COMPILE_MODE */
/* Function Implementations -----------------------------------*/

void eos_dispatcher_init(void)
{
#if EOS_COMPILE_MODE == EOS_DEBUG
    if (async_initialized)
        return;
#endif /* EOS_COMPILE_MODE */

    s_capacity = 8;
    s_queue = eos_malloc(sizeof(eos_dispatcher_item_t) * s_capacity);
    if (!s_queue)
    {
        s_capacity = 0;
    }
    s_head = 0;
    s_tail = 0;
#if EOS_COMPILE_MODE == EOS_DEBUG
    async_initialized = true;
#endif /* EOS_COMPILE_MODE */
}

static int _queue_expand(void)
{
    int new_capacity = s_capacity * 2;
    eos_dispatcher_item_t *new_queue = eos_malloc(sizeof(eos_dispatcher_item_t) * new_capacity);
    if (!new_queue)
    {
        EOS_LOG_E("_queue_expand FAILED: new_capacity=%d", new_capacity);
        return -1;
    }

    int i = 0;
    for (int idx = s_head; idx != s_tail; idx = (idx + 1) % s_capacity)
    {
        new_queue[i++] = s_queue[idx];
    }

    eos_free(s_queue);
    s_queue = new_queue;
    s_head = 0;
    s_tail = i;
    s_capacity = new_capacity;
    return 0;
}

void eos_dispatcher_call(eos_dispatcher_cb_t cb, void *user_data)
{
#if EOS_COMPILE_MODE == EOS_DEBUG
    if (!async_initialized)
        return;
#endif /* EOS_COMPILE_MODE */
    if (!cb)
        return;

    eos_critical_ctx_t ctx = eos_critical_enter();

    int next = (s_tail + 1) % s_capacity;
    if (next == s_head)
    {
        if (_queue_expand() < 0)
        {
            EOS_LOG_E("DISPATCHER QUEUE FULL: callback DROPPED! cb=%p user_data=%p", cb, user_data);
            eos_critical_leave(ctx);
            return;
        }
        next = (s_tail + 1) % s_capacity;
    }

    s_queue[s_tail].cb = cb;
    s_queue[s_tail].user_data = user_data;
    s_tail = next;

    eos_critical_leave(ctx);
}

void eos_dispatch_tick(void)
{
#if EOS_COMPILE_MODE == EOS_DEBUG
    if (!async_initialized)
        return;
#endif

    if (s_head == s_tail)
        return;

    eos_critical_ctx_t ctx = eos_critical_enter();

    while (s_head != s_tail)
    {
        eos_dispatcher_item_t item = s_queue[s_head];
        s_head = (s_head + 1) & (s_capacity - 1);

        eos_critical_leave(ctx);

        item.cb(item.user_data);

        ctx = eos_critical_enter();
    }

    eos_critical_leave(ctx);
}
