/**
 * @file eos_audio_player.h
 * @brief Audio player - orchestrates decoder to speaker pipeline
 */
#ifndef EOS_AUDIO_PLAYER_H
#define EOS_AUDIO_PLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_audio_decoder.h"
#include "eos_error.h"

/* Forward declarations ---------------------------------------*/
struct eos_audio_feed;

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef enum {
    EOS_AUDIO_IDLE,
    EOS_AUDIO_PLAYING,
    EOS_AUDIO_PAUSED,
} eos_audio_player_state_t;

typedef struct eos_audio_player_t eos_audio_player_t;

/** Called when playback completes naturally (not on user stop/pause). */
typedef void (*eos_audio_player_done_cb)(eos_audio_player_t *player, void *user_data);

/**
 * @brief Audio player instance.
 * Initialize with eos_audio_player_init() before use.
 */
struct eos_audio_player_t
{
    eos_audio_player_state_t state;
    eos_audio_decoder_dsc_t dsc;
    bool decoder_open;
    struct eos_audio_feed *feed;
    uint8_t volume;
    bool muted;
    void *cached_src;
    eos_audio_src_type_t cached_src_type;
    eos_audio_player_done_cb done_cb;
    void *done_user_data;
};

/* Public function prototypes --------------------------------*/

/**
 * @brief Initialize a player instance. Must be called before use.
 * @param p Pointer to player instance (caller-allocated)
 */
void eos_audio_player_init(eos_audio_player_t *p);

eos_result_t eos_audio_player_play(eos_audio_player_t *player,
    const void *src, eos_audio_src_type_t src_type);
eos_result_t eos_audio_player_stop(eos_audio_player_t *player);
eos_result_t eos_audio_player_pause(eos_audio_player_t *player);
eos_result_t eos_audio_player_resume(eos_audio_player_t *player);
eos_result_t eos_audio_player_seek(eos_audio_player_t *player, uint32_t sample);

eos_audio_player_state_t eos_audio_player_get_state(eos_audio_player_t *player);
uint32_t eos_audio_player_get_position(eos_audio_player_t *player);
uint32_t eos_audio_player_get_duration(eos_audio_player_t *player);
uint32_t eos_audio_player_get_sample_rate(eos_audio_player_t *player);
eos_result_t eos_audio_player_set_volume(eos_audio_player_t *player, uint8_t vol);
void eos_audio_player_set_mute(eos_audio_player_t *player, bool mute);
bool eos_audio_player_is_muted(eos_audio_player_t *player);

/**
 * @brief Apply player's internal volume to the speaker device.
 * Call this mid-playback after set_volume() to push volume to hardware.
 */
void eos_audio_player_apply_volume(eos_audio_player_t *player);

/**
 * @brief Set a callback invoked when playback reaches end naturally.
 * Not called on explicit stop() or pause().
 */
void eos_audio_player_set_done_callback(eos_audio_player_t *player,
    eos_audio_player_done_cb cb, void *user_data);

/**
 * @brief Save playback state for later restoration (interrupt/resume pattern).
 * Transfers ownership of cached_src to caller; player forgets it.
 * @param[out] saved_src   Source pointer (caller must manage lifetime)
 * @param[out] saved_type  Source type
 * @param[out] saved_pos   Current sample position
 */
void eos_audio_player_save_state(eos_audio_player_t *player,
    void **saved_src, eos_audio_src_type_t *saved_type, uint32_t *saved_pos);

/**
 * @brief Restore playback from a previously saved state.
 * Player must be in IDLE state.
 */
eos_result_t eos_audio_player_restore_state(eos_audio_player_t *player,
    void *src, eos_audio_src_type_t src_type, uint32_t position);

#ifdef __cplusplus
}
#endif

#endif /* EOS_AUDIO_PLAYER_H */
