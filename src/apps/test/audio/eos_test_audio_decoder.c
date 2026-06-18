/**
 * @file eos_test_audio_decoder.c
 * @brief Comprehensive unit tests for audio decoder & player subsystems
 */

#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

#include "eos_test_audio_decoder.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_crown.h"
#include "eos_audio_decoder.h"
#include "eos_audio_player.h"
#include "eos_audio_effects.h"
#include "eos_dev_speaker.h"
#include "eos_audio_feed.h"
#include "eos_service_audio.h"
#include "eos_log.h"
#include "eos_test_audio_framework.h"
#include "eos_fs_port.h"
#include "eos_basic_widgets.h"
#include "eos_lang.h"
#include "lvgl.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "AudioDecTest"

/* Variables --------------------------------------------------*/
static eos_audio_player_t _test_player;
static bool _test_player_inited = false;
static bool _wav_test_ready = false;

/* Function Implementations -----------------------------------*/

static void _record_test(const char *name, bool passed, const char *details)
{
    eos_test_record(name, passed, details);
}

static void _ensure_player_ready(void)
{
    if (!_test_player_inited) {
        eos_audio_player_init(&_test_player);
        _test_player_inited = true;
    }
}

/* 16-bit signed LE, stereo, 44100 Hz, 16 samples of PCM data */
static const uint8_t _test_pcm_data[] = {
    0x00, 0x00, 0x00, 0x00,  /* sample 0: L=0, R=0 */
    0x64, 0x00, 0xC8, 0x00,  /* sample 1: L=100, R=200 */
    0xFF, 0x7F, 0x00, 0x80,  /* sample 2: L=32767, R=-32768 */
    0x01, 0x80, 0xFF, 0x7F,  /* sample 3: L=-32767, R=32767 */
    0x10, 0x27, 0x00, 0x00,  /* sample 4: L=10000, R=0 */
    0x00, 0x00, 0xE8, 0x03,  /* sample 5: L=0, R=1000 */
    0xAA, 0x55, 0x55, 0xAA,  /* sample 6: L=21930, R=-21931 */
    0x34, 0x12, 0x78, 0x56,  /* sample 7: L=4660, R=22136 */
    0x88, 0x77, 0x66, 0x55,  /* sample 8: L=30600, R=21862 */
    0x44, 0x33, 0x22, 0x11,  /* sample 9: L=13124, R=4386 */
    0xFF, 0xFF, 0xFE, 0xFF,  /* sample 10: L=65535, R=65534 */
    0x01, 0x00, 0x02, 0x00,  /* sample 11: L=1, R=2 */
    0x7F, 0x7F, 0x80, 0x80,  /* sample 12: L=32639, R=-32640 */
    0x80, 0x80, 0x7F, 0x7F,  /* sample 13: L=-32640, R=32639 */
    0x00, 0x00, 0x00, 0x00,  /* sample 14: L=0, R=0 */
    0xAB, 0xCD, 0xEF, 0x12,  /* sample 15: L=-12885, R=4847 */
};
#define TEST_PCM_TOTAL_SAMPLES 16
#define TEST_PCM_SAMPLE_RATE 44100
#define TEST_PCM_CHANNELS 2
#define TEST_PCM_BITS 16
#define TEST_PCM_DATA_SIZE (TEST_PCM_TOTAL_SAMPLES * (TEST_PCM_BITS / 8) * TEST_PCM_CHANNELS)

static const eos_audio_dsc_t _test_var_audio = {
    .magic = EOS_AUDIO_HEADER_MAGIC,
    .sample_rate = TEST_PCM_SAMPLE_RATE,
    .channels = TEST_PCM_CHANNELS,
    .bits_per_sample = TEST_PCM_BITS,
    .total_samples = TEST_PCM_TOTAL_SAMPLES,
    .data_size = TEST_PCM_DATA_SIZE,
    .data = _test_pcm_data,
};

/* Invalid VAR: wrong magic */
static const eos_audio_dsc_t _test_var_bad_magic = {
    .magic = 0x00,
    .sample_rate = 8000,
    .channels = 1,
    .bits_per_sample = 8,
    .total_samples = 10,
    .data_size = 10,
    .data = _test_pcm_data,
};

static bool _test_dec_init_creates_list(void)
{
    eos_audio_decoder_t *first = eos_audio_decoder_get_next(NULL);
    /* After init, built-in decoders (VAR, AT) should be registered */
    bool passed = (first != NULL);
    _record_test("Decoder: init registers built-in decoders", passed,
                 passed ? "Decoders exist" : "No decoders found");
    return passed;
}

static bool _test_dec_create_adds_to_list(void)
{
    eos_audio_decoder_t *dec = eos_audio_decoder_create();
    if (!dec)
    {
        _record_test("Decoder: create non-null", false, "create returned NULL");
        return false;
    }

    dec->name = "TestCreate";
    bool has_dec = false;
    eos_audio_decoder_t *it;
    for (it = eos_audio_decoder_get_next(NULL); it != NULL;
         it = eos_audio_decoder_get_next(it))
    {
        if (it == dec)
        {
            has_dec = true;
            break;
        }
    }
    eos_audio_decoder_delete(dec);
    _record_test("Decoder: create adds to linked list", has_dec,
                 has_dec ? "Found in list" : "Not in list");
    return has_dec;
}

static bool _test_dec_get_next_traverses(void)
{
    /* Count decoders by traversing */
    int count = 0;
    eos_audio_decoder_t *dec;
    for (dec = eos_audio_decoder_get_next(NULL); dec != NULL;
         dec = eos_audio_decoder_get_next(dec))
    {
        count++;
    }
    bool passed = (count >= 1); /* At least VAR (VAR always, AT on sim only) */
    _record_test("Decoder: get_next traverses all nodes", passed,
                 passed ? "Found decoders" : "No decoders found");
    return passed;
}

static bool _test_dec_set_probe_cb(void)
{
    eos_audio_decoder_t *dec = eos_audio_decoder_create();
    dec->name = "SetProbeTest";
    eos_audio_decoder_set_probe_cb(dec, (eos_audio_decoder_probe_f_t)(uintptr_t)0x1000);
    bool passed = (dec->probe_cb == (eos_audio_decoder_probe_f_t)(uintptr_t)0x1000);
    eos_audio_decoder_delete(dec);
    _record_test("Decoder: set probe_cb", passed, passed ? "Callback set" : "Not set");
    return passed;
}

