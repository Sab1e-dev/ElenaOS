/**
 * @file eos_test_runner.c
 * @brief Global unit test runner - aggregates all unit test registrations
 */

#include "eos_test_runner.h"
#if EOS_ENABLE_TEST_APP

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "eos_test_framework.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/
void eos_test_audio_register_tests(void);
void eos_test_stress_register_tests(void);
void eos_test_sensor_register_tests(void);
void eos_test_event_register_tests(void);
void eos_test_permission_register_tests(void);

void eos_test_runner_register_all(void)
{
    eos_test_audio_register_tests();
    eos_test_stress_register_tests();
    eos_test_sensor_register_tests();
    eos_test_event_register_tests();
    eos_test_permission_register_tests();
}

void eos_test_runner_start(void)
{
    eos_test_runner_register_all();
    eos_test_fw_page_start("Unit Tests");
}

#endif /* EOS_ENABLE_TEST_APP */
