/**
 * @file eos_test_sensor.h
 * @brief Sensor test module header
 */

#ifndef EOS_TEST_SENSOR_H
#define EOS_TEST_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
typedef struct {
    const char *name;
    bool passed;
    char details[128];
} eos_sensor_test_result_t;

typedef struct {
    uint32_t total_tests;
    uint32_t passed_tests;
    uint32_t failed_tests;
} eos_sensor_test_stats_t;

/* Public function prototypes --------------------------------*/
void eos_test_sensor_start(void);
void eos_test_sensor_register_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_TEST_SENSOR_H */
