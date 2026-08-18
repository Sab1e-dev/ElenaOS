/**
 * @file eos_ww_moon_phase.c
 * @brief Watchface moon phase indicator
 */

#include "eos_ww_moon_phase.h"

/* Includes ---------------------------------------------------*/
#include <math.h>
#include "eos_ww_common.h"
#include "eos_icon.h"
#include "eos_service_time.h"
#define EOS_LOG_TAG "MoonPhase"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define _SYNODIC_MONTH_DAYS 29.530588
#define _REF_NEW_MOON_JDN 2451550.5 /* 2000-01-06 (approx. new moon) */
/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static long _julian_day_number(int year, int month, int day)
{
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    return (long)day + (153 * m + 2) / 5 + 365L * y + y / 4 - y / 100 + y / 400 - 32045;
}

static double _illumination_percent(const eos_datetime_t *now)
{
    long jdn = _julian_day_number(now->year, now->month, now->day);
    double age = fmod((double)jdn - _REF_NEW_MOON_JDN, _SYNODIC_MONTH_DAYS);
    if (age < 0)
    {
        age += _SYNODIC_MONTH_DAYS;
    }
    /* Illuminated fraction: 0 at new moon, 100 at full moon */
    return (1.0 - cos(2.0 * M_PI * age / _SYNODIC_MONTH_DAYS)) / 2.0 * 100.0;
}

static void _moon_phase_update(eos_ww_status_t *s)
{
    eos_datetime_t now = eos_time_get();
    eos_ww_status_set_value(s, "%.0f%%", _illumination_percent(&now));
}

lv_obj_t *eos_ww_moon_phase_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    eos_ww_status_t *s = eos_ww_status_create(parent, RI_MOON_FILL);
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);

    eos_ww_status_start_timer(s, _moon_phase_update, 60000);
    return eos_ww_status_get_container(s);
}
