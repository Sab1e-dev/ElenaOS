/**
 * @file esh_log_bridge.h
 * @brief EOS_LOG bridge for the active ESH frontend
 */

#ifndef ESH_LOG_BRIDGE_H
#define ESH_LOG_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "eos_error.h"
#include "esh.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Attach ESH as an EOS_LOG listener
 *
 * The bridge disables the system "std_log" listener while attached so a
 * shared physical output receives each log only once. Detach the bridge when
 * ESH no longer owns that output to restore the previous listener state.
 * @param esh ESH instance receiving log messages
 * @return EOS_OK on success
 */
eos_result_t esh_log_bridge_attach(esh_t *esh);

/**
 * @brief Detach ESH from EOS_LOG
 * @return EOS_OK on success
 */
eos_result_t esh_log_bridge_detach(void);

#ifdef __cplusplus
}
#endif

#endif /* ESH_LOG_BRIDGE_H */
