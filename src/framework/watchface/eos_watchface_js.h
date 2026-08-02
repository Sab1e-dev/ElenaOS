/**
 * @file eos_watchface_js.h
 * @brief JavaScript script watchface implementation
 */

#ifndef EOS_WATCHFACE_JS_H
#define EOS_WATCHFACE_JS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "eos_watchface.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a JS watchface instance with its own Activity
 * @param watchface_id Watchface package ID (e.g. "cn.sab1e.clock")
 * @return eos_watchface_instance_t* Instance pointer, NULL on failure
 *
 * The instance loads the script package from disk and owns its Activity.
 * Caller is responsible for destroying the instance when no longer needed.
 */
eos_watchface_instance_t *eos_watchface_js_create(const char *watchface_id);

/**
 * @brief Initialize JS watchface crash recovery handler
 * Subscribes to EOS_EVENT_SCRIPT_FATAL for watchface callback crashes
 */
void eos_watchface_js_crash_handler_init(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WATCHFACE_JS_H */
