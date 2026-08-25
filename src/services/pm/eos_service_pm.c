/**
 * @file eos_service_pm.c
 * @brief Power Manager
 */

#include "eos_service_pm.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#define EOS_LOG_TAG "PowerManager"
#include "eos_log.h"
#include "eos_service_config.h"
#include "eos_event.h"
#include "eos_dev_power.h"
#include "eos_config.h"
#include "eos_touch.h"
#include "eos_dispatcher.h"
#include "eos_dfw.h"
/* Macros and Definitions -------------------------------------*/
#define DEBUG_DISABLE_TIMER 1 /**< [Debug] Whether to disable the timer */
#define _DEFAULT_TIMEOUT_SEC 15
/* Variables --------------------------------------------------*/
static lv_timer_t *t; /**< Sleep timer, triggers sleep mode after timeout */
static eos_pm_state_t pm_state = EOS_PM_DISPLAY_ON;
static bool aod_mode = true;
/* Function Implementations -----------------------------------*/
void eos_pm_reset_timer(void);

#if EOS_COMPILE_MODE == EOS_DEBUG
static char *_print_state(eos_pm_state_t state)
{
    switch (state)
    {
        case EOS_PM_DISPLAY_ON:
            return "EOS_PM_DISPLAY_ON";
        case EOS_PM_DISPLAY_AOD:
            return "EOS_PM_DISPLAY_AOD";
        case EOS_PM_SLEEP:
            return "EOS_PM_SLEEP";
        default:
            return "";
    }
}
#endif /* EOS_COMPILE_MODE */
static void _pm_set_state(eos_pm_state_t state)
{
#if EOS_COMPILE_MODE == EOS_DEBUG
    EOS_LOG_I("State: %s -> %s", _print_state(pm_state), _print_state(state));
#else
    EOS_LOG_I("State: %d -> %d", pm_state, state);
#endif /* EOS_COMPILE_MODE */
    pm_state = state;

    eos_dev_power_t *dev = eos_dev_power_get_instance();
    if (dev->ops == NULL || dev->ops->set_power == NULL)
    {
        EOS_LOG_E("Power device OPS not available");
        return;
    }

    switch (state)
    {
        case EOS_PM_DISPLAY_ON:
            eos_event_post(EOS_EVENT_SYSTEM_DISPLAY_ON, NULL, NULL);
            dev->ops->set_power(DEV_POWER_STATE_ON);
            if (t)
                lv_timer_resume(t);
            break;
        case EOS_PM_SLEEP:
            lv_timer_pause(t);
            eos_event_post(EOS_EVENT_SYSTEM_SLEEP, NULL, NULL);
#if EOS_DFW_ENABLE
            eos_dfw_sync();
#endif /* EOS_DFW_ENABLE */
            dev->ops->set_power(DEV_POWER_STATE_SLEEP);
            break;
        case EOS_PM_DISPLAY_AOD:
            lv_timer_pause(t);
            eos_event_post(EOS_EVENT_SYSTEM_DISPLAY_AOD, NULL, NULL);
            dev->ops->set_power(DEV_POWER_STATE_AOD);
            break;
        default:
            EOS_LOG_E("Unknown state");
            return;
    }
    eos_pm_reset_timer();
}

void eos_pm_set_aod_mode(bool enable)
{
    aod_mode = enable;
    eos_config_set_bool(EOS_CONFIG_KEY_AOD_MODE_BOOL, enable);
}

eos_pm_state_t eos_pm_get_state(void)
{
    return pm_state;
}

void eos_pm_request_sleep(void)
{
    if (aod_mode)
    {
        _pm_set_state(EOS_PM_DISPLAY_AOD);
    }
    else
    {
        _pm_set_state(EOS_PM_SLEEP);
    }
}

static void _sleep_timer_cb(lv_timer_t *t)
{
    eos_pm_request_sleep();
    if (t)
    {
        lv_timer_pause(t);
        lv_timer_reset(t);
    }
}

void eos_pm_set_sleep_timeout(uint32_t sec)
{
    if (t)
    {
        lv_timer_set_period(t, sec * 1000);
        lv_timer_reset(t);
    }
}

void eos_pm_wake_up(void)
{
    _pm_set_state(EOS_PM_DISPLAY_ON);
}

void eos_pm_reset_timer(void)
{
    EOS_LOG_I("Sleep timer reset");
    if (t)
        lv_timer_reset(t);
}

static void _indev_pressed_cb(lv_event_t *e)
{
    EOS_LOG_I("Timer paused");
    if (t && pm_state == EOS_PM_DISPLAY_ON)
    {
        lv_timer_pause(t);
        lv_timer_reset(t);
    }
}

static void _indev_released_cb(lv_event_t *e)
{
    EOS_LOG_I("Timer resumed");
    if (t && pm_state == EOS_PM_DISPLAY_ON)
    {
        lv_timer_resume(t);
        lv_timer_reset(t);
    }
}

void eos_service_pm_init(void)
{
    EOS_LOG_I("Power manager init");
    aod_mode = eos_config_get_bool(EOS_CONFIG_KEY_AOD_MODE_BOOL, false);
    uint32_t timer_period_sec = eos_config_get_number(EOS_CONFIG_KEY_SLEEP_TIMEOUT_SEC_NUMBER, _DEFAULT_TIMEOUT_SEC);
#if DEBUG_DISABLE_TIMER
    t = NULL;
#else
    t = lv_timer_create(_sleep_timer_cb, timer_period_sec * 1000, NULL);
    lv_timer_set_repeat_count(t, -1); // Must be infinite, otherwise timer will be deleted
#endif /* DEBUG_DISABLE_TIMER */
    lv_indev_add_event_cb(eos_touch_get_indev(), _indev_pressed_cb, LV_EVENT_PRESSED, NULL);
    lv_indev_add_event_cb(eos_touch_get_indev(), _indev_released_cb, LV_EVENT_RELEASED, NULL);
}
