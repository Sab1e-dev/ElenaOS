/**
 * @file eos_test_battery_history.h
 * @brief Battery history chart visualization test module header
 */

#ifndef EOS_TEST_BATTERY_HISTORY_H
#define EOS_TEST_BATTERY_HISTORY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "eos_config.h"

#if EOS_ENABLE_TEST_APP

/* Public function prototypes ---------------------------------*/

/**
 * @brief Start battery history chart test
 *
 * Creates a new activity with a chart showing battery level history.
 */
void eos_test_battery_history_start(void);

#endif /* EOS_ENABLE_TEST_APP */

#ifdef __cplusplus
}
#endif

#endif /* EOS_TEST_BATTERY_HISTORY_H */