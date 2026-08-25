/**
 * @file eos_ww_battery.c
 * @brief Watchface battery indicator (icon + percent)
 */

#include "eos_ww_battery.h"

/* Includes ---------------------------------------------------*/
#include "eos_ww_common.h"
#include "eos_icon.h"
#include "eos_service_battery.h"
#define EOS_LOG_TAG "Battery"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _battery_update(eos_ww_status_t *s)
{
    int8_t percent = eos_battery_get_percent();
    if (percent < 0)
    {
        eos_ww_status_set_value(s, "N/A");
        eos_ww_status_set_icon(s, RI_BATTERY_LINE);
        return;
    }

    eos_ww_status_set_value(s, "%d%%", percent);
    eos_ww_status_set_icon(s, eos_battery_is_charging() ? RI_BATTERY_CHARGE_FILL : RI_BATTERY_FILL);
}

lv_obj_t *eos_ww_battery_create(lv_obj_t *parent)
{
    EOS_CHECK_PTR_RETURN_VAL(parent, NULL);

    eos_ww_status_t *s = eos_ww_status_create(parent, RI_BATTERY_FILL);
    EOS_CHECK_PTR_RETURN_VAL(s, NULL);

    eos_ww_status_start_timer(s, _battery_update, 1000);
    return eos_ww_status_get_container(s);
}
