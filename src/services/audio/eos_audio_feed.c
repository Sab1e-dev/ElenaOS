/**
 * @file eos_audio_feed.c
 * @brief Audio feed clock default implementation using LVGL lv_timer
 *
 * All functions are EOS_WEAK. Platform ports may override with DMA ISR
 * or RTOS timer implementations by providing strong definitions.
 */

#include "eos_audio_feed.h"

/* Includes ---------------------------------------------------*/
#include <stdlib.h>
#include "eos_mem.h"
#define EOS_LOG_TAG "AudioFeed"
#include "eos_log.h"
#include "eos_port.h"
#include "lvgl.h"

/* Macros and Definitions -------------------------------------*/

struct eos_audio_feed
{
    lv_timer_t *timer;
    eos_audio_feed_cb_t cb;
    void *user_data;
    bool paused;
};

/* Function Implementations -----------------------------------*/

static void _feed_timer_cb(lv_timer_t *timer)
{
    eos_audio_feed_t *feed = (eos_audio_feed_t *)lv_timer_get_user_data(timer);
    if (feed && feed->cb && !feed->paused)
    {
        feed->cb(feed->user_data);
    }
}

EOS_WEAK eos_audio_feed_t *eos_audio_feed_create(uint32_t period_ms, eos_audio_feed_cb_t cb, void *user_data)
{
    eos_audio_feed_t *feed = eos_malloc_zeroed(sizeof(eos_audio_feed_t));
    if (!feed)
        return NULL;

    feed->cb = cb;
    feed->user_data = user_data;
    feed->paused = false;
    feed->timer = lv_timer_create(_feed_timer_cb, period_ms, feed);
    if (!feed->timer)
    {
        eos_free(feed);
        return NULL;
    }
    return feed;
}

EOS_WEAK void eos_audio_feed_delete(eos_audio_feed_t *feed)
{
    if (!feed)
        return;
    if (feed->timer)
    {
        lv_timer_delete(feed->timer);
    }
    eos_free(feed);
}

EOS_WEAK void eos_audio_feed_pause(eos_audio_feed_t *feed)
{
    if (!feed)
        return;
    feed->paused = true;
}

EOS_WEAK void eos_audio_feed_resume(eos_audio_feed_t *feed)
{
    if (!feed)
        return;
    feed->paused = false;
}