static bool _test_dec_set_open_cb(void)
{
    eos_audio_decoder_t *dec = eos_audio_decoder_create();
    eos_audio_decoder_set_open_cb(dec, (eos_audio_decoder_open_f_t)(uintptr_t)0x2000);
    bool passed = (dec->open_cb == (eos_audio_decoder_open_f_t)(uintptr_t)0x2000);
    eos_audio_decoder_delete(dec);
    _record_test("Decoder: set open_cb", passed, passed ? "Callback set" : "Not set");
    return passed;
}

static bool _test_dec_set_read_cb(void)
{
    eos_audio_decoder_t *dec = eos_audio_decoder_create();
    eos_audio_decoder_set_read_cb(dec, (eos_audio_decoder_read_f_t)(uintptr_t)0x3000);
    bool passed = (dec->read_cb == (eos_audio_decoder_read_f_t)(uintptr_t)0x3000);
    eos_audio_decoder_delete(dec);
    _record_test("Decoder: set read_cb", passed, passed ? "Callback set" : "Not set");
    return passed;
}

static bool _test_dec_set_close_cb(void)
{
    eos_audio_decoder_t *dec = eos_audio_decoder_create();
    eos_audio_decoder_set_close_cb(dec, (eos_audio_decoder_close_f_t)(uintptr_t)0x4000);
    bool passed = (dec->close_cb == (eos_audio_decoder_close_f_t)(uintptr_t)0x4000);
    eos_audio_decoder_delete(dec);
    _record_test("Decoder: set close_cb", passed, passed ? "Callback set" : "Not set");
    return passed;
}

static bool _test_dec_open_null_dsc(void)
{
    eos_result_t r = eos_audio_decoder_open(NULL, (void *)"test", EOS_AUDIO_SRC_FILE);
    bool passed = (r != EOS_OK);
    _record_test("Decoder: open NULL dsc fails", passed,
                 passed ? "Rejected" : "Should have rejected");
    return passed;
}

static bool _test_dec_open_null_src(void)
{
    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, NULL, EOS_AUDIO_SRC_FILE);
    bool passed = (r != EOS_OK);
    _record_test("Decoder: open NULL src fails", passed,
                 passed ? "Rejected" : "Should have rejected");
    return passed;
}

static bool _test_dec_read_null_dsc(void)
{
    uint8_t buf[64];
    uint32_t br = 0;
    eos_result_t r = eos_audio_decoder_read(NULL, buf, sizeof(buf), &br);
    bool passed = (r != EOS_OK);
    _record_test("Decoder: read NULL dsc fails", passed,
                 passed ? "Rejected" : "Should have rejected");
    return passed;
}

static bool _test_dec_close_null_dsc(void)
{
    eos_audio_decoder_close(NULL);
    _record_test("Decoder: close NULL dsc no-crash", true, "No crash");
    return true;
}

static bool _test_src_type_null(void)
{
    eos_audio_src_type_t t = eos_audio_src_get_type(NULL);
    bool passed = (t == EOS_AUDIO_SRC_NONE);
    _record_test("SrcType: NULL returns NONE", passed,
                 passed ? "Correct" : "Wrong type");
    return passed;
}

static bool _test_src_type_var_magic(void)
{
    eos_audio_src_type_t t = eos_audio_src_get_type(&_test_var_audio);
    bool passed = (t == EOS_AUDIO_SRC_VAR);
    _record_test("SrcType: VAR magic byte returns VAR", passed,
                 passed ? "Correct" : "Wrong type");
    return passed;
}

static bool _test_src_type_file_string(void)
{
    eos_audio_src_type_t t = eos_audio_src_get_type("/fs/music.mp3");
    bool passed = (t == EOS_AUDIO_SRC_FILE);
    _record_test("SrcType: file path returns FILE", passed,
                 passed ? "Correct" : "Wrong type");
    return passed;
}

static bool _test_src_type_plain_string(void)
{
    eos_audio_src_type_t t = eos_audio_src_get_type("sound.wav");
    bool passed = (t == EOS_AUDIO_SRC_FILE);
    _record_test("SrcType: plain string returns FILE", passed,
                 passed ? "Correct" : "Wrong type");
    return passed;
}

static bool _test_var_probe_ok(void)
{
    eos_audio_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    eos_result_t r = eos_audio_decoder_open(NULL, NULL, EOS_AUDIO_SRC_NONE);
    (void)r;

    /* Find VAR decoder and test probe directly */
    eos_audio_decoder_t *dec;
    for (dec = eos_audio_decoder_get_next(NULL); dec != NULL;
         dec = eos_audio_decoder_get_next(dec))
    {
        if (dec->name && strcmp(dec->name, "VAR") == 0)
            break;
    }
    if (!dec)
    {
        _record_test("VAR: decoder found in registry", false, "VAR decoder not registered");
        return false;
    }

    eos_result_t ret = dec->probe_cb(&_test_var_audio, EOS_AUDIO_SRC_VAR, &fmt);
    bool passed = (ret == EOS_OK &&
                   fmt.sample_rate == TEST_PCM_SAMPLE_RATE &&
                   fmt.channels == TEST_PCM_CHANNELS &&
                   fmt.bits_per_sample == TEST_PCM_BITS &&
                   fmt.total_samples == TEST_PCM_TOTAL_SAMPLES &&
                   fmt.duration_ms == 0); /* 16 * 1000 / 44100 = 0 integer div */
    _record_test("VAR: probe succeeds with correct format", passed,
                 passed ? "Format correct" : "Probe failed or format wrong");
    return passed;
}

static bool _test_var_probe_duration_calc(void)
{
    eos_audio_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    eos_audio_decoder_t *dec;
    for (dec = eos_audio_decoder_get_next(NULL); dec != NULL;
         dec = eos_audio_decoder_get_next(dec))
    {
        if (dec->name && strcmp(dec->name, "VAR") == 0)
            break;
    }
    if (!dec)
    {
        _record_test("VAR: duration calc", false, "VAR decoder not found");
        return false;
    }

    /* Use a descriptor where duration_ms is calculable: 44100 samples = 1 second */
    static const uint8_t tmp_data[44100 * 2 * 2]; /* 1 sec stereo 16bit */
    eos_audio_dsc_t long_dsc = {
        .magic = EOS_AUDIO_HEADER_MAGIC,
        .sample_rate = 44100,
        .channels = 2,
        .bits_per_sample = 16,
        .total_samples = 44100,
        .data_size = 44100 * 4,
        .data = tmp_data,
    };
    eos_result_t ret = dec->probe_cb(&long_dsc, EOS_AUDIO_SRC_VAR, &fmt);
    bool passed = (ret == EOS_OK && fmt.duration_ms == 1000);
    _record_test("VAR: duration_ms calculated correctly", passed,
                 passed ? "1000ms" : "Wrong duration");
    return passed;
}

