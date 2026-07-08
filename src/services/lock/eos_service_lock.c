/**
 * @file eos_service_lock.c
 * @brief Lock screen service implementation
 */

#include "eos_service_lock.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#define EOS_LOG_TAG "LockService"
#include "eos_log.h"
#include "eos_event.h"
#include "eos_config.h"
#include "eos_service_config.h"
#include "eos_mem.h"
#include "eos_service_pm.h"
#include "eos_lock_page.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/
static bool _lock_active = false;

/* Function Implementations -----------------------------------*/

static void _on_display_on_cb(eos_event_t *e)
{
    (void)e;

    /* Don't re-show if already active */
    if (_lock_active)
    {
        return;
    }

    /* Check if password is enabled and a hash exists */
    bool enabled = eos_config_get_bool(EOS_CONFIG_KEY_PASSWORD_ENABLED_BOOL, false);
    if (!enabled)
    {
        return;
    }

    char *hash = eos_config_get_string(EOS_CONFIG_KEY_PASSWORD_HASH_STR, "");
    bool has_hash = (hash && strlen(hash) > 0);
    eos_free(hash);

    if (!has_hash)
    {
        return;
    }

    /* Show lock screen security barrier on lv_layer_top() */
    _lock_active = true;
    eos_lock_page_show();
    EOS_LOG_I("Lock screen shown");
}

bool eos_lock_screen_is_active(void)
{
    return _lock_active || eos_lock_page_is_visible();
}

void eos_lock_screen_dismiss(void)
{
    if (!_lock_active && !eos_lock_page_is_visible())
    {
        return;
    }

    /* Don't dismiss while sleeping — async verify callback may fire during sleep.
     * Defer dismissal until next DISPLAY_ON so the lock page survives sleep/wake. */
    if (eos_pm_get_state() == EOS_PM_SLEEP)
    {
        EOS_LOG_W("Lock dismiss deferred: device is asleep");
        return;
    }

    _lock_active = false;

    /* Reset input devices to release any pressed objects before destroying overlay */
    lv_indev_t *indev = NULL;
    while ((indev = lv_indev_get_next(indev)) != NULL)
    {
        lv_indev_reset(indev, NULL);
    }

    eos_lock_page_hide();
    EOS_LOG_I("Lock screen dismissed");
}

void eos_service_lock_init(void)
{
    EOS_LOG_I("Lock service init — security barrier mode (not chrome overlay)");
    eos_event_subscribe_ex(EOS_EVENT_SYSTEM_DISPLAY_ON, _on_display_on_cb, NULL, NULL);
}
