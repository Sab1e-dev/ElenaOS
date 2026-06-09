/**
 * @file eos_side_button.c
 * @brief Side button
 */

#include "eos_side_button.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "eos_dispatcher.h"
#include "input/eos_input.h"
#include "eos_control_center.h"
#include "eos_service_pm.h"
#include "eos_service_lock.h"
#include "eos_chrome_manager.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _side_button_async_cb(void *user_data)
{
    if (eos_pm_get_state() == EOS_PM_SLEEP)
    {
        eos_pm_wake_up();
        return;
    }
    eos_pm_reset_timer();

    /* Security: block all side button actions while lock screen is active */
    if (eos_lock_screen_is_active()) {
        return;
    }

    /* Absorb side key events when any overlay is open (e.g. permission panel),
     * but allow the side button to toggle the control center which it controls */
    if (eos_chrome_manager_any_overlay_open()) {
        const eos_chrome_overlay_t *top = eos_chrome_manager_get_top_overlay();
        if (top != eos_control_center_get_overlay_descriptor()) {
            return;
        }
    }

    eos_button_state_t state = (eos_button_state_t)(intptr_t)user_data;
    switch (state)
    {
    case EOS_BUTTON_STATE_CLICKED:
        eos_control_panel_slide_change();
    default:
        break;
    }
}

void eos_side_button_report(eos_button_state_t state)
{
    eos_dispatcher_call(_side_button_async_cb, (void *)(intptr_t)state);
}
