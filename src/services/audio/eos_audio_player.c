/**
 * @file eos_audio_player.c
 * @brief Audio player implementation
 */
#include "eos_audio_player.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EOS_LOG_TAG "AudioPlayer"
#include "eos_log.h"
#include "eos_dev_speaker.h"
#include "eos_audio_feed.h"

/* Macros and Definitions -------------------------------------*/

#define FEED_TIMER_PERIOD_MS 30
#define PRE_FILL_BUFFERS      8

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _player_stop_internal(eos_audio_player_t *p);

static bool _player_fill_one_buffer(eos_audio_player_t *p)
{
    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (spk == NULL || spk->ops == NULL) return false;

    void *buf = NULL;
    uint32_t cap = 0;
    if (spk->ops->borrow == NULL || spk->ops->borrow(&buf, &cap) != 0)
        return false;

    if (p->muted)
    {
        memset(buf, 0, cap);
        spk->ops->enqueue(buf, cap);
        return true;
    }

    uint32_t bytes_read = 0;
    eos_result_t res = eos_audio_decoder_read(&p->dsc, buf, cap, &bytes_read);

    if (res == EOS_OK && bytes_read == 0 &&
        p->dsc.format.total_samples > 0 &&
        p->dsc.current_sample < p->dsc.format.total_samples)
    {
        p->dsc.current_sample = p->dsc.format.total_samples;
    }

    spk->ops->enqueue(buf, bytes_read);
    return (res == EOS_OK && bytes_read > 0);
}

static void _feed_cb(void *user_data)
{
    eos_audio_player_t *p = (eos_audio_player_t *)user_data;
    if (p->state != EOS_AUDIO_PLAYING) return;

    while (_player_fill_one_buffer(p)) { }

    if (p->dsc.current_sample >= p->dsc.format.total_samples && p->dsc.format.total_samples > 0)
    {
        EOS_LOG_I("Playback complete");
        _player_stop_internal(p);
        if (p->done_cb)
        {
            p->done_cb(p, p->done_user_data);
        }
    }
}

static void _player_stop_internal(eos_audio_player_t *p)
{
    if (p->feed)
    {
        eos_audio_feed_delete(p->feed);
        p->feed = NULL;
    }

    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (spk && spk->ops && spk->ops->stop)
    {
        spk->ops->stop();
    }

    if (p->decoder_open)
    {
        eos_audio_decoder_close(&p->dsc);
        p->decoder_open = false;
    }

    p->state = EOS_AUDIO_IDLE;
}

static void _player_start_feed(eos_audio_player_t *p)
{
    if (p->feed)
    {
        eos_audio_feed_delete(p->feed);
    }
    p->feed = eos_audio_feed_create(FEED_TIMER_PERIOD_MS, _feed_cb, p);
}

void eos_audio_player_init(eos_audio_player_t *p)
{
    if (p == NULL) return;
    memset(p, 0, sizeof(*p));
    p->state = EOS_AUDIO_IDLE;
    p->volume = 50;
    p->muted = false;
    EOS_LOG_I("Audio player initialized");
}

eos_result_t eos_audio_player_play(eos_audio_player_t *p,
    const void *src, eos_audio_src_type_t src_type)
{
    if (p == NULL || src == NULL) return EOS_ERR_INVALID_ARG;

    if (src_type == EOS_AUDIO_SRC_NONE)
    {
        src_type = eos_audio_src_get_type(src);
        if (src_type == EOS_AUDIO_SRC_NONE) return EOS_ERR_INVALID_ARG;
    }

    if (p->state != EOS_AUDIO_IDLE)
    {
        _player_stop_internal(p);
    }

    if (p->cached_src && p->cached_src_type == EOS_AUDIO_SRC_FILE)
    {
        free(p->cached_src);
    }
    p->cached_src = NULL;
    if (src_type == EOS_AUDIO_SRC_FILE && src)
    {
        p->cached_src = strdup((const char *)src);
        p->cached_src_type = EOS_AUDIO_SRC_FILE;
    }
    else
    {
        p->cached_src = NULL;
        p->cached_src_type = EOS_AUDIO_SRC_NONE;
    }

    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (spk == NULL || spk->ops == NULL)
    {
        return EOS_ERR_DEV_NOT_FOUND;
    }

    eos_result_t res = eos_audio_decoder_open(&p->dsc, src, src_type);
    if (res != EOS_OK)
    {
        EOS_LOG_E("Failed to open decoder");
        return res;
    }
    p->decoder_open = true;

    int ret = spk->ops->open(p->dsc.format.sample_rate,
                              p->dsc.format.channels,
                              p->dsc.format.bits_per_sample);
    if (ret != 0)
    {
        EOS_LOG_E("Failed to open speaker");
        eos_audio_decoder_close(&p->dsc);
        p->decoder_open = false;
        return EOS_ERR_DEV_ERROR;
    }

    if (spk->ops->set_volume)
    {
        spk->ops->set_volume(p->volume);
    }

    int filled = 0;
    for (int i = 0; i < PRE_FILL_BUFFERS; i++)
    {
        if (_player_fill_one_buffer(p)) filled++;
        else break;
    }

    if (filled == 0 && p->dsc.format.total_samples == 0)
    {
        EOS_LOG_E("Cannot play: source has no samples and unknown duration");
        if (spk->ops->stop) spk->ops->stop();
        eos_audio_decoder_close(&p->dsc);
        p->decoder_open = false;
        p->state = EOS_AUDIO_IDLE;
        return EOS_ERR_DEV_ERROR;
    }

    p->state = EOS_AUDIO_PLAYING;
    _player_start_feed(p);

    EOS_LOG_I("Playing: %s", (const char *)src);

    return EOS_OK;
}

