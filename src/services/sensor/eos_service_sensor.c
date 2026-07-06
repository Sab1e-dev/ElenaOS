/**
 * @file eos_service_sensor.c
 * @brief Sensor service implementation
 */

/* Includes ---------------------------------------------------*/
#include <string.h>
#include "eos_service_sensor.h"
#include "eos_fifo.h"
#include "eos_port_critical.h"
#include "eos_event.h"
#include "eos_core.h"
#include "eos_service_pm.h"
#include "eos_mem.h"
#define EOS_LOG_DISABLE
#define EOS_LOG_TAG "SensorService"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

/**
 * @brief Default sensor mode policies
 *
 *  NORMAL:    No restrictions — subscriber min_interval fully honored.
 *  LOW_POWER: Floor at 1000ms (1 Hz) to reduce bus traffic and CPU wake-ups.
 *  ACTIVE:    No floor — full speed allowed (workout mode).
 *  SLEEP:     Sensors may not be enabled; all active sensors are disabled.
 *
 *  These mirror the battery-service policy pattern (eos_service_battery.c).
 */
static eos_sensor_policy_t _policies[EOS_SENSOR_MODE_COUNT] = {
    [EOS_SENSOR_MODE_NORMAL] = {.min_interval_ms = 0, .allow_enable = true},
    [EOS_SENSOR_MODE_LOW_POWER] = {.min_interval_ms = 1000, .allow_enable = true},
    [EOS_SENSOR_MODE_ACTIVE] = {.min_interval_ms = 0, .allow_enable = true},
    [EOS_SENSOR_MODE_SLEEP] = {.min_interval_ms = 0, .allow_enable = false},
};

static eos_sensor_mode_t _current_mode = EOS_SENSOR_MODE_NORMAL;

/* Variables --------------------------------------------------*/
static eos_sensor_service_instance_t _instances[EOS_SENSOR_TYPE_MAX] = {0};
static bool _service_initialized = false;

/** Nesting depth of eos_sensor_notify broadcast; >0 means inside broadcast */
static int _broadcast_depth = 0;

/* Function Implementations -----------------------------------*/

/* Forward declarations */
static uint32_t _recalc_min_interval(eos_sensor_service_instance_t *inst);

/**
 * @brief Free all subscribers marked for deletion on a given instance.
 *        Called after broadcast_depth drops to 0.
 */
static void _cleanup_marked(eos_sensor_service_instance_t *inst)
{
    sensor_subscriber_t **curr = &inst->subscribers;
    while (*curr)
    {
        if ((*curr)->marked_for_delete)
        {
            sensor_subscriber_t *tmp = *curr;
            *curr = tmp->next;
            eos_free(tmp);
        }
        else
        {
            curr = &(*curr)->next;
        }
    }
}

/**
 * @brief PM event callback — maps power-manager state changes
 *        to sensor operating modes via eos_pm_get_state().
 *
 *  DISPLAY_ON  → NORMAL
 *  DISPLAY_AOD → LOW_POWER
 *  SLEEP       → SLEEP (all sensors disabled)
 */
