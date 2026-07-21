/**
 * @file eos_service_time.h
 * @brief Time service
 */

#ifndef EOS_SERVICE_TIME_H
#define EOS_SERVICE_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_dev_time.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize time service
 * @return EOS_OK if successful, error code otherwise
 */
eos_result_t eos_service_time_init(void);

/**
 * @brief Get system time
 * @return Current datetime with millisecond precision
 */
eos_datetime_t eos_time_get(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SERVICE_TIME_H */
