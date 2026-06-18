/**
 * @file eos_test_audio_decoder.h
 * @brief Unit tests for audio decoder, VAR decoder, and audio player
 */

#ifndef EOS_TEST_AUDIO_DECODER_H
#define EOS_TEST_AUDIO_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief Register all audio decoder/player unit tests
 */
void eos_test_audio_decoder_register_tests(void);

/**
 * @brief Start audio decoder/player tests UI page
 */
void eos_test_audio_decoder_start(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_TEST_AUDIO_DECODER_H */