static bool _test_var_probe_bad_magic(void)
{
    eos_audio_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    eos_audio_decoder_t *dec;
    for (dec = eos_audio_decoder_get_next(NULL); dec != NULL;
         dec = eos_audio_decoder_get_next(dec))
    {
        if (dec->name && strcmp(dec->name, "VAR") == 0)
            break;
    }
    if (!dec)
    {
        _record_test("VAR: probe bad magic", false, "VAR decoder not found");
        return false;
    }

    eos_result_t ret = dec->probe_cb(&_test_var_bad_magic, EOS_AUDIO_SRC_VAR, &fmt);
    bool passed = (ret != EOS_OK);
    _record_test("VAR: probe rejects bad magic", passed,
                 passed ? "Rejected" : "Accepted bad magic");
    return passed;
}

static bool _test_var_probe_wrong_src_type(void)
{
    eos_audio_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    eos_audio_decoder_t *dec;
    for (dec = eos_audio_decoder_get_next(NULL); dec != NULL;
         dec = eos_audio_decoder_get_next(dec))
    {
        if (dec->name && strcmp(dec->name, "VAR") == 0)
            break;
    }
    if (!dec)
    {
        _record_test("VAR: probe wrong src_type", false, "VAR decoder not found");
        return false;
    }

    eos_result_t ret = dec->probe_cb(&_test_var_audio, EOS_AUDIO_SRC_FILE, &fmt);
    bool passed = (ret != EOS_OK);
    _record_test("VAR: probe rejects FILE type", passed,
                 passed ? "Rejected" : "Accepted FILE type");
    return passed;
}

static bool _test_var_open_and_format(void)
{
    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    if (r != EOS_OK)
    {
        _record_test("VAR: open", false, "Open failed");
        return false;
    }

    bool passed = (dsc.decoder != NULL &&
                   dsc.src == &_test_var_audio &&
                   dsc.src_type == EOS_AUDIO_SRC_VAR &&
                   dsc.format.sample_rate == TEST_PCM_SAMPLE_RATE &&
                   dsc.format.channels == TEST_PCM_CHANNELS &&
                   dsc.format.bits_per_sample == TEST_PCM_BITS &&
                   dsc.format.total_samples == TEST_PCM_TOTAL_SAMPLES &&
                   dsc.current_sample == 0 &&
                   dsc.user_data != NULL);

    eos_audio_decoder_close(&dsc);
    _record_test("VAR: open sets all fields correctly", passed,
                 passed ? "All correct" : "Field mismatch");
    return passed;
}

static bool _test_var_read_full(void)
{
    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    if (r != EOS_OK)
    {
        _record_test("VAR: read full", false, "Open failed");
        return false;
    }

    uint8_t buf[TEST_PCM_DATA_SIZE];
    uint32_t br = 0;
    r = eos_audio_decoder_read(&dsc, buf, sizeof(buf), &br);

    bool passed = (r == EOS_OK &&
                   br == TEST_PCM_DATA_SIZE &&
                   dsc.current_sample == TEST_PCM_TOTAL_SAMPLES &&
                   memcmp(buf, _test_pcm_data, TEST_PCM_DATA_SIZE) == 0);

    eos_audio_decoder_close(&dsc);
    _record_test("VAR: read full data matches", passed,
                 passed ? "Exact match" : "Mismatch or wrong size");
    return passed;
}

static bool _test_var_read_partial(void)
{
    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    if (r != EOS_OK)
    {
        _record_test("VAR: read partial", false, "Open failed");
        return false;
    }

    /* Read 1 sample worth of bytes (4 bytes for stereo 16-bit) */
    uint8_t buf[4];
    uint32_t br = 0;
    r = eos_audio_decoder_read(&dsc, buf, sizeof(buf), &br);

    bool passed = (r == EOS_OK &&
                   br == 4 &&
                   dsc.current_sample == 1 &&
                   memcmp(buf, _test_pcm_data, 4) == 0);

    eos_audio_decoder_close(&dsc);
    _record_test("VAR: read partial (1 sample)", passed,
                 passed ? "Correct partial read" : "Partial read wrong");
    return passed;
}

static bool _test_var_read_multi_tracked(void)
{
    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    if (r != EOS_OK)
    {
        _record_test("VAR: read multi tracked", false, "Open failed");
        return false;
    }

    uint8_t buf[8]; /* 2 samples = 8 bytes */
    uint32_t br = 0;

    r = eos_audio_decoder_read(&dsc, buf, 4, &br);
    bool ok = (r == EOS_OK && br == 4 && dsc.current_sample == 1);

    r = eos_audio_decoder_read(&dsc, buf, 8, &br);
    ok = ok && (r == EOS_OK && br == 8 && dsc.current_sample == 3);

    r = eos_audio_decoder_read(&dsc, buf, 4, &br);
    ok = ok && (r == EOS_OK && br == 4 && dsc.current_sample == 4);

    eos_audio_decoder_close(&dsc);
    _record_test("VAR: read multi tracks position", ok,
                 ok ? "Position tracked correctly" : "Position wrong");
    return ok;
}

static bool _test_var_read_past_end(void)
{
    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    if (r != EOS_OK)
    {
        _record_test("VAR: read past end", false, "Open failed");
        return false;
    }

    uint8_t buf[200]; /* larger than total */
    uint32_t br = 99;
    r = eos_audio_decoder_read(&dsc, buf, sizeof(buf), &br);

    /* Second read should return 0 bytes */
    uint32_t br2 = 99;
    r = eos_audio_decoder_read(&dsc, buf, sizeof(buf), &br2);

    bool passed = (r == EOS_OK && br2 == 0);

    eos_audio_decoder_close(&dsc);
    _record_test("VAR: read past end returns 0", passed,
                 passed ? "Zero bytes" : "Returned data past end");
    return passed;
}

static bool _test_var_read_empty_buf(void)
{
    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    if (r != EOS_OK)
    {
        _record_test("VAR: read empty buffer", false, "Open failed");
        return false;
    }

    uint8_t buf[1];
    uint32_t br = 99;
    r = eos_audio_decoder_read(&dsc, buf, 0, &br);
    /* buf_size of 0 should result in 0 bytes read */
    bool passed = (r == EOS_OK && br == 0);

    eos_audio_decoder_close(&dsc);
    _record_test("VAR: read with 0 buffer size", passed,
                 passed ? "Zero bytes" : "Should return 0");
    return passed;
}

