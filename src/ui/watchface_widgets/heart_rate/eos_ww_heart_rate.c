/**
 * @file eos_ww_heart_rate.c
 * @brief Watchface heart rate indicator (icon + BPM)
 */

#include "eos_ww_heart_rate.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#include "eos_icon.h"
#define EOS_LOG_TAG "HeartRate"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _heart_rate_update(eos_ww_status_t *s, const eos_sensor_raw_data_t *data)
{
    eos_ww_status_set_value(s, "%d", data->data.hr.heart_rate);
}

lv_obj_t *eos_ww_heart_rate_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    eos_ww_status_t *s = eos_ww_status_create(parent, RI_HEART_PULSE_FILL);
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);

    eos_ww_status_start_sensor(s, EOS_SENSOR_TYPE_HR, _heart_rate_update, 1000);
    return eos_ww_status_get_container(s);
}
