/**
 * @file eos_test_stress.h
 * @brief SPM stress test - memory leak check
 */

#ifndef EOS_TEST_STRESS_H
#define EOS_TEST_STRESS_H

#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/
void eos_test_stress_register_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_TEST_APP */
#endif /* EOS_TEST_STRESS_H */
