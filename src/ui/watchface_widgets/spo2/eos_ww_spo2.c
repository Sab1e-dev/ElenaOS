/**
 * @file eos_ww_spo2.c
 * @brief Watchface SpO2 indicator (icon + percent)
 */

#include "eos_ww_spo2.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#include "eos_icon.h"
#define EOS_LOG_TAG "SpO2"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _spo2_update(eos_ww_status_t *s, const eos_sensor_raw_data_t *data)
{
    eos_ww_status_set_value(s, "%d%%", data->data.spo2.spo2);
}

lv_obj_t *eos_ww_spo2_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    eos_ww_status_t *s = eos_ww_status_create(parent, RI_DROP_FILL);
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);

    eos_ww_status_start_sensor(s, EOS_SENSOR_TYPE_SPO2, _spo2_update, 1000);
    return eos_ww_status_get_container(s);
}