static void _on_pm_state_change(eos_event_t *e)
{
    (void)e;

    eos_pm_state_t pm_state = eos_pm_get_state();
    eos_sensor_mode_t new_mode;

    switch (pm_state)
    {
        case EOS_PM_DISPLAY_ON:
            new_mode = EOS_SENSOR_MODE_NORMAL;
            break;
        case EOS_PM_DISPLAY_AOD:
            new_mode = EOS_SENSOR_MODE_LOW_POWER;
            break;
        case EOS_PM_SLEEP:
            new_mode = EOS_SENSOR_MODE_SLEEP;
            break;
        default:
            return;
    }

    if (new_mode == _current_mode)
        return;

    EOS_LOG_I("PM mode transition: %d → %d", _current_mode, new_mode);

    eos_sensor_policy_t *old_policy = &_policies[_current_mode];
    eos_sensor_policy_t *new_policy = &_policies[new_mode];
    _current_mode = new_mode;

    /*
     * On SLEEP entry: disable all actively-sampling sensors.
     * On wake-up (SLEEP → NORMAL): re-enable sensors that have subscribers.
     */
    if (!new_policy->allow_enable && old_policy->allow_enable)
    {
        /* Entering SLEEP — save & disable all enabled sensors */
        for (uint32_t i = EOS_SENSOR_TYPE_UNKNOWN + 1; i < EOS_SENSOR_TYPE_MAX; i++)
        {
            eos_sensor_service_instance_t *inst = &_instances[i];
            if (inst->is_enabled && inst->device && inst->device->ops)
            {
                if (inst->device->ops->disable)
                {
                    inst->device->ops->disable(inst->device);
                }
                EOS_LOG_I("SLEEP: disabled sensor type=%d (subscribers=%u)", i, inst->subscriber_count);
            }
        }
    }
    else if (new_policy->allow_enable && !old_policy->allow_enable)
    {
        /* Waking from SLEEP — re-enable sensors with subscribers */
        for (uint32_t i = EOS_SENSOR_TYPE_UNKNOWN + 1; i < EOS_SENSOR_TYPE_MAX; i++)
        {
            eos_sensor_service_instance_t *inst = &_instances[i];
            if (inst->subscriber_count > 0 && !inst->is_enabled && inst->device && inst->device->ops)
            {
                if (inst->device->ops->enable)
                {
                    inst->device->ops->enable(inst->device);
                }
                inst->is_enabled = true;
                /* Re-apply the sample rate */
                if (inst->device->ops->set_sample_rate && inst->sample_period_ms > 0)
                {
                    uint32_t hz = 1000 / inst->sample_period_ms;
                    if (hz == 0)
                        hz = 1;
                    inst->device->ops->set_sample_rate(inst->device, hz);
                }
                EOS_LOG_I("WAKE: re-enabled sensor type=%d (subscribers=%u, period=%ums)",
                          i,
                          inst->subscriber_count,
                          inst->sample_period_ms);
            }
        }
    }
    else if (new_policy->min_interval_ms > old_policy->min_interval_ms)
    {
        /*
         * Downgrade (e.g. NORMAL → LOW_POWER): re-clamp all active sensors.
         * Sensors with period < min_interval_ms get bumped up to the floor.
         */
        for (uint32_t i = EOS_SENSOR_TYPE_UNKNOWN + 1; i < EOS_SENSOR_TYPE_MAX; i++)
        {
            eos_sensor_service_instance_t *inst = &_instances[i];
            if (inst->is_active && inst->sample_period_ms < new_policy->min_interval_ms)
            {
                eos_sensor_set_sample_period((eos_sensor_type_t)i, new_policy->min_interval_ms);
            }
        }
    }
    else if (new_policy->min_interval_ms < old_policy->min_interval_ms)
    {
        /*
         * Upgrade (e.g. LOW_POWER → NORMAL): recalculate from subscriber
         * intervals now that the floor is lower.
         */
        for (uint32_t i = EOS_SENSOR_TYPE_UNKNOWN + 1; i < EOS_SENSOR_TYPE_MAX; i++)
        {
            eos_sensor_service_instance_t *inst = &_instances[i];
            if (inst->subscriber_count > 0)
            {
                uint32_t recalc = _recalc_min_interval(inst);
                if (recalc > 0)
                {
                    eos_sensor_set_sample_period((eos_sensor_type_t)i, recalc);
                }
            }
        }
    }
}

eos_result_t eos_service_sensor_init(void)
{
    if (_service_initialized)
        return EOS_ERR_ALREADY_INITIALIZED;

    for (uint32_t i = EOS_SENSOR_TYPE_UNKNOWN + 1; i < EOS_SENSOR_TYPE_MAX; i++)
    {
        _instances[i].type = (eos_sensor_type_t)i;
        _instances[i].device = eos_dev_sensor_get_default((eos_sensor_type_t)i);
        _instances[i].fifo = eos_fifo_create(EOS_SENSOR_FIFO_CAPACITY * sizeof(eos_sensor_raw_data_t));
        _instances[i].sample_period_ms = 0;
        _instances[i].last_sample_time = 0;
        _instances[i].is_active = false;
        _instances[i].is_enabled = false;
        _instances[i].subscriber_count = 0;
        _instances[i].subscribers = NULL;
        memset(&_instances[i].latest_data, 0, sizeof(eos_sensor_raw_data_t));
    }

    _service_initialized = true;

    /* Subscribe to power-manager state changes for mode transitions */
    eos_event_subscribe(EOS_EVENT_SYSTEM_DISPLAY_ON, _on_pm_state_change, NULL);
    eos_event_subscribe(EOS_EVENT_SYSTEM_DISPLAY_AOD, _on_pm_state_change, NULL);
    eos_event_subscribe(EOS_EVENT_SYSTEM_SLEEP, _on_pm_state_change, NULL);
    EOS_LOG_I("sensor service initialized, PM events subscribed");

    return EOS_OK;
}

