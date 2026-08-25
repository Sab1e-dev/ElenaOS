/**
 * @file eos_watchface_builtin.h
 * @brief Built-in fallback watchface implementation
 */

#ifndef EOS_WATCHFACE_BUILTIN_H
#define EOS_WATCHFACE_BUILTIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "eos_watchface.h"
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a built-in watchface instance with its own Activity
 * @return eos_watchface_instance_t* Instance pointer, NULL on failure
 *
 * The instance owns its Activity and manages its lifecycle independently.
 * Caller is responsible for destroying the instance when no longer needed.
 */
eos_watchface_instance_t *eos_watchface_builtin_create(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WATCHFACE_BUILTIN_H */
