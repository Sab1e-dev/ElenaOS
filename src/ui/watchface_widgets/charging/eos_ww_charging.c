/**
 * @file eos_ww_charging.c
 * @brief Watchface charging indicator (shown only while charging)
 */

#include "eos_ww_charging.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#include "eos_icon.h"
#include "eos_service_battery.h"
#define EOS_LOG_TAG "Charging"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _charging_update(eos_ww_status_t *s)
{
    if (eos_battery_is_charging())
    {
        lv_obj_clear_flag(s->container, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(s->container, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *eos_ww_charging_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    eos_ww_status_t *s = eos_ww_status_create(parent, RI_BATTERY_CHARGE_FILL);
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);

    eos_ww_status_set_value(s, "");
    eos_ww_status_start_timer(s, _charging_update, 1000);
    return eos_ww_status_get_container(s);
}