eos_result_t eos_sensor_read(eos_sensor_type_t type, eos_sensor_raw_data_t *data)
{
    if (!_service_initialized || !data)
        return EOS_ERR_INVALID_ARG;

    if (type <= EOS_SENSOR_TYPE_UNKNOWN || type >= EOS_SENSOR_TYPE_MAX)
        return EOS_ERR_INVALID_ARG;

    eos_sensor_service_instance_t *inst = &_instances[type];
    if (!inst->fifo || eos_fifo_is_empty(inst->fifo))
        return EOS_ERR_NOT_FOUND;

    uint16_t read = eos_fifo_read(inst->fifo, data, sizeof(eos_sensor_raw_data_t));
    return (read == sizeof(eos_sensor_raw_data_t)) ? EOS_OK : EOS_ERR_NOT_FOUND;
}

eos_result_t eos_sensor_read_latest(eos_sensor_type_t type, eos_sensor_raw_data_t *data)
{
    if (!_service_initialized || !data)
        return EOS_ERR_INVALID_ARG;

    if (type <= EOS_SENSOR_TYPE_UNKNOWN || type >= EOS_SENSOR_TYPE_MAX)
        return EOS_ERR_INVALID_ARG;

    eos_sensor_service_instance_t *inst = &_instances[type];
    memcpy(data, &inst->latest_data, sizeof(eos_sensor_raw_data_t));
    return EOS_OK;
}

/**
 * @brief Recalculate the minimum sample interval from all active subscribers
 */
static uint32_t _recalc_min_interval(eos_sensor_service_instance_t *inst)
{
    uint32_t min_interval = 0;
    sensor_subscriber_t *sub = inst->subscribers;
    while (sub)
    {
        if (!sub->marked_for_delete && sub->min_interval_ms > 0)
        {
            if (min_interval == 0 || sub->min_interval_ms < min_interval)
            {
                min_interval = sub->min_interval_ms;
            }
        }
        sub = sub->next;
    }
    return min_interval;
}

/**
 * @brief Find subscriber node by callback and user_data pair
 * @return Node pointer or NULL if not found
 */
static sensor_subscriber_t *_find_subscriber(eos_sensor_service_instance_t *inst,
                                             eos_sensor_data_cb_t cb,
                                             void *user_data)
{
    sensor_subscriber_t *sub = inst->subscribers;
    while (sub)
    {
        if (!sub->marked_for_delete && sub->cb == cb && sub->user_data == user_data)
        {
            return sub;
        }
        sub = sub->next;
    }
    return NULL;
}