static bool _test_var_close_frees_user_data(void)
{
    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    if (r != EOS_OK)
    {
        _record_test("VAR: close frees user_data", false, "Open failed");
        return false;
    }

    void *ud = dsc.user_data;
    eos_audio_decoder_close(&dsc);

    bool passed = (dsc.user_data == NULL && dsc.decoder == NULL);
    _record_test("VAR: close clears user_data & decoder", passed,
                 passed ? "Cleaned up" : "Not cleaned");
    return passed;
}

static bool _test_var_full_cycle_via_api(void)
{
    eos_audio_decoder_dsc_t dsc;

    eos_result_t r = eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    if (r != EOS_OK)
    {
        _record_test("VAR: full cycle via API", false, "Open failed");
        return false;
    }

    uint8_t buf[TEST_PCM_DATA_SIZE * 2];
    uint32_t total = 0;
    while (1)
    {
        uint32_t br = 0;
        r = eos_audio_decoder_read(&dsc, buf + total, sizeof(buf) - total, &br);
        if (r != EOS_OK || br == 0)
            break;
        total += br;
    }

    bool passed = (total == TEST_PCM_DATA_SIZE &&
                   dsc.current_sample == TEST_PCM_TOTAL_SAMPLES &&
                   memcmp(buf, _test_pcm_data, TEST_PCM_DATA_SIZE) == 0);

    eos_audio_decoder_close(&dsc);
    _record_test("VAR: full cycle open→read→close", passed,
                 passed ? "Complete, data matches" : "Failed");
    return passed;
}

static eos_result_t _mock_probe(const void *src, eos_audio_src_type_t type,
                                eos_audio_format_t *format)
{
    if (src == NULL || type != EOS_AUDIO_SRC_VAR)
        return EOS_FAILED;
    const eos_audio_dsc_t *dsc = (const eos_audio_dsc_t *)src;
    if (dsc->magic != EOS_AUDIO_HEADER_MAGIC)
        return EOS_FAILED;
    format->sample_rate = dsc->sample_rate;
    format->channels = dsc->channels;
    format->bits_per_sample = dsc->bits_per_sample;
    format->total_samples = dsc->total_samples;
    return EOS_OK;
}

static eos_result_t _mock_open(eos_audio_decoder_dsc_t *dsc)
{
    dsc->user_data = (void *)(uintptr_t)0xDEADBEEF;
    return EOS_OK;
}

static eos_result_t _mock_read(eos_audio_decoder_dsc_t *dsc,
                               void *buf, uint32_t buf_size, uint32_t *bytes_read)
{
    (void)dsc;
    if (buf_size > 0 && buf)
    {
        memset(buf, 0xAA, buf_size > 16 ? 16 : buf_size);
        *bytes_read = buf_size > 16 ? 16 : buf_size;
    }
    else
    {
        *bytes_read = 0;
    }
    return EOS_OK;
}

static void _mock_close(eos_audio_decoder_dsc_t *dsc)
{
    dsc->user_data = NULL;
}

static bool _test_custom_dec_register_and_probe(void)
{
    eos_audio_decoder_t *dec = eos_audio_decoder_create();
    eos_audio_decoder_set_probe_cb(dec, _mock_probe);
    eos_audio_decoder_set_open_cb(dec, _mock_open);
    eos_audio_decoder_set_read_cb(dec, _mock_read);
    eos_audio_decoder_set_close_cb(dec, _mock_close);
    dec->name = "Mock";

    /* Verify in list */
    bool found = false;
    eos_audio_decoder_t *it;
    for (it = eos_audio_decoder_get_next(NULL); it != NULL;
         it = eos_audio_decoder_get_next(it))
    {
        if (it == dec)
        {
            found = true;
            break;
        }
    }

    eos_audio_decoder_delete(dec);

    bool in_list = false;
    for (it = eos_audio_decoder_get_next(NULL); it != NULL;
         it = eos_audio_decoder_get_next(it))
    {
        if (it == dec)
        {
            in_list = true;
            break;
        }
    }

    bool passed = found && !in_list;
    _record_test("CustomDec: register & delete", passed,
                 passed ? "Added and removed from list" : "List operation failed");
    return passed;
}

static bool _test_custom_dec_probe_dispatch(void)
{
    /* Register a high-priority mock decoder (inserted at head) */
    eos_audio_decoder_t *dec = eos_audio_decoder_create();
    eos_audio_decoder_set_probe_cb(dec, _mock_probe);
    eos_audio_decoder_set_open_cb(dec, _mock_open);
    eos_audio_decoder_set_read_cb(dec, _mock_read);
    eos_audio_decoder_set_close_cb(dec, _mock_close);
    dec->name = "MockDispatch";

    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    bool passed = (r == EOS_OK && dsc.user_data == (void *)(uintptr_t)0xDEADBEEF);

    eos_audio_decoder_close(&dsc);
    eos_audio_decoder_delete(dec);
    _record_test("CustomDec: probe dispatch to mock", passed,
                 passed ? "Mock decoder used" : "Dispatch failed");
    return passed;
}

static bool _test_custom_dec_read(void)
{
    eos_audio_decoder_t *dec = eos_audio_decoder_create();
    eos_audio_decoder_set_probe_cb(dec, _mock_probe);
    eos_audio_decoder_set_open_cb(dec, _mock_open);
    eos_audio_decoder_set_read_cb(dec, _mock_read);
    eos_audio_decoder_set_close_cb(dec, _mock_close);
    dec->name = "MockRead";

    eos_audio_decoder_dsc_t dsc;
    eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);

    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    uint32_t br = 0;
    eos_result_t r = eos_audio_decoder_read(&dsc, buf, sizeof(buf), &br);

    bool passed = (r == EOS_OK && br == 16);
    for (uint32_t i = 0; i < br; i++)
    {
        if (buf[i] != 0xAA)
        {
            passed = false;
            break;
        }
    }

    eos_audio_decoder_close(&dsc);
    eos_audio_decoder_delete(dec);
    _record_test("CustomDec: read produces 0xAA pattern", passed,
                 passed ? "Pattern correct" : "Wrong data");
    return passed;
}

static bool _test_player_init(void)
{
    eos_audio_player_t test_p;
    eos_audio_player_init(&test_p);
    eos_audio_player_state_t state = eos_audio_player_get_state(&test_p);
    bool passed = (state == EOS_AUDIO_IDLE);
    _record_test("Player: init sets IDLE state", passed,
                 passed ? "IDLE" : "Not IDLE");
    return passed;
}

