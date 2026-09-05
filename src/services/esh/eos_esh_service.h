/**
 * @file eos_esh_service.h
 * @brief EOS-owned ESH runtime service
 */

#ifndef EOS_ESH_SERVICE_H
#define EOS_ESH_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stddef.h>
#include <stdint.h>

#include "esh.h"

/* Public function prototypes ---------------------------------*/
/**
 * @brief Initialize and claim the EOS-owned ESH instance
 * @param frontend Frontend used for ESH output
 * @return EOS_OK on success, otherwise an error code
 */
eos_result_t eos_esh_service_init(const esh_frontend_t *frontend);

/**
 * @brief Submit transport bytes to EOS for serialized ESH processing
 * @param data Input bytes
 * @param length Number of input bytes
 * @return Number of bytes accepted into the bounded input queue
 */
size_t eos_esh_service_feed(const uint8_t *data, size_t length);

/**
 * @brief Run EOS-owned periodic ESH maintenance
 *
 * Command bytes are executed by the EOS dispatcher. This function only runs
 * bounded protocol maintenance such as YMODEM timeout handling.
 */
void eos_esh_service_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ESH_SERVICE_H */