eos_result_t eos_sensor_subscribe(eos_sensor_type_t type,
                                  eos_sensor_data_cb_t cb,
                                  void *user_data,
                                  uint32_t min_interval_ms)
{
    if (!_service_initialized || !cb)
        return EOS_ERR_INVALID_ARG;

    if (type <= EOS_SENSOR_TYPE_UNKNOWN || type >= EOS_SENSOR_TYPE_MAX)
        return EOS_ERR_INVALID_ARG;

    eos_sensor_service_instance_t *inst = &_instances[type];

    /* Overflow protection */
    if (inst->subscriber_count >= UINT16_MAX)
    {
        EOS_LOG_E("subscribe failed: subscriber_count overflow for type %d", type);
        return EOS_ERR_BUSY;
    }

    /* Check for duplicate subscribe (same cb + user_data pair) */
    if (_find_subscriber(inst, cb, user_data))
    {
        EOS_LOG_W("subscribe: type=%d, cb=%p already subscribed — ignored", type, (void *)cb);
        return EOS_OK;
    }

    /* Allocate and prepend subscriber node */
    sensor_subscriber_t *node = (sensor_subscriber_t *)eos_malloc(sizeof(sensor_subscriber_t));
    if (!node)
    {
        EOS_LOG_E("subscribe failed: out of memory for type %d", type);
        return EOS_ERR_MEM;
    }
    node->cb = cb;
    node->user_data = user_data;
    node->min_interval_ms = min_interval_ms;
    node->next = inst->subscribers;
    node->marked_for_delete = false;
    inst->subscribers = node;
    inst->subscriber_count++;

    /* Recalculate the minimum interval across all subscribers */
    uint32_t new_min = _recalc_min_interval(inst);
    if (new_min > 0 && (inst->sample_period_ms == 0 || new_min < inst->sample_period_ms))
    {
        eos_sensor_set_sample_period(type, new_min);
    }

    EOS_LOG_I("subscribe: type=%d, min_interval=%ums, subscribers=%u, period=%ums",
              type,
              min_interval_ms,
              inst->subscriber_count,
              inst->sample_period_ms);
    return EOS_OK;
}

eos_result_t eos_sensor_unsubscribe(eos_sensor_type_t type, eos_sensor_data_cb_t cb, void *user_data)
{
    if (!_service_initialized)
        return EOS_ERR_INVALID_ARG;

    if (type <= EOS_SENSOR_TYPE_UNKNOWN || type >= EOS_SENSOR_TYPE_MAX)
        return EOS_ERR_INVALID_ARG;

    eos_sensor_service_instance_t *inst = &_instances[type];
    if (!cb)
        return EOS_ERR_INVALID_ARG;

    /*
     * Find the subscriber node.
     *
     * If we are inside a broadcast (_broadcast_depth > 0), only mark
     * the node for deferred deletion — the notify loop is still
     * traversing this list and unlinking now would break iteration
     * (especially when the node being unlinked is the "next" pointer
     * that was saved before the callback).
     *
     * If outside broadcast, unlink and free immediately.
     */
    sensor_subscriber_t *sub = inst->subscribers;
    bool found = false;

    while (sub)
    {
        if (!sub->marked_for_delete && sub->cb == cb && sub->user_data == user_data)
        {
            found = true;

            if (_broadcast_depth > 0)
            {
                /* Deferred: keep node in list, skip in notify/recalc/find */
                sub->marked_for_delete = true;
                EOS_LOG_I("unsubscribe: type=%d, cb=%p marked for deferred deletion (depth=%d)",
                          type,
                          (void *)cb,
                          _broadcast_depth);
            }
            else
            {
                /* Immediate unlink and free */
                if (sub == inst->subscribers)
                {
                    inst->subscribers = sub->next;
                }
                else
                {
                    sensor_subscriber_t *prev = inst->subscribers;
                    while (prev && prev->next != sub)
                        prev = prev->next;
                    if (prev)
                        prev->next = sub->next;
                }
                eos_free(sub);
                EOS_LOG_I("unsubscribe: type=%d, cb=%p freed immediately", type, (void *)cb);
            }
            break;
        }
        sub = sub->next;
    }

    if (!found)
    {
        EOS_LOG_W("unsubscribe: type=%d, cb=%p not found — nothing to remove", type, (void *)cb);
        return EOS_ERR_NOT_FOUND;
    }

    /* Decrement count */
    if (inst->subscriber_count > 0)
    {
        inst->subscriber_count--;
    }

    /* Recalculate min interval from remaining subscribers */
    if (inst->subscriber_count == 0)
    {
        /* No subscribers left — disable sensor */
        eos_sensor_set_sample_period(type, 0);
        EOS_LOG_I("unsubscribe: type=%d, last subscriber removed, sensor disabled", type);
    }
    else
    {
        uint32_t new_min = _recalc_min_interval(inst);
        if (new_min > 0 && new_min != inst->sample_period_ms)
        {
            /* Adjust rate — slow down if the fastest subscriber left */
            eos_sensor_set_sample_period(type, new_min);
        }
        EOS_LOG_I("unsubscribe: type=%d, subscribers=%u, period recalculated=%ums",
                  type,
                  inst->subscriber_count,
                  new_min);
    }

    return EOS_OK;
}

