/**
 * @file eos_esh_service.c
 * @brief EOS-owned ESH runtime service
 */

#include "eos_esh_service.h"

/* Includes ---------------------------------------------------*/
#include <stdbool.h>
#include "eos_dispatcher.h"
#include "eos_port_critical.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_ESH_SERVICE_INPUT_QUEUE_SIZE (256U)
#define EOS_ESH_SERVICE_DISPATCH_BUDGET (64U)

/* Variables --------------------------------------------------*/
static esh_t s_esh;
static esh_owner_token_t s_owner;
static bool s_initialized;
static uint8_t s_input_queue[EOS_ESH_SERVICE_INPUT_QUEUE_SIZE];
static size_t s_input_head;
static size_t s_input_tail;
static size_t s_input_count;
static bool s_dispatch_pending;

/* Function Implementations -----------------------------------*/
static void _eos_esh_service_dispatch(void *user_data);

static bool _eos_esh_service_schedule_dispatch(void)
{
    bool schedule = false;
    eos_critical_ctx_t ctx = eos_critical_enter();

    if ((s_input_count > 0U) && !s_dispatch_pending)
    {
        s_dispatch_pending = true;
        schedule = true;
    }

    eos_critical_leave(ctx);

    if (schedule && !eos_dispatcher_try_call(_eos_esh_service_dispatch, NULL))
    {
        ctx = eos_critical_enter();
        s_dispatch_pending = false;
        eos_critical_leave(ctx);
        return false;
    }
    return true;
}

static void _eos_esh_service_dispatch(void *user_data)
{
    uint8_t data[EOS_ESH_SERVICE_DISPATCH_BUDGET];
    size_t length;

    (void)user_data;

    if (!s_initialized)
    {
        return;
    }

    for (;;)
    {
        eos_critical_ctx_t ctx = eos_critical_enter();

        length = 0U;
        while ((length < sizeof(data)) && (s_input_count > 0U))
        {
            data[length++] = s_input_queue[s_input_head];
            s_input_head = (s_input_head + 1U) % EOS_ESH_SERVICE_INPUT_QUEUE_SIZE;
            s_input_count--;
        }

        eos_critical_leave(ctx);

        if (length == 0U)
        {
            break;
        }

        (void)esh_input(&s_esh, s_owner, data, length);
    }

    {
        eos_critical_ctx_t ctx = eos_critical_enter();
        s_dispatch_pending = false;
        eos_critical_leave(ctx);
    }
    (void)_eos_esh_service_schedule_dispatch();
}

eos_result_t eos_esh_service_init(const esh_frontend_t *frontend)
{
    eos_result_t result;

    if (frontend == NULL)
    {
        return EOS_ERR_INVALID_ARG;
    }
    if (s_initialized)
    {
        return EOS_OK;
    }

    result = esh_init(&s_esh);
    if (result != EOS_OK)
    {
        return result;
    }

    result = esh_claim(&s_esh, frontend, ESH_CLAIM_TAKEOVER, &s_owner);
    if (result == EOS_OK)
    {
        s_initialized = true;
    }
    return result;
}

size_t eos_esh_service_feed(const uint8_t *data, size_t length)
{
    size_t accepted = 0U;

    if (!s_initialized || ((data == NULL) && (length > 0U)))
    {
        return 0U;
    }

    {
        eos_critical_ctx_t ctx = eos_critical_enter();

        while ((accepted < length) && (s_input_count < EOS_ESH_SERVICE_INPUT_QUEUE_SIZE))
        {
            s_input_queue[s_input_tail] = data[accepted++];
            s_input_tail = (s_input_tail + 1U) % EOS_ESH_SERVICE_INPUT_QUEUE_SIZE;
            s_input_count++;
        }

        eos_critical_leave(ctx);
    }

    (void)_eos_esh_service_schedule_dispatch();

    return accepted;
}

void eos_esh_service_poll(void)
{
    if (s_initialized)
    {
        /* Retry scheduling if the dispatcher was temporarily full.  Parsing
         * is still never performed directly by this maintenance call. */
        (void)_eos_esh_service_schedule_dispatch();
        esh_poll(&s_esh);
    }
}
