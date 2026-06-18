/**
 * @file eos_audio_feed.h
 * @brief Audio feed clock abstraction - decouples audio pipeline from LVGL timer
 *
 * Default implementation uses LVGL lv_timer as fallback.
 * Platform ports can override with DMA half-complete ISR or RTOS timer
 * via EOS_WEAK function redefinition.
 */

#ifndef EOS_AUDIO_FEED_H
#define EOS_AUDIO_FEED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef void (*eos_audio_feed_cb_t)(void *user_data);

typedef struct eos_audio_feed eos_audio_feed_t;

/* Public function prototypes --------------------------------*/

/**
 * @brief Create a periodic feed clock
 * @param period_ms Interval in milliseconds (recommended 10-30ms)
 * @param cb Callback invoked each period
 * @param user_data User data passed to callback
 * @return Feed handle, or NULL on failure
 *
 * Default implementation uses lv_timer_create (EOS_WEAK).
 * Override to use DMA ISR / RTOS timer on real hardware.
 */
eos_audio_feed_t *eos_audio_feed_create(uint32_t period_ms,
    eos_audio_feed_cb_t cb, void *user_data);

/**
 * @brief Delete feed clock and stop callbacks
 */
void eos_audio_feed_delete(eos_audio_feed_t *feed);

/**
 * @brief Pause the feed clock (no callbacks)
 */
void eos_audio_feed_pause(eos_audio_feed_t *feed);

/**
 * @brief Resume the feed clock
 */
void eos_audio_feed_resume(eos_audio_feed_t *feed);

#ifdef __cplusplus
}
#endif

#endif /* EOS_AUDIO_FEED_H */
