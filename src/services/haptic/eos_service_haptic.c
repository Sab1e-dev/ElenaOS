/**
 * @file eos_service_haptic.c
 * @brief Haptic service
 */

#include "eos_service_haptic.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "eos_service_config.h"
#include "eos_dev_vibrator.h"
#include "eos_utils.h"

/* Macros and Definitions -------------------------------------*/

#define _TICK_PERIOD 20
#define _BUZZ_PERIOD 50
#define _VIBRATE_LONG_PERIOD 150

#define _TICK_STRENGTH 127
#define _BUZZ_STRENGTH 200
#define _VIBRATE_LONG_STRENGTH 255

/* Variables --------------------------------------------------*/

static lv_timer_t *t;
static eos_haptic_strength_t strength_option;

/* Function Implementations -----------------------------------*/

static void _haptic_timer_stop_cb(lv_timer_t *t)
{
    eos_dev_vibrator_t *dev = eos_dev_vibrator_get_instance();
    if (dev->ops && dev->ops->off)
    {
        dev->ops->off();
    }
    lv_timer_pause(t);
}

static void _haptic_common(uint32_t period, uint8_t strength)
{
    eos_dev_vibrator_t *dev = eos_dev_vibrator_get_instance();

    switch (strength_option)
    {
        case EOS_HAPTIC_STRENGTH_OFF:
            return;
        case EOS_HAPTIC_STRENGTH_INTENSE:
            strength += 25;
            break;
        default:
            break;
    }
    EOS_CLAMP(strength, 0, 255);

    if (dev->ops && dev->ops->on)
    {
        dev->ops->on(strength);
    }

    lv_timer_set_period(t, period);
    lv_timer_reset(t);
    lv_timer_resume(t);
}

void eos_haptic_tick(void)
{
    _haptic_common(_TICK_PERIOD, _TICK_STRENGTH);
}

void eos_haptic_buzz(void)
{
    _haptic_common(_BUZZ_PERIOD, _BUZZ_STRENGTH);
}

void eos_haptic_vibrate_long(void)
{
    _haptic_common(_VIBRATE_LONG_PERIOD, _VIBRATE_LONG_STRENGTH);
}

void eos_haptic_set_strength(eos_haptic_strength_t s)
{
    strength_option = s;
    EOS_CLAMP(strength_option, 0, 255);
    eos_config_set_number(EOS_CONFIG_KEY_VIBRATOR_STRENGTH_NUMBER, strength_option);
}

void eos_service_haptic_init(void)
{
    t = lv_timer_create_basic();
    lv_timer_set_period(t, 0);
    lv_timer_set_repeat_count(t, -1);
    lv_timer_set_cb(t, _haptic_timer_stop_cb);
    lv_timer_pause(t);
}