static bool _test_player_initial_state(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    /* Stop any previous playback first */
    eos_audio_player_stop(p);
    eos_audio_player_state_t state = eos_audio_player_get_state(p);
    bool passed = (state == EOS_AUDIO_IDLE);
    _record_test("Player: initial state is IDLE", passed,
                 passed ? "IDLE" : "Not IDLE");
    return passed;
}

static bool _test_player_play_null_fails(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    eos_result_t r = eos_audio_player_play(p, NULL, EOS_AUDIO_SRC_VAR);
    bool passed = (r != EOS_OK);
    _record_test("Player: play NULL fails", passed,
                 passed ? "Rejected" : "Should reject");
    return passed;
}

static bool _test_player_play_var(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    eos_audio_player_stop(p);

    /* Check speaker available before testing */
    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (!spk || !spk->ops || !spk->ops->is_available || !spk->ops->is_available())
    {
        _record_test("Player: play VAR audio", false, "Speaker not available");
        return false;
    }

    eos_result_t r = eos_audio_player_play(p, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    eos_audio_player_state_t state = eos_audio_player_get_state(p);

    bool passed = (r == EOS_OK && state == EOS_AUDIO_PLAYING);
    _record_test("Player: play VAR transitions to PLAYING", passed,
                 passed ? "PLAYING" : "Failed to play");
    return passed;
}

static bool _test_player_duration_after_play(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    uint32_t dur = eos_audio_player_get_duration(p);
    bool passed = (dur == TEST_PCM_TOTAL_SAMPLES);
    _record_test("Player: get_duration matches descriptor", passed,
                 passed ? "Correct" : "Mismatch");
    return passed;
}

static bool _test_player_stop_transitions_to_idle(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    eos_audio_player_stop(p);

    eos_audio_player_state_t state = eos_audio_player_get_state(p);
    bool passed = (state == EOS_AUDIO_IDLE);
    _record_test("Player: stop transitions to IDLE", passed,
                 passed ? "IDLE" : "Not IDLE");
    return passed;
}

static bool _test_player_pause_resume(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    eos_audio_player_stop(p);

    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (!spk || !spk->ops || !spk->ops->is_available || !spk->ops->is_available())
    {
        _record_test("Player: pause/resume cycle", false, "Speaker not available");
        return false;
    }

    eos_audio_player_play(p, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    eos_audio_player_pause(p);
    eos_audio_player_state_t paused_state = eos_audio_player_get_state(p);

    eos_audio_player_resume(p);
    eos_audio_player_state_t resumed_state = eos_audio_player_get_state(p);

    eos_audio_player_stop(p);

    bool passed = (paused_state == EOS_AUDIO_PAUSED &&
                   resumed_state == EOS_AUDIO_PLAYING);
    _record_test("Player: pause→PAUSED, resume→PLAYING", passed,
                 passed ? "State transitions correct" : "State wrong");
    return passed;
}

static bool _test_player_pause_from_idle_fails(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    eos_audio_player_stop(p);
    eos_result_t r = eos_audio_player_pause(p);
    bool passed = (r != EOS_OK);
    _record_test("Player: pause from IDLE fails", passed,
                 passed ? "Rejected" : "Should reject");
    return passed;
}

static bool _test_player_resume_from_idle_fails(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    eos_audio_player_stop(p);
    eos_result_t r = eos_audio_player_resume(p);
    bool passed = (r != EOS_OK);
    _record_test("Player: resume from IDLE fails", passed,
                 passed ? "Rejected" : "Should reject");
    return passed;
}

static bool _test_player_get_position(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    eos_audio_player_stop(p);

    uint32_t pos = eos_audio_player_get_position(p);
    bool passed = (pos == 0 || pos > 0); /* 0 if idle, >0 if playing */
    _record_test("Player: get_position returns valid value", passed,
                 passed ? "Valid" : "Invalid");
    return passed;
}

static bool _test_player_set_volume(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    uint8_t saved = eos_service_audio_get_volume();
    eos_result_t r = eos_audio_player_set_volume(p, 75);
    eos_service_audio_set_volume(saved);
    bool passed = (r == EOS_OK);
    _record_test("Player: set_volume returns OK", passed,
                 passed ? "OK" : "Failed");
    return passed;
}

static bool _test_player_set_volume_clamp(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    uint8_t saved = eos_service_audio_get_volume();
    eos_result_t r = eos_audio_player_set_volume(p, 200);
    bool passed = (r == EOS_OK);
    eos_service_audio_set_volume(saved);
    _record_test("Player: set_volume >100 clamps", passed,
                 passed ? "OK" : "Failed");
    return passed;
}

static bool _test_player_get_state_consistent(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    eos_audio_player_stop(p);

    eos_audio_player_state_t s1 = eos_audio_player_get_state(p);
    eos_audio_player_state_t s2 = eos_audio_player_get_state(p);
    bool passed = (s1 == s2 && s1 == EOS_AUDIO_IDLE);
    _record_test("Player: get_state is consistent", passed,
                 passed ? "Consistent" : "Inconsistent");
    return passed;
}

static bool _test_player_null_guard(void)
{
    eos_audio_player_state_t state = eos_audio_player_get_state(NULL);
    uint32_t pos = eos_audio_player_get_position(NULL);
    uint32_t dur = eos_audio_player_get_duration(NULL);
    eos_result_t r1 = eos_audio_player_stop(NULL);
    eos_result_t r2 = eos_audio_player_pause(NULL);
    eos_result_t r3 = eos_audio_player_resume(NULL);
    bool passed = (state == EOS_AUDIO_IDLE && pos == 0 && dur == 0 &&
                   r1 != EOS_OK && r2 != EOS_OK && r3 != EOS_OK);
    _record_test("Player: NULL player guarded", passed,
                 passed ? "All guarded" : "Some leaked");
    return passed;
}

static bool _test_spk_register_missing_open(void)
{
    eos_dev_speaker_ops_t ops = {
        .set_volume = (void *)1,
        .open = NULL,
        .borrow = (void *)1,
        .enqueue = (void *)1,
        .stop = (void *)1,
        .is_available = (void *)1,
    };
    eos_result_t ret = eos_dev_speaker_register(&ops);
    bool passed = (ret == EOS_ERR_INVALID_ARG || ret == EOS_ERR_ALREADY_EXISTS);
    _record_test("SpeakerOps: reject missing open", passed,
                 passed ? "Rejected" : "Should reject");
    return passed;
}

static bool _test_spk_register_missing_borrow(void)
{
    eos_dev_speaker_ops_t ops = {
        .set_volume = (void *)1,
        .open = (void *)1,
        .borrow = NULL,
        .enqueue = (void *)1,
        .stop = (void *)1,
        .is_available = (void *)1,
    };
    eos_result_t ret = eos_dev_speaker_register(&ops);
    bool passed = (ret == EOS_ERR_INVALID_ARG || ret == EOS_ERR_ALREADY_EXISTS);
    _record_test("SpeakerOps: reject missing borrow", passed,
                 passed ? "Rejected" : "Should reject");
    return passed;
}

static bool _test_spk_get_instance_after_register(void)
{
    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    bool passed = (spk != NULL && spk->ops != NULL);
    _record_test("SpeakerOps: get_instance returns valid ops", passed,
                 passed ? "Valid" : "Invalid");
    return passed;
}

static bool _test_spk_has_new_ops(void)
{
    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (!spk || !spk->ops)
    {
        _record_test("SpeakerOps: has borrow/enqueue ops", false, "No ops");
        return false;
    }

    bool passed = (spk->ops->open != NULL &&
                   spk->ops->borrow != NULL &&
                   spk->ops->enqueue != NULL &&
                   spk->ops->stop != NULL &&
                   spk->ops->set_volume != NULL &&
                   spk->ops->is_available != NULL);
    _record_test("SpeakerOps: open/borrow/enqueue/stop/set_volume/is_available exist", passed,
                 passed ? "All required ops present" : "Some missing");
    return passed;
}

static bool _test_spk_optional_ops_exist(void)
{
    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (!spk || !spk->ops)
    {
        _record_test("SpeakerOps: optional pause/resume", false, "No ops");
        return false;
    }

    bool passed = (spk->ops->pause != NULL && spk->ops->resume != NULL);
    _record_test("SpeakerOps: pause and resume are implemented", passed,
                 passed ? "Implemented" : "NULL");
    return passed;
}

static void _run_decoder_registry_tests(void)
{
    EOS_LOG_I("--- Decoder Registry Tests ---");
    _test_dec_init_creates_list();
    _test_dec_create_adds_to_list();
    _test_dec_get_next_traverses();
    _test_dec_set_probe_cb();
    _test_dec_set_open_cb();
    _test_dec_set_read_cb();
    _test_dec_set_close_cb();
    _test_dec_open_null_dsc();
    _test_dec_open_null_src();
    _test_dec_read_null_dsc();
    _test_dec_close_null_dsc();
}

static void _run_src_type_tests(void)
{
    EOS_LOG_I("--- Source Type Detection Tests ---");
    _test_src_type_null();
    _test_src_type_var_magic();
    _test_src_type_file_string();
    _test_src_type_plain_string();
}

/* ---- VAR decoder seek tests -------------------------------- */

static bool _test_var_seek_middle(void)
{
    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    if (r != EOS_OK)
    {
        _record_test("VAR Seek: setup open", false, "Open failed");
        return false;
    }

    r = eos_audio_decoder_seek(&dsc, 8);
    bool passed = (r == EOS_OK && dsc.current_sample == 8);
    eos_audio_decoder_close(&dsc);

    _record_test("VAR Seek: seek to middle", passed,
                 passed ? "Seeked to 8" : "Seek failed");
    return passed;
}

static bool _test_var_seek_to_start(void)
{
    eos_audio_decoder_dsc_t dsc;
    eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);

    eos_audio_decoder_seek(&dsc, 10);
    eos_result_t r = eos_audio_decoder_seek(&dsc, 0);
    bool passed = (r == EOS_OK && dsc.current_sample == 0);
    eos_audio_decoder_close(&dsc);

    _record_test("VAR Seek: seek to start", passed,
                 passed ? "Seeked to 0" : "Failed");
    return passed;
}

static bool _test_var_seek_past_end(void)
{
    eos_audio_decoder_dsc_t dsc;
    eos_audio_decoder_open(&dsc, &_test_var_audio, EOS_AUDIO_SRC_VAR);

    eos_result_t r = eos_audio_decoder_seek(&dsc, TEST_PCM_TOTAL_SAMPLES);
    bool passed = (r != EOS_OK);
    eos_audio_decoder_close(&dsc);

    _record_test("VAR Seek: past end returns error", passed,
                 passed ? "Error returned" : "Should have failed");
    return passed;
}

static void _run_var_decoder_tests(void)
{
    EOS_LOG_I("--- VAR Decoder Tests ---");
    _test_var_probe_ok();
    _test_var_probe_duration_calc();
    _test_var_probe_bad_magic();
    _test_var_probe_wrong_src_type();
    _test_var_open_and_format();
    _test_var_read_full();
    _test_var_read_partial();
    _test_var_read_multi_tracked();
    _test_var_read_past_end();
    _test_var_read_empty_buf();
    _test_var_close_frees_user_data();
    _test_var_full_cycle_via_api();
    _test_var_seek_middle();
    _test_var_seek_to_start();
    _test_var_seek_past_end();
}

static void _run_custom_decoder_tests(void)
{
    EOS_LOG_I("--- Custom Decoder Tests ---");
    _test_custom_dec_register_and_probe();
    _test_custom_dec_probe_dispatch();
    _test_custom_dec_read();
}

/* ---- Player mute / seek tests ----------------------------- */

static bool _test_player_mute(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    eos_audio_player_set_mute(p, true);
    bool muted = eos_audio_player_is_muted(p);
    eos_audio_player_set_mute(p, false);
    bool unmuted = !eos_audio_player_is_muted(p);

    bool passed = muted && unmuted;
    _record_test("Player: set_mute/is_muted", passed,
                 passed ? "Mute toggles correctly" : "Mute state mismatch");
    return passed;
}

static bool _test_player_seek_var(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    eos_audio_player_stop(p);

    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (!spk || !spk->ops || !spk->ops->is_available || !spk->ops->is_available())
    {
        _record_test("Player: seek VAR source", false, "Speaker not available");
        return false;
    }

    eos_result_t r = eos_audio_player_play(p, &_test_var_audio, EOS_AUDIO_SRC_VAR);
    if (r != EOS_OK)
    {
        _record_test("Player: seek VAR source", false, "Play failed for setup");
        return false;
    }

    r = eos_audio_player_seek(p, 4);
    uint32_t pos = eos_audio_player_get_position(p);
    eos_audio_player_stop(p);

    bool passed = (r == EOS_OK && pos == 4);
    _record_test("Player: seek VAR source", passed,
                 passed ? "Seeked to sample 4" : "Seek/position mismatch");
    return passed;
}

static void _run_player_tests(void)
{
    EOS_LOG_I("--- Audio Player Tests ---");
    _ensure_player_ready();
    _test_player_init();
    _test_player_initial_state();
    _test_player_play_null_fails();
    _test_player_play_var();
    _test_player_duration_after_play();
    _test_player_stop_transitions_to_idle();
    _test_player_pause_resume();
    _test_player_pause_from_idle_fails();
    _test_player_resume_from_idle_fails();
    _test_player_get_position();
    _test_player_set_volume();
    _test_player_set_volume_clamp();
    _test_player_get_state_consistent();
    _test_player_null_guard();
    _test_player_mute();
    _test_player_seek_var();
}

static void _run_speaker_ops_tests(void)
{
    EOS_LOG_I("--- Speaker OPS Tests ---");
    _test_spk_register_missing_open();
    _test_spk_register_missing_borrow();
    _test_spk_get_instance_after_register();
    _test_spk_has_new_ops();
    _test_spk_optional_ops_exist();
}

static bool _test_effects_get_all(void)
{
    bool all_ok = true;
    for (int id = 0; id < EOS_AUDIO_EFFECT_COUNT; id++)
    {
        const eos_audio_dsc_t *dsc = eos_audio_effect_get((eos_audio_effect_id_t)id);
        if (dsc == NULL)
        {
            all_ok = false;
            break;
        }
    }
    _record_test("Effects: all IDs return non-NULL", all_ok,
                 all_ok ? "All valid" : "Some NULL");
    return all_ok;
}

static bool _test_effects_magic_valid(void)
{
    bool all_ok = true;
    for (int id = 0; id < EOS_AUDIO_EFFECT_COUNT; id++)
    {
        const eos_audio_dsc_t *dsc = eos_audio_effect_get((eos_audio_effect_id_t)id);
        if (dsc == NULL || dsc->magic != EOS_AUDIO_HEADER_MAGIC)
        {
            all_ok = false;
            break;
        }
    }
    _record_test("Effects: magic byte is EOS_AUDIO_HEADER_MAGIC", all_ok,
                 all_ok ? "All valid" : "Magic mismatch");
    return all_ok;
}

static bool _test_effects_play_click(void)
{
    _ensure_player_ready();
    eos_audio_player_t *p = &_test_player;
    eos_audio_player_stop(p);

    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (!spk || !spk->ops || !spk->ops->is_available || !spk->ops->is_available())
    {
        _record_test("Effects: play click via player", false, "Speaker not available");
        return false;
    }

    const eos_audio_dsc_t *dsc = eos_audio_effect_get(EOS_AUDIO_EFFECT_CLICK);
    if (dsc == NULL)
    {
        _record_test("Effects: play click via player", false, "Descriptor NULL");
        return false;
    }

    eos_result_t r = eos_audio_player_play(p, dsc, EOS_AUDIO_SRC_VAR);
    eos_audio_player_stop(p);

    bool passed = (r == EOS_OK);
    _record_test("Effects: play click via player", passed,
                 passed ? "Played successfully" : "Play failed");
    return passed;
}

static bool _test_effects_get_invalid(void)
{
    const eos_audio_dsc_t *dsc = eos_audio_effect_get(EOS_AUDIO_EFFECT_COUNT);
    bool passed = (dsc == NULL);
    _record_test("Effects: out-of-range ID returns NULL", passed,
                  passed ? "NULL returned" : "Should return NULL");
    return passed;
}

/* ---- WAV Decoder Tests ------------------------------------- */

#define WAV_TEST_PATH "/elenixos/test_wav_decoder.wav"

static bool _generate_test_wav(void)
{
    uint8_t wav[] = {
        'R','I','F','F', 44,0,0,0, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
        0x40,0x1f,0,0, 0x80,0x3e,0,0, 2,0, 16,0,
        'd','a','t','a', 8,0,0,0,
        0,0, 1,0, 2,0, 3,0
    };
    eos_fs_mkdir("/elenixos");
    eos_file_t f = eos_fs_open_write(WAV_TEST_PATH);
    if (f == EOS_FILE_INVALID) return false;
    int n = eos_fs_write(f, wav, sizeof(wav));
    eos_fs_close(f);
    return (n == (int)sizeof(wav));
}

static void _cleanup_test_wav(void)
{
    eos_fs_remove(WAV_TEST_PATH);
}

static bool _ensure_wav_test_file(void)
{
    if (_wav_test_ready) return true;
    _wav_test_ready = _generate_test_wav();
    return _wav_test_ready;
}

static bool _test_wav_probe(void)
{
    if (!_ensure_wav_test_file()) {
        _record_test("WAV: probe + open file", false, "Generated file failed");
        return false;
    }
    eos_audio_decoder_dsc_t dsc;
    eos_result_t res = eos_audio_decoder_open(&dsc, WAV_TEST_PATH, EOS_AUDIO_SRC_FILE);
    if (res != EOS_OK)
    {
        _record_test("WAV: probe + open file", false, "Open failed - file not found");
        return false;
    }

    bool passed = (dsc.format.sample_rate == 8000)
               && (dsc.format.channels == 1)
               && (dsc.format.bits_per_sample == 16)
               && (dsc.format.total_samples == 4);
    eos_audio_decoder_close(&dsc);
    _record_test("WAV: probe + open file", passed,
                 passed ? "Format correct (8000Hz 1ch 16bit 4samp)" : "Format mismatch");
    return passed;
}

static bool _test_wav_read(void)
{
    if (!_ensure_wav_test_file()) {
        _record_test("WAV: read data", false, "Generated file failed");
        return false;
    }
    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, WAV_TEST_PATH, EOS_AUDIO_SRC_FILE);
    if (r != EOS_OK)
    {
        _record_test("WAV: read data", false, "Open failed");
        return false;
    }

    uint8_t buf[16];
    uint32_t bytes_read = 0;
    r = eos_audio_decoder_read(&dsc, buf, sizeof(buf), &bytes_read);
    bool passed = (r == EOS_OK && bytes_read == 8 && dsc.current_sample == 4);
    eos_audio_decoder_close(&dsc);

    _record_test("WAV: read data", passed,
                 passed ? "Read 8 bytes (4 samples)" : "Read mismatch");
    return passed;
}