eos_result_t eos_audio_player_stop(eos_audio_player_t *p)
{
    if (p == NULL) return EOS_ERR_INVALID_ARG;
    _player_stop_internal(p);
    if (p->cached_src_type == EOS_AUDIO_SRC_FILE && p->cached_src)
    {
        free(p->cached_src);
    }
    p->cached_src = NULL;
    p->cached_src_type = EOS_AUDIO_SRC_NONE;
    return EOS_OK;
}

eos_result_t eos_audio_player_pause(eos_audio_player_t *p)
{
    if (p == NULL) return EOS_ERR_INVALID_ARG;
    if (p->state != EOS_AUDIO_PLAYING) return EOS_ERR_INVALID_STATE;

    eos_audio_feed_pause(p->feed);

    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (spk && spk->ops && spk->ops->pause)
    {
        spk->ops->pause();
    }
    p->state = EOS_AUDIO_PAUSED;
    return EOS_OK;
}

eos_result_t eos_audio_player_resume(eos_audio_player_t *p)
{
    if (p == NULL) return EOS_ERR_INVALID_ARG;
    if (p->state != EOS_AUDIO_PAUSED) return EOS_ERR_INVALID_STATE;

    for (int i = 0; i < PRE_FILL_BUFFERS; i++)
    {
        if (!_player_fill_one_buffer(p)) break;
    }

    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (spk && spk->ops && spk->ops->resume)
    {
        spk->ops->resume();
    }
    p->state = EOS_AUDIO_PLAYING;
    eos_audio_feed_resume(p->feed);
    return EOS_OK;
}

eos_result_t eos_audio_player_seek(eos_audio_player_t *p, uint32_t sample)
{
    if (p == NULL) return EOS_ERR_INVALID_ARG;
    if (!p->decoder_open) return EOS_ERR_INVALID_STATE;

    uint32_t total = p->dsc.format.total_samples;
    if (total == 0) return EOS_ERR_INVALID_STATE;
    if (sample >= total) sample = total - 1;

    if (p->dsc.decoder && p->dsc.decoder->seek_cb)
    {
        eos_result_t res = eos_audio_decoder_seek(&p->dsc, sample);
        if (res == EOS_OK)
        {
            EOS_LOG_I("Seeked to sample %u / %u (native)", sample, total);
            return EOS_OK;
        }
        /* Fall through: decoder has seek_cb but it failed — use reopen fallback */
    }

    eos_audio_player_state_t prev_state = p->state;

    if (p->feed)
    {
        eos_audio_feed_delete(p->feed);
        p->feed = NULL;
    }

    if (p->decoder_open)
    {
        eos_audio_decoder_close(&p->dsc);
        p->decoder_open = false;
    }

    eos_result_t res = eos_audio_decoder_open(&p->dsc, p->cached_src, p->cached_src_type);
    if (res != EOS_OK)
    {
        EOS_LOG_E("Seek: failed to re-open decoder");
        p->state = EOS_AUDIO_IDLE;
        return res;
    }
    p->decoder_open = true;

    if (sample > 0)
    {
        uint32_t bytes_per_frame = (p->dsc.format.bits_per_sample / 8) * p->dsc.format.channels;
        uint32_t aligned_chunk = bytes_per_frame * 512;
        if (aligned_chunk < 1024) aligned_chunk = 1024;
        uint32_t to_skip = sample * bytes_per_frame;
        uint32_t skipped = 0;
        uint8_t *discard = malloc(aligned_chunk);

        if (discard)
        {
            while (skipped < to_skip)
            {
                uint32_t chunk = (to_skip - skipped) > aligned_chunk ? aligned_chunk : (to_skip - skipped);
                uint32_t bytes_read = 0;
                res = eos_audio_decoder_read(&p->dsc, discard, chunk, &bytes_read);
                if (res != EOS_OK || bytes_read == 0) break;
                skipped += bytes_read;
            }
            free(discard);
        }
    }
    p->dsc.current_sample = sample;

    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (spk && spk->ops)
    {
        int ret = spk->ops->open(p->dsc.format.sample_rate,
                                  p->dsc.format.channels,
                                  p->dsc.format.bits_per_sample);
        if (ret != 0)
        {
            eos_audio_decoder_close(&p->dsc);
            p->decoder_open = false;
            p->state = EOS_AUDIO_IDLE;
            return EOS_ERR_DEV_ERROR;
        }

        if (spk->ops->set_volume)
        {
            spk->ops->set_volume(p->volume);
        }
    }

    if (prev_state != EOS_AUDIO_IDLE)
    {
        for (int i = 0; i < PRE_FILL_BUFFERS; i++)
        {
            if (!_player_fill_one_buffer(p)) break;
        }
    }

    p->state = prev_state;
    if (prev_state != EOS_AUDIO_IDLE)
    {
        _player_start_feed(p);
    }

    EOS_LOG_I("Seeked to sample %u / %u", sample, total);
    return EOS_OK;
}