eos_result_t eos_sensor_set_sample_period(eos_sensor_type_t type, uint32_t period_ms)
{
    if (!_service_initialized)
        return EOS_ERR_NOT_INITIALIZED;

    if (type <= EOS_SENSOR_TYPE_UNKNOWN || type >= EOS_SENSOR_TYPE_MAX)
        return EOS_ERR_INVALID_ARG;

    eos_sensor_service_instance_t *inst = &_instances[type];

    /* Mode policy clamp */
    eos_sensor_policy_t *policy = &_policies[_current_mode];
    if (period_ms > 0)
    {
        if (!policy->allow_enable)
        {
            EOS_LOG_W("set_sample_period: type=%d rejected — sensors disabled in mode %d", type, _current_mode);
            return EOS_ERR_INVALID_STATE;
        }
        if (policy->min_interval_ms > 0 && period_ms < policy->min_interval_ms)
        {
            EOS_LOG_I("set_sample_period: type=%d clamped %ums → %ums (mode=%d policy floor)",
                      type,
                      period_ms,
                      policy->min_interval_ms,
                      _current_mode);
            period_ms = policy->min_interval_ms;
        }
    }

    inst->sample_period_ms = period_ms;
    inst->is_active = (period_ms > 0);

    if (inst->device && inst->device->ops)
    {
        if (period_ms > 0)
        {
            /* Guard: only call enable on state transition */
            if (!inst->is_enabled && inst->device->ops->enable)
            {
                inst->device->ops->enable(inst->device);
                EOS_LOG_I("set_sample_period: type=%d, ENABLING device at %ums period", type, period_ms);
            }
            inst->is_enabled = true;

            if (inst->device->ops->set_sample_rate)
            {
                uint32_t hz = 1000 / period_ms;
                if (hz == 0)
                    hz = 1;
                inst->device->ops->set_sample_rate(inst->device, hz);
            }
        }
        else
        {
            /* Guard: only call disable on state transition */
            if (inst->is_enabled && inst->device->ops->disable)
            {
                inst->device->ops->disable(inst->device);
                EOS_LOG_I("set_sample_period: type=%d, DISABLING device", type);
            }
            inst->is_enabled = false;
        }
    }

    return EOS_OK;
}

uint32_t eos_sensor_get_sample_period(eos_sensor_type_t type)
{
    if (!_service_initialized)
        return 0;

    if (type <= EOS_SENSOR_TYPE_UNKNOWN || type >= EOS_SENSOR_TYPE_MAX)
        return 0;

    return _instances[type].sample_period_ms;
}

void eos_sensor_notify(eos_sensor_type_t type, const eos_sensor_data_t *data, uint32_t timestamp)
{
    if (!_service_initialized || !data)
        return;

    if (type <= EOS_SENSOR_TYPE_UNKNOWN || type >= EOS_SENSOR_TYPE_MAX)
        return;

    eos_sensor_service_instance_t *inst = &_instances[type];
    if (!inst->fifo)
        return;

    /* Service-layer rate limiting (defense-in-depth) */
    uint32_t now = eos_tick_get();
    if (inst->sample_period_ms > 0 && inst->last_sample_time != 0)
    {
        uint32_t elapsed = now - inst->last_sample_time;
        if (elapsed < inst->sample_period_ms)
        {
            return; /* Within throttle window — drop */
        }
    }
    inst->last_sample_time = now;

    eos_sensor_raw_data_t raw_data = {.type = type, .timestamp = timestamp};
    memcpy(&raw_data.data, data, sizeof(eos_sensor_data_t));

    eos_critical_ctx_t ctx = eos_critical_enter();
    /* Use atomic write to prevent torn entries */
    eos_fifo_write_atomic(inst->fifo, &raw_data, sizeof(eos_sensor_raw_data_t));
    memcpy(&inst->latest_data, &raw_data, sizeof(eos_sensor_raw_data_t));
    eos_critical_leave(ctx);

    EOS_LOG_I("eos_sensor_notify: type=%d, timestamp=%d", type, timestamp);

    /*
     * Broadcast directly to sensor subscribers.
     * Done outside the critical section — callbacks may be long-running
     * (e.g. JS interop). raw_data is a stack copy, safe to pass by pointer.
     *
     * Use broadcast_depth so that eos_sensor_unsubscribe called from
     * within a callback can defer node deletion (mark_for_delete) instead
     * of unlinking mid-traversal and causing use-after-free.
     *
     * Save sub->next before invoking the callback to protect against
     * self-unsubscribe (the node sub itself may be freed after depth
     * drops to 0 and cleanup runs).
     */
    _broadcast_depth++;
    sensor_subscriber_t *sub = inst->subscribers;
    while (sub)
    {
        sensor_subscriber_t *next = sub->next;
        if (!sub->marked_for_delete)
        {
            sub->cb(type, &raw_data, sub->user_data);
        }
        sub = next;
    }
    _broadcast_depth--;

    /* Deferred cleanup: free nodes marked during this broadcast */
    if (_broadcast_depth == 0)
    {
        _cleanup_marked(inst);
    }
}

