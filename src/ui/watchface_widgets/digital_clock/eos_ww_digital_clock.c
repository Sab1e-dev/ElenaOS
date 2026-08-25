/**
 * @file eos_ww_digital_clock.c
 * @brief Watchface digital clock (HH:MM:SS)
 */

#include "eos_ww_digital_clock.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#include "eos_service_time.h"
#define EOS_LOG_TAG "DigitalClock"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _digital_clock_update(eos_ww_status_t *s)
{
    eos_datetime_t now = eos_time_get();
    eos_ww_status_set_value(s, "%02d:%02d:%02d", now.hour, now.min, now.sec);
}

lv_obj_t *eos_ww_digital_clock_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    eos_ww_status_t *s = eos_ww_status_create(parent, NULL);
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);

    eos_ww_status_start_timer(s, _digital_clock_update, 1000);
    return eos_ww_status_get_container(s);
}
