/**
 * @file eos_utils.h
 * @brief Utility functions and macros
 */

#ifndef EOS_UTILS_H
#define EOS_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

#define EOS_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* EOS_UTILS_H */