eos_result_t eos_sensor_set_mode_policy(eos_sensor_mode_t mode, const eos_sensor_policy_t *policy)
{
    if (mode >= EOS_SENSOR_MODE_COUNT)
    {
        return EOS_ERR_INVALID_ARG;
    }

    eos_sensor_policy_t new_policy;
    if (policy)
    {
        new_policy = *policy;
    }
    else
    {
        /* Reset to default */
        switch (mode)
        {
            case EOS_SENSOR_MODE_NORMAL:
                new_policy = (eos_sensor_policy_t){.min_interval_ms = 0, .allow_enable = true};
                break;
            case EOS_SENSOR_MODE_LOW_POWER:
                new_policy = (eos_sensor_policy_t){.min_interval_ms = 1000, .allow_enable = true};
                break;
            case EOS_SENSOR_MODE_ACTIVE:
                new_policy = (eos_sensor_policy_t){.min_interval_ms = 0, .allow_enable = true};
                break;
            case EOS_SENSOR_MODE_SLEEP:
                new_policy = (eos_sensor_policy_t){.min_interval_ms = 0, .allow_enable = false};
                break;
            default:
                return EOS_ERR_INVALID_ARG;
        }
    }

    eos_critical_ctx_t ctx = eos_critical_enter();
    _policies[mode] = new_policy;
    eos_critical_leave(ctx);

    /*
     * If the policy for the current mode changed, re-apply the clamp.
     */
    if (mode == _current_mode)
    {
        for (uint32_t i = EOS_SENSOR_TYPE_UNKNOWN + 1; i < EOS_SENSOR_TYPE_MAX; i++)
        {
            eos_sensor_service_instance_t *inst = &_instances[i];
            if (inst->is_active)
            {
                if (!new_policy.allow_enable)
                {
                    /* New policy forbids enable — disable sensor */
                    if (inst->is_enabled && inst->device && inst->device->ops && inst->device->ops->disable)
                    {
                        inst->device->ops->disable(inst->device);
                        inst->is_enabled = false;
                    }
                }
                else if (inst->sample_period_ms < new_policy.min_interval_ms)
                {
                    /* New floor is higher — clamp period */
                    inst->sample_period_ms = new_policy.min_interval_ms;
                    uint32_t hz = 1000 / new_policy.min_interval_ms;
                    if (hz == 0)
                        hz = 1;
                    if (inst->device && inst->device->ops && inst->device->ops->set_sample_rate)
                    {
                        inst->device->ops->set_sample_rate(inst->device, hz);
                    }
                }
            }
        }
    }

    EOS_LOG_I("policy set for mode %d: min_interval=%ums allow_enable=%d",
              mode,
              new_policy.min_interval_ms,
              new_policy.allow_enable);
    return EOS_OK;
}

eos_result_t eos_sensor_get_mode_policy(eos_sensor_mode_t mode, eos_sensor_policy_t *policy)
{
    if (mode >= EOS_SENSOR_MODE_COUNT || !policy)
    {
        return EOS_ERR_INVALID_ARG;
    }

    eos_critical_ctx_t ctx = eos_critical_enter();
    *policy = _policies[mode];
    eos_critical_leave(ctx);

    return EOS_OK;
}

eos_sensor_mode_t eos_sensor_get_current_mode(void)
{
    return _current_mode;
}