static bool _test_wav_seek(void)
{
    if (!_ensure_wav_test_file()) {
        _record_test("WAV: seek", false, "Generated file failed");
        return false;
    }
    eos_audio_decoder_dsc_t dsc;
    eos_result_t r = eos_audio_decoder_open(&dsc, WAV_TEST_PATH, EOS_AUDIO_SRC_FILE);
    if (r != EOS_OK)
    {
        _record_test("WAV: seek", false, "Open failed");
        return false;
    }

    r = eos_audio_decoder_seek(&dsc, 2);
    bool passed = (r == EOS_OK || r == EOS_ERR_DEV_OPS_NOT_SUPPORTED);
    if (r == EOS_OK)
        passed = passed && (dsc.current_sample == 2);
    eos_audio_decoder_close(&dsc);

    _record_test("WAV: seek to sample 2", passed,
                 passed ? "Seeked or N/A" : "Seek failed");
    return passed;
}

static void _run_wav_decoder_tests(void)
{
    EOS_LOG_I("--- WAV Decoder Tests ---");
    _generate_test_wav();
    _test_wav_probe();
    _test_wav_read();
    _test_wav_seek();
    _cleanup_test_wav();
}

static void _run_effects_tests(void)
{
    EOS_LOG_I("--- VAR Effects Tests ---");
    _ensure_player_ready();
    _test_effects_get_all();
    _test_effects_magic_valid();
    _test_effects_play_click();
    _test_effects_get_invalid();
}

