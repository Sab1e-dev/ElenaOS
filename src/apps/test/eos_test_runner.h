/**
 * @file eos_test_runner.h
 * @brief Global unit test runner - aggregates all unit test registrations
 */

#ifndef EOS_TEST_RUNNER_H
#define EOS_TEST_RUNNER_H

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
void eos_test_runner_register_all(void);
void eos_test_runner_start(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_TEST_APP */
#endif /* EOS_TEST_RUNNER_H */
