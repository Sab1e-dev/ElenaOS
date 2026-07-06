/**
 * @file eos_service_battery.c
 * @brief Battery service implementation with policy-based approach
 */

#include "eos_service_battery.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdlib.h>
#include "eos_port_critical.h"
#include "eos_core.h"
#include "eos_event.h"
#include "eos_mem.h"
#define EOS_LOG_TAG "BatteryService"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

/**
 * @brief Battery history entry
 */
typedef struct
{
    int8_t percent;
    bool charging;
    uint32_t ts;
} battery_history_entry_t;

/**
 * @brief Battery policies for different modes
 */
static eos_battery_policy_t _policies[EOS_BATTERY_MODE_COUNT] = {
    [EOS_BATTERY_MODE_NORMAL] =
        {
            .interval_ms = 60000,
            .threshold_percent = 1,
        },
    [EOS_BATTERY_MODE_LOW_POWER] =
        {
            .interval_ms = 300000,
            .threshold_percent = 2,
        },
    [EOS_BATTERY_MODE_CHARGING] =
        {
            .interval_ms = 5000,
            .threshold_percent = 1,
        },
    [EOS_BATTERY_MODE_ACTIVE] =
        {
            .interval_ms = 10000,
            .threshold_percent = 1,
        },
};

/**
 * @brief Battery history ring buffer size
 */
#define BATTERY_HISTORY_SIZE 128

/* Variables --------------------------------------------------*/
/**
 * @brief Battery service internal state
 */
typedef struct
{
    bool initialized;
    const eos_battery_dev_ops_t *dev_ops;
    eos_battery_mode_t current_mode;
    eos_battery_state_t state;
    int8_t last_reported_percent;
    bool last_reported_charging;
    eos_event_code_t event_id;
    lv_timer_t *timer;
    uint32_t last_update_tick;
    int8_t current_percent;
    bool current_charging;
    uint32_t cycle_count;
    battery_history_entry_t history[BATTERY_HISTORY_SIZE];
    uint32_t history_write_idx;
    uint32_t history_count;
} battery_svc_t;

static battery_svc_t _svc = {0};

/* Function Implementations -----------------------------------*/

static eos_battery_mode_t _determine_mode(int8_t percent, bool charging)
{
    (void)percent;
    if (charging)
    {
        return EOS_BATTERY_MODE_CHARGING;
    }
    if (percent <= 20)
    {
        return EOS_BATTERY_MODE_LOW_POWER;
    }
    return EOS_BATTERY_MODE_NORMAL;
}

static void _history_write(int8_t percent, bool charging)
{
    uint32_t now = eos_tick_get();
    if (_svc.history_count > 0)
    {
        uint32_t last_idx = (_svc.history_write_idx + BATTERY_HISTORY_SIZE - 1) % BATTERY_HISTORY_SIZE;
        battery_history_entry_t *last = &_svc.history[last_idx];
        if (last->percent == percent && last->charging == charging)
        {
            return;
        }
    }

    _svc.history[_svc.history_write_idx].percent = percent;
    _svc.history[_svc.history_write_idx].charging = charging;
    _svc.history[_svc.history_write_idx].ts = now;
    _svc.history_write_idx = (_svc.history_write_idx + 1) % BATTERY_HISTORY_SIZE;
    if (_svc.history_count < BATTERY_HISTORY_SIZE)
    {
        _svc.history_count++;
    }
}

static void _notify_if_changed(void)
{
    bool percent_changed =
        (_svc.last_reported_percent != -1
         && abs(_svc.state.percent - _svc.last_reported_percent) >= _policies[_svc.current_mode].threshold_percent);
    bool charging_changed = (_svc.last_reported_percent != -1 && _svc.state.charging != _svc.last_reported_charging);

    if (percent_changed || charging_changed)
    {
        eos_event_post(_svc.event_id, &_svc.state, NULL);
    }

    _svc.last_reported_percent = _svc.state.percent;
    _svc.last_reported_charging = _svc.state.charging;
}

static void _update_mode(void)
{
    eos_battery_mode_t new_mode = _determine_mode(_svc.current_percent, _svc.current_charging);
    if (new_mode != _svc.current_mode)
    {
        _svc.current_mode = new_mode;
        EOS_LOG_I("Battery mode changed to %d", new_mode);
        if (_svc.timer)
        {
            lv_timer_set_period(_svc.timer, _policies[new_mode].interval_ms);
        }
    }
}

static void _battery_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!_svc.dev_ops || !_svc.dev_ops->request_update)
    {
        return;
    }

    _svc.dev_ops->request_update();
}

eos_result_t eos_service_battery_init(void)
{
    if (_svc.initialized)
    {
        return EOS_ERR_ALREADY_INITIALIZED;
    }

    eos_dev_battery_t *dev = eos_dev_battery_get_instance();
    if (!dev || !dev->ops.request_update)
    {
        EOS_LOG_E("Initialization failed: Battery device not valid");
        return EOS_FAILED;
    }

    _svc.dev_ops = &dev->ops;
    _svc.current_mode = EOS_BATTERY_MODE_NORMAL;
    _svc.last_reported_percent = -1;
    _svc.last_reported_charging = false;
    _svc.current_percent = -1;
    _svc.current_charging = false;
    _svc.state.valid = false;
    _svc.cycle_count = 0;
    _svc.history_write_idx = 0;
    _svc.history_count = 0;

    _svc.event_id = eos_event_register_id();

    uint32_t interval = _policies[_svc.current_mode].interval_ms;
    _svc.timer = lv_timer_create(_battery_timer_cb, interval, NULL);
    if (!_svc.timer)
    {
        EOS_LOG_E("Initialization failed: Failed to create timer");
        return EOS_ERR_MEM;
    }

    _svc.initialized = true;
    _svc.last_update_tick = eos_tick_get();

    _svc.dev_ops->request_update();

    EOS_LOG_I("Battery service initialized, event_id: %u", _svc.event_id);
    return EOS_OK;
}