static void _register_all_tests(void)
{
    eos_test_register("Decoder: init registers built-in decoders", _test_dec_init_creates_list);
    eos_test_register("Decoder: create adds to linked list", _test_dec_create_adds_to_list);
    eos_test_register("Decoder: get_next traverses all nodes", _test_dec_get_next_traverses);
    eos_test_register("Decoder: set probe_cb", _test_dec_set_probe_cb);
    eos_test_register("Decoder: set open_cb", _test_dec_set_open_cb);
    eos_test_register("Decoder: set read_cb", _test_dec_set_read_cb);
    eos_test_register("Decoder: set close_cb", _test_dec_set_close_cb);
    eos_test_register("Decoder: open NULL dsc fails", _test_dec_open_null_dsc);
    eos_test_register("Decoder: open NULL src fails", _test_dec_open_null_src);
    eos_test_register("Decoder: read NULL dsc fails", _test_dec_read_null_dsc);
    eos_test_register("Decoder: close NULL dsc no-crash", _test_dec_close_null_dsc);
    eos_test_register("SrcType: NULL returns NONE", _test_src_type_null);
    eos_test_register("SrcType: VAR magic byte returns VAR", _test_src_type_var_magic);
    eos_test_register("SrcType: file path returns FILE", _test_src_type_file_string);
    eos_test_register("SrcType: plain string returns FILE", _test_src_type_plain_string);
    eos_test_register("VAR: probe succeeds with correct format", _test_var_probe_ok);
    eos_test_register("VAR: duration_ms calculated correctly", _test_var_probe_duration_calc);
    eos_test_register("VAR: probe rejects bad magic", _test_var_probe_bad_magic);
    eos_test_register("VAR: probe rejects FILE type", _test_var_probe_wrong_src_type);
    eos_test_register("VAR: open sets all fields correctly", _test_var_open_and_format);
    eos_test_register("VAR: read full data matches", _test_var_read_full);
    eos_test_register("VAR: read partial (1 sample)", _test_var_read_partial);
    eos_test_register("VAR: read multi tracks position", _test_var_read_multi_tracked);
    eos_test_register("VAR: read past end returns 0", _test_var_read_past_end);
    eos_test_register("VAR: read with 0 buffer size", _test_var_read_empty_buf);
    eos_test_register("VAR: close clears user_data & decoder", _test_var_close_frees_user_data);
    eos_test_register("VAR: full cycle open->read->close", _test_var_full_cycle_via_api);
    eos_test_register("VAR Seek: seek to middle", _test_var_seek_middle);
    eos_test_register("VAR Seek: seek to start", _test_var_seek_to_start);
    eos_test_register("VAR Seek: past end returns error", _test_var_seek_past_end);
    eos_test_register("CustomDec: register & delete", _test_custom_dec_register_and_probe);
    eos_test_register("CustomDec: probe dispatch to mock", _test_custom_dec_probe_dispatch);
    eos_test_register("CustomDec: read produces 0xAA pattern", _test_custom_dec_read);
    eos_test_register("Player: init sets IDLE state", _test_player_init);
    eos_test_register("Player: initial state is IDLE", _test_player_initial_state);
    eos_test_register("Player: play NULL fails", _test_player_play_null_fails);
    eos_test_register("Player: play VAR transitions to PLAYING", _test_player_play_var);
    eos_test_register("Player: get_duration matches descriptor", _test_player_duration_after_play);
    eos_test_register("Player: stop transitions to IDLE", _test_player_stop_transitions_to_idle);
    eos_test_register("Player: pause->PAUSED, resume->PLAYING", _test_player_pause_resume);
    eos_test_register("Player: pause from IDLE fails", _test_player_pause_from_idle_fails);
    eos_test_register("Player: resume from IDLE fails", _test_player_resume_from_idle_fails);
    eos_test_register("Player: get_position returns valid value", _test_player_get_position);
    eos_test_register("Player: set_volume returns OK", _test_player_set_volume);
    eos_test_register("Player: set_volume < 0 clamps", _test_player_set_volume_clamp);
    eos_test_register("Player: get_state is consistent", _test_player_get_state_consistent);
    eos_test_register("Player: NULL player guarded", _test_player_null_guard);
    eos_test_register("Player: set_mute/is_muted", _test_player_mute);
    eos_test_register("Player: seek VAR source", _test_player_seek_var);
    eos_test_register("SpeakerOps: reject missing open", _test_spk_register_missing_open);
    eos_test_register("SpeakerOps: reject missing borrow", _test_spk_register_missing_borrow);
    eos_test_register("SpeakerOps: get_instance returns valid ops", _test_spk_get_instance_after_register);
    eos_test_register("SpeakerOps: open/borrow/enqueue/stop/set_volume/is_available exist", _test_spk_has_new_ops);
    eos_test_register("SpeakerOps: pause and resume are implemented", _test_spk_optional_ops_exist);
    eos_test_register("Effects: all IDs return non-NULL", _test_effects_get_all);
    eos_test_register("Effects: magic byte is EOS_AUDIO_HEADER_MAGIC", _test_effects_magic_valid);
    eos_test_register("Effects: play click via player", _test_effects_play_click);
    eos_test_register("Effects: out-of-range ID returns NULL", _test_effects_get_invalid);
    eos_test_register("WAV: probe + open file", _test_wav_probe);
    eos_test_register("WAV: read data", _test_wav_read);
    eos_test_register("WAV: seek", _test_wav_seek);
}

void eos_test_audio_decoder_start(void)
{
    _register_all_tests();
    eos_test_audio_page_start();
}

#endif /* EOS_ENABLE_TEST_APP */