eos_audio_player_state_t eos_audio_player_get_state(eos_audio_player_t *p)
{
    if (p == NULL) return EOS_AUDIO_IDLE;
    return p->state;
}

uint32_t eos_audio_player_get_position(eos_audio_player_t *p)
{
    if (p == NULL || !p->decoder_open) return 0;
    return p->dsc.current_sample;
}

uint32_t eos_audio_player_get_duration(eos_audio_player_t *p)
{
    if (p == NULL || !p->decoder_open) return 0;
    return p->dsc.format.total_samples;
}

uint32_t eos_audio_player_get_sample_rate(eos_audio_player_t *p)
{
    if (p == NULL || !p->decoder_open) return 0;
    return p->dsc.format.sample_rate;
}

eos_result_t eos_audio_player_set_volume(eos_audio_player_t *p, uint8_t vol)
{
    if (p == NULL) return EOS_ERR_INVALID_ARG;
    if (vol > 100) vol = 100;
    p->volume = vol;
    return EOS_OK;
}

void eos_audio_player_apply_volume(eos_audio_player_t *p)
{
    if (p == NULL) return;
    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (spk && spk->ops && spk->ops->set_volume)
    {
        spk->ops->set_volume(p->volume);
    }
}

void eos_audio_player_set_mute(eos_audio_player_t *p, bool mute)
{
    if (p == NULL) return;
    p->muted = mute;
}

bool eos_audio_player_is_muted(eos_audio_player_t *p)
{
    if (p == NULL) return false;
    return p->muted;
}

void eos_audio_player_set_done_callback(eos_audio_player_t *p,
    eos_audio_player_done_cb cb, void *user_data)
{
    if (p == NULL) return;
    p->done_cb = cb;
    p->done_user_data = user_data;
}

void eos_audio_player_save_state(eos_audio_player_t *p,
    void **saved_src, eos_audio_src_type_t *saved_type, uint32_t *saved_pos)
{
    if (p == NULL || saved_src == NULL || saved_type == NULL || saved_pos == NULL)
        return;
    *saved_src = p->cached_src;
    *saved_type = p->cached_src_type;
    *saved_pos = p->dsc.current_sample;
    p->cached_src = NULL;
    p->cached_src_type = EOS_AUDIO_SRC_NONE;
}

eos_result_t eos_audio_player_restore_state(eos_audio_player_t *p,
    void *src, eos_audio_src_type_t src_type, uint32_t position)
{
    if (p == NULL || src == NULL) return EOS_ERR_INVALID_ARG;
    if (p->state != EOS_AUDIO_IDLE) return EOS_ERR_INVALID_STATE;

    eos_result_t r = eos_audio_player_play(p, src, src_type);
    if (r != EOS_OK) return r;

    if (position > 0)
    {
        r = eos_audio_player_seek(p, position);
    }
    return r;
}
