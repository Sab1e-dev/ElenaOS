/**
 * @file eos_service_audio.h
 * @brief Audio service - high-level API for speaker and microphone
 */

#ifndef EOS_SERVICE_AUDIO_H
#define EOS_SERVICE_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_error.h"
#include "eos_audio_player.h"

/* Public macros ----------------------------------------------*/

#define EOS_SPEAKER_VOLUME_MIN 0
#define EOS_SPEAKER_VOLUME_MAX 100

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize audio service
 */
void eos_service_audio_init(void);

/**
 * @brief Set speaker volume and persist to config
 * @param volume Volume level (0-100)
 * @return EOS_OK if successful
 */
eos_result_t eos_service_audio_set_volume(uint8_t volume);

/**
 * @brief Get current volume from config
 * @return Volume level (0-100), default 50
 */
uint8_t eos_service_audio_get_volume(void);

/**
 * @brief Enable or disable mute
 * @param mute true to mute, false to unmute
 */
void eos_service_audio_set_mute(bool mute);

/**
 * @brief Check if audio is muted
 * @return true if muted
 */
bool eos_service_audio_is_muted(void);

/**
 * @brief Play an audio file
 * @param file_path Path to audio file (format handled by port layer)
 * @return EOS_OK if successful
 */
eos_result_t eos_service_audio_play(const char *file_path);

/**
 * @brief Play a tone at given frequency and duration
 * @param freq Frequency in Hz
 * @param duration_ms Duration in milliseconds
 * @return EOS_OK if successful
 */
eos_result_t eos_service_audio_play_tone(uint16_t freq, uint32_t duration_ms);

/**
 * @brief Stop current playback
 * @return EOS_OK if successful
 */
eos_result_t eos_service_audio_stop(void);

/**
 * @brief Pause current playback (keeps queue alive for resume)
 * @return EOS_OK if successful
 */
eos_result_t eos_service_audio_pause(void);

/**
 * @brief Resume paused playback
 * @return EOS_OK if successful
 */
eos_result_t eos_service_audio_resume(void);

/**
 * @brief Start recording from microphone
 * @param file_path Path to save recorded audio
 * @return EOS_OK if successful
 */
eos_result_t eos_service_audio_start_recording(const char *file_path);

/**
 * @brief Stop current recording
 * @return EOS_OK if successful
 */
eos_result_t eos_service_audio_stop_recording(void);

/**
 * @brief Check if speaker hardware is available
 * @return true if available
 */
bool eos_service_audio_speaker_available(void);

/**
 * @brief Check if microphone hardware is available
 * @return true if available
 */
bool eos_service_audio_microphone_available(void);

/**
 * @brief Get the service-managed player instance for advanced queries.
 * For normal playback use the service play/stop/pause/resume functions.
 * @return Pointer to the media channel player (never NULL after init).
 */
eos_audio_player_t *eos_service_audio_get_player(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SERVICE_AUDIO_H */
