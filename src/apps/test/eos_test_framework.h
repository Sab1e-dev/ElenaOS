/**
 * @file eos_test_framework.h
 * @brief Unit test framework - registration, execution, UI page
 */

#ifndef EOS_TEST_FRAMEWORK_H
#define EOS_TEST_FRAMEWORK_H

#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/* Public macros ----------------------------------------------*/
#define EOS_TEST_NAME_MAX 80
#define EOS_TEST_MAX 256

#define EOS_EXPECT_TRUE(cond, name, msg) eos_test_record(name, (cond), msg)

#define EOS_EXPECT_FALSE(cond, name, msg) eos_test_record(name, !(cond), msg)

#define EOS_EXPECT_EQ(a, b, name, msg) eos_test_record(name, (a) == (b), msg)

#define EOS_EXPECT_NE(a, b, name, msg) eos_test_record(name, (a) != (b), msg)

#define EOS_EXPECT_NOT_NULL(ptr, name, msg) eos_test_record(name, (ptr) != NULL, msg)

#define EOS_EXPECT_NULL(ptr, name, msg) eos_test_record(name, (ptr) == NULL, msg)

/* Public typedefs --------------------------------------------*/
typedef bool (*eos_test_fn_t)(void);

/* Public function prototypes ---------------------------------*/
void eos_test_register(const char *name, eos_test_fn_t fn);
void eos_test_record(const char *name, bool passed, const char *detail);
void eos_test_reset(void);
void eos_test_run_all(void);
void eos_test_run_group(const char *prefix);
uint32_t eos_test_get_total(void);
uint32_t eos_test_get_passed(void);
uint32_t eos_test_get_failed(void);
void eos_test_fw_page_start(const char *title);
bool eos_test_assert(bool cond, const char *file, int line, const char *msg);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_TEST_APP */
#endif /* EOS_TEST_FRAMEWORK_H */
