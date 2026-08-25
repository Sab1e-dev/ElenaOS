/**
 * @file eos_ww_barometer.c
 * @brief Watchface barometer indicator (hPa)
 */

#include "eos_ww_barometer.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#define EOS_LOG_TAG "Barometer"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _barometer_update(eos_ww_status_t *s, const eos_sensor_raw_data_t *data)
{
    int32_t pressure = data->data.baro.pressure; /* Pa */
    eos_ww_status_set_value(s, "%d hPa", (int)(pressure / 100));
}

lv_obj_t *eos_ww_barometer_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    eos_ww_status_t *s = eos_ww_status_create(parent, NULL);
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);

    eos_ww_status_start_sensor(s, EOS_SENSOR_TYPE_BARO, _barometer_update, 1000);
    return eos_ww_status_get_container(s);
}
