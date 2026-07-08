/**
 * @file eos_test_snapshot.h
 * @brief LVGL snapshot performance test - measures tick cost of full-screen snapshot
 */

#ifndef EOS_TEST_SNAPSHOT_H
#define EOS_TEST_SNAPSHOT_H

#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public function prototypes --------------------------------*/
void eos_test_snapshot_register_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_TEST_APP */
#endif /* EOS_TEST_SNAPSHOT_H */
