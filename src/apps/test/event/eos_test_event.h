/**
 * @file eos_test_event.h
 * @brief Event system test module header
 */

#ifndef EOS_TEST_EVENT_H
#define EOS_TEST_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/
void eos_test_event_start(void);
void eos_test_event_register_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_TEST_EVENT_H */