void eos_battery_report_raw(const eos_battery_raw_t *raw)
{
    if (!_svc.initialized || !raw)
    {
        return;
    }

    eos_critical_ctx_t ctx = eos_critical_enter();

    if (raw->percent < 0 || raw->percent > 100)
    {
        EOS_LOG_W("Invalid battery percent: %d", raw->percent);
        eos_critical_leave(ctx);
        return;
    }

    _svc.current_percent = raw->percent;
    _svc.current_charging = raw->charging;

    _svc.state.percent = raw->percent;
    _svc.state.charging = raw->charging;
    _svc.state.voltage_mv = raw->voltage_mv;
    _svc.state.current_ma = raw->current_ma;
    _svc.state.ts = eos_tick_get();
    _svc.state.valid = true;

    _history_write(raw->percent, raw->charging);

    eos_critical_leave(ctx);

    _update_mode();
    _notify_if_changed();

    EOS_LOG_I("Battery: %d%%, %s", _svc.state.percent, _svc.state.charging ? "charging" : "discharging");
}

int8_t eos_battery_get_percent(void)
{
    if (!_svc.initialized || !_svc.state.valid)
    {
        return -1;
    }
    return _svc.state.percent;
}

int16_t eos_battery_get_voltage_mv(void)
{
    if (!_svc.initialized || !_svc.state.valid)
    {
        return -1;
    }
    return _svc.state.voltage_mv;
}

bool eos_battery_is_charging(void)
{
    if (!_svc.initialized || !_svc.state.valid)
    {
        return false;
    }
    return _svc.state.charging;
}

bool eos_battery_get_state(eos_battery_state_t *state)
{
    if (!_svc.initialized || !state || !_svc.state.valid)
    {
        return false;
    }
    *state = _svc.state;
    return true;
}

uint32_t eos_battery_get_design_capacity(void)
{
    eos_dev_battery_t *dev = eos_dev_battery_get_instance();
    if (dev && dev->design_capacity > 0)
    {
        return dev->design_capacity;
    }
    return 500;
}

uint32_t eos_battery_get_current_capacity(void)
{
    if (!_svc.initialized)
    {
        return 0;
    }
    return (uint32_t)((_svc.state.percent * 500) / 100);
}

uint32_t eos_battery_get_cycle_count(void)
{
    return _svc.cycle_count;
}

void eos_battery_set_calc_fn(eos_battery_calc_fn_t fn)
{
    (void)fn;
}

eos_result_t eos_battery_set_policy(eos_battery_mode_t mode, const eos_battery_policy_t *policy)
{
    if (!_svc.initialized || mode >= EOS_BATTERY_MODE_COUNT || !policy)
    {
        return EOS_ERR_INVALID_ARG;
    }

    eos_battery_policy_t new_policy = *policy;
    if (new_policy.interval_ms < 1000)
    {
        new_policy.interval_ms = 1000;
    }
    if (new_policy.threshold_percent == 0)
    {
        new_policy.threshold_percent = 1;
    }

    eos_critical_ctx_t ctx = eos_critical_enter();
    _policies[mode].interval_ms = new_policy.interval_ms;
    _policies[mode].threshold_percent = new_policy.threshold_percent;
    eos_critical_leave(ctx);

    if (_svc.current_mode == mode && _svc.timer)
    {
        lv_timer_set_period(_svc.timer, new_policy.interval_ms);
    }

    EOS_LOG_I("Battery policy set for mode %d: interval=%ums, threshold=%d%%",
              mode,
              new_policy.interval_ms,
              new_policy.threshold_percent);
    return EOS_OK;
}

eos_result_t eos_battery_get_policy(eos_battery_mode_t mode, eos_battery_policy_t *policy)
{
    if (mode >= EOS_BATTERY_MODE_COUNT || !policy)
    {
        return EOS_ERR_INVALID_ARG;
    }

    eos_critical_ctx_t ctx = eos_critical_enter();
    policy->interval_ms = _policies[mode].interval_ms;
    policy->threshold_percent = _policies[mode].threshold_percent;
    eos_critical_leave(ctx);

    return EOS_OK;
}

eos_event_code_t eos_battery_get_event_id(void)
{
    return _svc.event_id;
}

uint32_t eos_battery_get_history_count(void)
{
    return _svc.history_count;
}

bool eos_battery_get_history_entry(uint32_t index, eos_battery_state_t *entry)
{
    if (!_svc.initialized || index >= _svc.history_count)
    {
        return false;
    }

    uint32_t actual_idx =
        (_svc.history_write_idx + BATTERY_HISTORY_SIZE - _svc.history_count + index) % BATTERY_HISTORY_SIZE;
    entry->percent = _svc.history[actual_idx].percent;
    entry->charging = _svc.history[actual_idx].charging;
    entry->ts = _svc.history[actual_idx].ts;
    entry->valid = true;
    return true;
}
