/**
 * @file eos_ww_temperature.c
 * @brief Watchface temperature indicator (icon + °C)
 */

#include "eos_ww_temperature.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#include "eos_icon.h"
#define EOS_LOG_TAG "Temperature"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _temperature_update(eos_ww_status_t *s, const eos_sensor_raw_data_t *data)
{
    int32_t temp = data->data.temp.temp; /* hundredths of °C */
    eos_ww_status_set_value(s, "%d.%dC", (int)(temp / 100), (int)((temp % 100) / 10));
}

lv_obj_t *eos_ww_temperature_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    eos_ww_status_t *s = eos_ww_status_create(parent, RI_THERMOMETER_FILL);
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);

    eos_ww_status_start_sensor(s, EOS_SENSOR_TYPE_TEMP, _temperature_update, 1000);
    return eos_ww_status_get_container(s);
}
