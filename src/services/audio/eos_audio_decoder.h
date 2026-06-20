/**
 * @file eos_audio_decoder.h
 * @brief Audio decoder abstraction - pluggable format decoders
 */

#ifndef EOS_AUDIO_DECODER_H
#define EOS_AUDIO_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_error.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef enum
{
    EOS_AUDIO_SRC_NONE,
    EOS_AUDIO_SRC_FILE, /**< File path string */
} eos_audio_src_type_t;

typedef struct
{
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint32_t total_samples; /**< 0 if unknown (streaming) */
    uint32_t duration_ms;   /**< 0 if unknown */
} eos_audio_format_t;

struct eos_audio_decoder_t;
struct eos_audio_decoder_dsc_t;

typedef eos_result_t (*eos_audio_decoder_probe_f_t)(
    const void *src, eos_audio_src_type_t src_type, eos_audio_format_t *format);

typedef eos_result_t (*eos_audio_decoder_open_f_t)(
    struct eos_audio_decoder_dsc_t *dsc);

typedef eos_result_t (*eos_audio_decoder_read_f_t)(
    struct eos_audio_decoder_dsc_t *dsc, void *buf, uint32_t buf_size, uint32_t *bytes_read);

typedef void (*eos_audio_decoder_close_f_t)(
    struct eos_audio_decoder_dsc_t *dsc);

typedef eos_result_t (*eos_audio_decoder_seek_f_t)(
    struct eos_audio_decoder_dsc_t *dsc, uint32_t sample);

typedef struct eos_audio_decoder_dsc_t
{
    struct eos_audio_decoder_t *decoder;
    const void *src;
    eos_audio_src_type_t src_type;
    eos_audio_format_t format;
    uint32_t current_sample;
    void *user_data;
} eos_audio_decoder_dsc_t;

typedef struct eos_audio_decoder_t
{
    eos_audio_decoder_probe_f_t probe_cb;
    eos_audio_decoder_open_f_t open_cb;
    eos_audio_decoder_read_f_t read_cb;
    eos_audio_decoder_close_f_t close_cb;
    eos_audio_decoder_seek_f_t seek_cb;
    const char *name;
    void *user_data;
} eos_audio_decoder_t;

/* Public function prototypes --------------------------------*/

/**
 * @brief Initialize audio decoder subsystem and built-in decoders
 */
void eos_audio_decoder_init(void);

/**
 * @brief Detect the source type for an audio source
 * @param src Audio source (file path)
 * @return Source type
 */
eos_audio_src_type_t eos_audio_src_get_type(const void *src);

/**
 * @brief Create a new audio decoder instance and add to registry
 * @return eos_audio_decoder_t* Pointer to new decoder, or NULL on failure
 */
eos_audio_decoder_t *eos_audio_decoder_create(void);
void eos_audio_decoder_delete(eos_audio_decoder_t *decoder);
eos_audio_decoder_t *eos_audio_decoder_get_next(eos_audio_decoder_t *decoder);

void eos_audio_decoder_set_probe_cb(eos_audio_decoder_t *d, eos_audio_decoder_probe_f_t cb);
void eos_audio_decoder_set_open_cb(eos_audio_decoder_t *d, eos_audio_decoder_open_f_t cb);
void eos_audio_decoder_set_read_cb(eos_audio_decoder_t *d, eos_audio_decoder_read_f_t cb);
void eos_audio_decoder_set_close_cb(eos_audio_decoder_t *d, eos_audio_decoder_close_f_t cb);
void eos_audio_decoder_set_seek_cb(eos_audio_decoder_t *d, eos_audio_decoder_seek_f_t cb);

eos_result_t eos_audio_decoder_open(eos_audio_decoder_dsc_t *dsc,
    const void *src, eos_audio_src_type_t src_type);
eos_result_t eos_audio_decoder_read(eos_audio_decoder_dsc_t *dsc,
    void *buf, uint32_t buf_size, uint32_t *bytes_read);
eos_result_t eos_audio_decoder_seek(eos_audio_decoder_dsc_t *dsc, uint32_t sample);
void eos_audio_decoder_close(eos_audio_decoder_dsc_t *dsc);

#ifdef __cplusplus
}
#endif

#endif /* EOS_AUDIO_DECODER_H */
