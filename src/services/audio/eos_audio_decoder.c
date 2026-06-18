/**
 * @file eos_audio_decoder.c
 * @brief Audio decoder registry - linked list of decoder plugins
 */

#include "eos_audio_decoder.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "AudioDecoder"
#include "eos_log.h"
#include "lvgl.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/
static lv_ll_t _decoder_ll;
static bool _decoder_initialized = false;

/* Function Implementations -----------------------------------*/

void eos_audio_decoder_init(void)
{
    if (_decoder_initialized) return;
    lv_ll_init(&_decoder_ll, sizeof(eos_audio_decoder_t));
    _decoder_initialized = true;
    EOS_LOG_I("Audio decoder subsystem initialized");
}

eos_audio_src_type_t eos_audio_src_get_type(const void *src)
{
    if (src == NULL) return EOS_AUDIO_SRC_NONE;

    const uint8_t *p = (const uint8_t *)src;
    if (p[0] == EOS_AUDIO_HEADER_MAGIC) {
        return EOS_AUDIO_SRC_VAR;
    }
    return EOS_AUDIO_SRC_FILE;
}

eos_audio_decoder_t *eos_audio_decoder_create(void)
{
    if (!_decoder_initialized) return NULL;

    eos_audio_decoder_t *decoder = lv_ll_ins_head(&_decoder_ll);
    if (decoder) {
        lv_memzero(decoder, sizeof(eos_audio_decoder_t));
    }
    return decoder;
}

void eos_audio_decoder_delete(eos_audio_decoder_t *decoder)
{
    if (!_decoder_initialized || decoder == NULL) return;
    lv_ll_remove(&_decoder_ll, decoder);
}

eos_audio_decoder_t *eos_audio_decoder_get_next(eos_audio_decoder_t *decoder)
{
    if (!_decoder_initialized) return NULL;
    if (decoder == NULL)
        return lv_ll_get_head(&_decoder_ll);
    return lv_ll_get_next(&_decoder_ll, decoder);
}

void eos_audio_decoder_set_probe_cb(eos_audio_decoder_t *d, eos_audio_decoder_probe_f_t cb)
{
    if (d) d->probe_cb = cb;
}

void eos_audio_decoder_set_open_cb(eos_audio_decoder_t *d, eos_audio_decoder_open_f_t cb)
{
    if (d) d->open_cb = cb;
}

void eos_audio_decoder_set_read_cb(eos_audio_decoder_t *d, eos_audio_decoder_read_f_t cb)
{
    if (d) d->read_cb = cb;
}

void eos_audio_decoder_set_close_cb(eos_audio_decoder_t *d, eos_audio_decoder_close_f_t cb)
{
    if (d) d->close_cb = cb;
}

void eos_audio_decoder_set_seek_cb(eos_audio_decoder_t *d, eos_audio_decoder_seek_f_t cb)
{
    if (d) d->seek_cb = cb;
}

static eos_audio_decoder_t *_decoder_find_match(const void *src, eos_audio_src_type_t src_type,
                                               eos_audio_format_t *format)
{
    eos_audio_decoder_t *dec;
    LV_LL_READ(&_decoder_ll, dec) {
        if (dec->probe_cb == NULL) continue;
        if (dec->probe_cb(src, src_type, format) == EOS_OK) {
            return dec;
        }
    }
    return NULL;
}

eos_result_t eos_audio_decoder_open(eos_audio_decoder_dsc_t *dsc,
    const void *src, eos_audio_src_type_t src_type)
{
    if (!_decoder_initialized) return EOS_ERR_NOT_INITIALIZED;
    if (dsc == NULL || src == NULL) return EOS_ERR_INVALID_ARG;

    if (src_type == EOS_AUDIO_SRC_NONE) {
        src_type = eos_audio_src_get_type(src);
    }

    lv_memzero(dsc, sizeof(eos_audio_decoder_dsc_t));
    dsc->src = src;
    dsc->src_type = src_type;

    eos_audio_decoder_t *dec = _decoder_find_match(src, src_type, &dsc->format);
    if (dec == NULL) {
        EOS_LOG_E("No decoder found for source");
        return EOS_ERR_NOT_FOUND;
    }

    dsc->decoder = dec;
    if (dec->open_cb == NULL) return EOS_ERR_NOT_FOUND;

    return dec->open_cb(dsc);
}

eos_result_t eos_audio_decoder_read(eos_audio_decoder_dsc_t *dsc,
    void *buf, uint32_t buf_size, uint32_t *bytes_read)
{
    if (!_decoder_initialized) return EOS_ERR_NOT_INITIALIZED;
    if (dsc == NULL || dsc->decoder == NULL || dsc->decoder->read_cb == NULL)
        return EOS_ERR_INVALID_ARG;
    return dsc->decoder->read_cb(dsc, buf, buf_size, bytes_read);
}

eos_result_t eos_audio_decoder_seek(eos_audio_decoder_dsc_t *dsc, uint32_t sample)
{
    if (!_decoder_initialized) return EOS_ERR_NOT_INITIALIZED;
    if (dsc == NULL || dsc->decoder == NULL) return EOS_ERR_INVALID_ARG;
    if (dsc->decoder->seek_cb == NULL) return EOS_ERR_DEV_OPS_NOT_SUPPORTED;
    return dsc->decoder->seek_cb(dsc, sample);
}

void eos_audio_decoder_close(eos_audio_decoder_dsc_t *dsc)
{
    if (!_decoder_initialized) return;
    if (dsc == NULL || dsc->decoder == NULL) return;
    if (dsc->decoder->close_cb) {
        dsc->decoder->close_cb(dsc);
    }
    lv_memzero(dsc, sizeof(eos_audio_decoder_dsc_t));
}
