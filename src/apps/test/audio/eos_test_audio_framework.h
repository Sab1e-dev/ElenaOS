#ifndef EOS_TEST_AUDIO_FRAMEWORK_H
#define EOS_TEST_AUDIO_FRAMEWORK_H

#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

#include <stdbool.h>
#include <stdint.h>

#define EOS_TEST_NAME_MAX 80
#define EOS_TEST_MAX 128

typedef bool (*eos_test_fn_t)(void);

void eos_test_register(const char *name, eos_test_fn_t fn);
void eos_test_record(const char *name, bool passed, const char *detail);
void eos_test_reset(void);
void eos_test_run_all(void);
void eos_test_run_group(const char *prefix);
uint32_t eos_test_get_total(void);
uint32_t eos_test_get_passed(void);
uint32_t eos_test_get_failed(void);
void eos_test_audio_page_start(void);

#endif /* EOS_ENABLE_TEST_APP */
#endif /* EOS_TEST_AUDIO_FRAMEWORK_H */
