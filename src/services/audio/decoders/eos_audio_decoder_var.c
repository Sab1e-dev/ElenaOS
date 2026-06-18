/**
 * @file eos_audio_decoder_var.c
 * @brief VAR decoder: embedded raw PCM from eos_audio_dsc_t
 */

#include "eos_audio_decoder_var.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EOS_LOG_TAG "AudioDecVAR"
#include "eos_log.h"
#include "eos_audio_decoder.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

typedef struct
{
    const uint8_t *data;
    uint32_t data_size;
    uint32_t read_pos;
    uint32_t bytes_per_sample;
} var_dec_data_t;

/* Function Implementations -----------------------------------*/

static eos_result_t _var_probe(const void *src, eos_audio_src_type_t src_type,
                               eos_audio_format_t *format)
{
    if (src_type != EOS_AUDIO_SRC_VAR)
        return EOS_FAILED;
    if (src == NULL)
        return EOS_FAILED;

    const eos_audio_dsc_t *dsc = (const eos_audio_dsc_t *)src;
    if (dsc->magic != EOS_AUDIO_HEADER_MAGIC)
        return EOS_FAILED;

    format->sample_rate = dsc->sample_rate;
    format->channels = dsc->channels;
    format->bits_per_sample = dsc->bits_per_sample;
    format->total_samples = dsc->total_samples;
    format->duration_ms = dsc->total_samples * 1000 / dsc->sample_rate;
    return EOS_OK;
}

static eos_result_t _var_open(eos_audio_decoder_dsc_t *dsc)
{
    const eos_audio_dsc_t *adsc = (const eos_audio_dsc_t *)dsc->src;

    var_dec_data_t *vd = malloc(sizeof(var_dec_data_t));
    if (!vd)
        return EOS_ERR_MEM;

    vd->data = adsc->data;
    vd->data_size = adsc->data_size;
    vd->read_pos = 0;
    vd->bytes_per_sample = (adsc->bits_per_sample / 8) * adsc->channels;

    dsc->user_data = vd;
    EOS_LOG_I("VAR decoder opened: %dHz %dch %dbits, %d samples",
              adsc->sample_rate, adsc->channels, adsc->bits_per_sample,
              adsc->total_samples);
    return EOS_OK;
}

static eos_result_t _var_read(eos_audio_decoder_dsc_t *dsc,
                              void *buf, uint32_t buf_size, uint32_t *bytes_read)
{
    var_dec_data_t *vd = (var_dec_data_t *)dsc->user_data;
    if (!vd || vd->read_pos >= vd->data_size)
    {
        *bytes_read = 0;
        return EOS_OK;
    }

    uint32_t remaining = vd->data_size - vd->read_pos;
    uint32_t to_read = (buf_size < remaining) ? buf_size : remaining;

    memcpy(buf, vd->data + vd->read_pos, to_read);
    vd->read_pos += to_read;
    *bytes_read = to_read;

    dsc->current_sample += to_read / vd->bytes_per_sample;
    return EOS_OK;
}

static void _var_close(eos_audio_decoder_dsc_t *dsc)
{
    if (dsc->user_data)
    {
        free(dsc->user_data);
        dsc->user_data = NULL;
    }
}

static eos_result_t _var_seek(eos_audio_decoder_dsc_t *dsc, uint32_t sample)
{
    var_dec_data_t *vd = (var_dec_data_t *)dsc->user_data;
    if (!vd) return EOS_ERR_INVALID_STATE;

    uint32_t byte_offset = sample * vd->bytes_per_sample;
    if (byte_offset >= vd->data_size) return EOS_ERR_INVALID_ARG;

    vd->read_pos = byte_offset;
    dsc->current_sample = sample;
    return EOS_OK;
}

void eos_audio_decoder_var_init(void)
{
    eos_audio_decoder_t *dec = eos_audio_decoder_create();
    eos_audio_decoder_set_probe_cb(dec, _var_probe);
    eos_audio_decoder_set_open_cb(dec, _var_open);
    eos_audio_decoder_set_read_cb(dec, _var_read);
    eos_audio_decoder_set_close_cb(dec, _var_close);
    eos_audio_decoder_set_seek_cb(dec, _var_seek);
    dec->name = "VAR";
    EOS_LOG_I("Registered VAR decoder");
}
