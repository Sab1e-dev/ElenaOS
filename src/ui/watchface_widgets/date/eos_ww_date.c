/**
 * @file eos_ww_date.c
 * @brief Watchface date (weekday + month/day)
 */

#include "eos_ww_date.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#include "eos_service_time.h"
#define EOS_LOG_TAG "Date"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _date_update(eos_ww_status_t *s)
{
    eos_datetime_t now = eos_time_get();
    eos_ww_status_set_value(s, "%s %02d/%02d", eos_ww_weekday_abbr(now.day_of_week), now.month, now.day);
}

lv_obj_t *eos_ww_date_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    eos_ww_status_t *s = eos_ww_status_create(parent, NULL);
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);

    eos_ww_status_start_timer(s, _date_update, 1000);
    return eos_ww_status_get_container(s);
}
