/**
 * @file eos_ww_steps.c
 * @brief Watchface step counter (icon + count)
 */

#include "eos_ww_steps.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#include "eos_icon.h"
#define EOS_LOG_TAG "Steps"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _steps_update(eos_ww_status_t *s, const eos_sensor_raw_data_t *data)
{
    eos_ww_status_set_value(s, "%u", data->data.step.steps);
}

lv_obj_t *eos_ww_steps_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    eos_ww_status_t *s = eos_ww_status_create(parent, RI_WALK_FILL);
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);

    eos_ww_status_start_sensor(s, EOS_SENSOR_TYPE_STEP, _steps_update, 1000);
    return eos_ww_status_get_container(s);
}
