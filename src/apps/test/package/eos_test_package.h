/**
 * @file eos_test_package.h
 * @brief Package installation test module header
 */

#ifndef EOS_TEST_PACKAGE_H
#define EOS_TEST_PACKAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "eos_config.h"

#if EOS_ENABLE_TEST_APP

/* Public function prototypes ---------------------------------*/

/**
 * @brief Start package installation test
 *
 * Creates a new activity with input field and install button for installing
 * .eapk (application) and .ewpk (watchface) packages.
 */
void eos_test_package_start(void);

#endif /* EOS_ENABLE_TEST_APP */

#ifdef __cplusplus
}
#endif

#endif /* EOS_TEST_PACKAGE_H */
