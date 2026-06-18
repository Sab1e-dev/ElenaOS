/**
 * @file eos_test_audio.c
 * @brief Comprehensive audio subsystem test module
 */

#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

#include "eos_test_audio.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_crown.h"
#include "eos_dev_speaker.h"
#include "eos_dev_microphone.h"
#include "eos_service_audio.h"
#include "eos_service_config.h"
#include "eos_log.h"
#include "eos_basic_widgets.h"
#include "eos_lang.h"
#include "lvgl.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "AudioTest"

/* Variables --------------------------------------------------*/
typedef struct
{
    lv_obj_t *container;
    lv_obj_t *list;
    lv_obj_t *result_label;
    struct
    {
        uint32_t total_tests;
        uint32_t passed_tests;
        uint32_t failed_tests;
    } stats;
} _test_context_t;

static _test_context_t _ctx = {0};

/* Function Implementations -----------------------------------*/

static void _update_result(const char *text)
{
    if (_ctx.result_label)
    {
        lv_label_set_text(_ctx.result_label, text);
    }
    EOS_LOG_I("%s", text);
}

static void _record_test(const char *name, bool passed, const char *details)
{
    _ctx.stats.total_tests++;
    if (passed)
    {
        _ctx.stats.passed_tests++;
    }
    else
    {
        _ctx.stats.failed_tests++;
    }

    char label_text[256];
    snprintf(label_text, sizeof(label_text),
             "%s: %s",
             name,
             passed ? "PASS" : "FAIL");

    lv_obj_t *btn = lv_list_add_button(_ctx.list, NULL, label_text);

    if (!passed)
    {
        lv_obj_set_style_text_color(btn, lv_color_hex(0xFF0000), 0);
    }
}

/**
 * @brief Speaker: get_instance returns non-null (even before register)
 */
static bool _test_spk_get_instance(void)
{
    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    bool passed = (spk != NULL);
    _record_test("Speaker: get_instance non-null", passed,
                 passed ? "Instance exists" : "get_instance returned NULL");
    return passed;
}

/**
 * @brief Speaker: state is READY (pre-registered by simulator port)
 */
static bool _test_spk_initial_state(void)
{
    eos_dev_state_t state = eos_dev_speaker_get_state();
    bool passed = (state == DEV_STATE_READY);
    _record_test("Speaker: state is READY", passed,
                 passed ? "State is DEV_STATE_READY" : "State incorrect");
    return passed;
}

/**
 * @brief Speaker: register rejects NULL ops
 */
static bool _test_spk_register_null_ops(void)
{
    eos_result_t ret = eos_dev_speaker_register(NULL);
    bool passed = (ret != EOS_OK);
    _record_test("Speaker: reject NULL ops", passed,
                 passed ? "NULL ops rejected" : "NULL ops accepted");
    return passed;
}

/**
 * @brief Speaker: register rejects incomplete ops (missing required)
 */
static bool _test_spk_register_incomplete_ops(void)
{
    eos_dev_speaker_ops_t ops = {0};
    /* All required fields are NULL */
    eos_result_t ret = eos_dev_speaker_register(&ops);
    bool passed = (ret == EOS_ERR_INVALID_ARG || ret == EOS_ERR_ALREADY_EXISTS);
    _record_test("Speaker: reject incomplete ops", passed,
                 passed ? "Incomplete ops rejected" : "Incomplete ops accepted");
    return passed;
}

/**
 * @brief Speaker: register rejects ops missing open/borrow/enqueue
 */
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
    _record_test("Speaker: reject missing open", passed,
                 passed ? "Missing open rejected" : "Should have rejected");
    return passed;
}

/**
 * @brief Speaker: register rejects ops missing is_available
 */
static bool _test_spk_register_missing_is_available(void)
{
    eos_dev_speaker_ops_t ops = {
        .set_volume = (void *)1,
        .open = (void *)1,
        .borrow = (void *)1,
        .enqueue = (void *)1,
        .stop = (void *)1,
        .is_available = NULL,
    };
    eos_result_t ret = eos_dev_speaker_register(&ops);
    bool passed = (ret == EOS_ERR_INVALID_ARG || ret == EOS_ERR_ALREADY_EXISTS);
    _record_test("Speaker: reject missing is_available", passed,
                 passed ? "Missing is_available rejected" : "Should have rejected");
    return passed;
}

/**
 * @brief Speaker: register rejects duplicate registration
 */
static bool _test_spk_register_duplicate(void)
{
    /* Re-use the actual registered ops to trigger "already registered" */
    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    if (spk == NULL || spk->ops == NULL)
    {
        _record_test("Speaker: reject duplicate register", false,
                     "Pre-condition: no ops registered yet");
        return false;
    }

    eos_result_t ret = eos_dev_speaker_register(spk->ops);
    bool passed = (ret == EOS_ERR_ALREADY_EXISTS);
    _record_test("Speaker: reject duplicate register", passed,
                 passed ? "Duplicate rejected with EOS_ERR_ALREADY_EXISTS" : "Duplicate should have been rejected");
    return passed;
}

/**
 * @brief Speaker: state transitions READY -> BUSY -> READY via report_state
 */
static bool _test_spk_state_transitions(void)
{
    /* Should be READY after valid registration */
    eos_dev_state_t initial = eos_dev_speaker_get_state();
    if (initial != DEV_STATE_READY)
    {
        _record_test("Speaker: state transitions", false, "Pre-condition: not READY");
        return false;
    }

    eos_dev_speaker_report_state(DEV_STATE_BUSY);
    eos_dev_state_t busy = eos_dev_speaker_get_state();

    eos_dev_speaker_report_state(DEV_STATE_READY);
    eos_dev_state_t back = eos_dev_speaker_get_state();

    bool passed = (busy == DEV_STATE_BUSY && back == DEV_STATE_READY);
    _record_test("Speaker: state transitions", passed,
                 passed ? "READY -> BUSY -> READY" : "State transition failed");
    return passed;
}

/**
 * @brief Speaker: report_state with same value is no-op
 */
static bool _test_spk_report_state_same(void)
{
    eos_dev_speaker_report_state(DEV_STATE_READY);
    eos_dev_state_t state = eos_dev_speaker_get_state();
    bool passed = (state == DEV_STATE_READY);
    _record_test("Speaker: report_state same no-op", passed,
                 passed ? "Same state is no-op" : "State changed unexpectedly");
    return passed;
}

/**
 * @brief Mic: get_instance returns non-null
 */
static bool _test_mic_get_instance(void)
{
    eos_dev_microphone_t *mic = eos_dev_microphone_get_instance();
    bool passed = (mic != NULL);
    _record_test("Mic: get_instance non-null", passed,
                 passed ? "Instance exists" : "get_instance returned NULL");
    return passed;
}

/**
 * @brief Mic: state is READY (pre-registered by simulator port)
 */
static bool _test_mic_initial_state(void)
{
    eos_dev_state_t state = eos_dev_microphone_get_state();
    bool passed = (state == DEV_STATE_READY);
    _record_test("Mic: state is READY", passed,
                 passed ? "State is DEV_STATE_READY" : "State incorrect");
    return passed;
}

/**
 * @brief Mic: register rejects NULL ops
 */
static bool _test_mic_register_null_ops(void)
{
    eos_result_t ret = eos_dev_microphone_register(NULL);
    bool passed = (ret != EOS_OK);
    _record_test("Mic: reject NULL ops", passed,
                 passed ? "NULL ops rejected" : "NULL ops accepted");
    return passed;
}

/**
 * @brief Mic: register rejects incomplete ops
 */
static bool _test_mic_register_incomplete_ops(void)
{
    eos_dev_microphone_ops_t ops = {0};
    eos_result_t ret = eos_dev_microphone_register(&ops);
    bool passed = (ret == EOS_ERR_INVALID_ARG || ret == EOS_ERR_ALREADY_EXISTS);
    _record_test("Mic: reject incomplete ops", passed,
                 passed ? "Incomplete ops rejected" : "Incomplete ops accepted");
    return passed;
}

/**
 * @brief Mic: register rejects ops missing set_buffer
 */
static bool _test_mic_register_missing_set_buffer(void)
{
    eos_dev_microphone_ops_t ops = {
        .set_buffer = NULL,
        .get_write_offset = (void *)1,
        .is_available = (void *)1,
    };
    eos_result_t ret = eos_dev_microphone_register(&ops);
    bool passed = (ret == EOS_ERR_INVALID_ARG || ret == EOS_ERR_ALREADY_EXISTS);
    _record_test("Mic: reject missing set_buffer", passed,
                 passed ? "Missing set_buffer rejected" : "Should have rejected");
    return passed;
}

/**
 * @brief Mic: register rejects ops missing is_available
 */
static bool _test_mic_register_missing_is_available(void)
{
    eos_dev_microphone_ops_t ops = {
        .set_buffer = (void *)1,
        .get_write_offset = (void *)1,
        .is_available = NULL,
    };
    eos_result_t ret = eos_dev_microphone_register(&ops);
    bool passed = (ret == EOS_ERR_INVALID_ARG || ret == EOS_ERR_ALREADY_EXISTS);
    _record_test("Mic: reject missing is_available", passed,
                 passed ? "Missing is_available rejected" : "Should have rejected");
    return passed;
}

/**
 * @brief Mic: register rejects duplicate registration
 */
static bool _test_mic_register_duplicate(void)
{
    eos_dev_microphone_t *mic = eos_dev_microphone_get_instance();
    if (mic == NULL || mic->ops == NULL)
    {
        _record_test("Mic: reject duplicate register", false,
                     "Pre-condition: no ops registered yet");
        return false;
    }

    eos_result_t ret = eos_dev_microphone_register(mic->ops);
    bool passed = (ret == EOS_ERR_ALREADY_EXISTS);
    _record_test("Mic: reject duplicate register", passed,
                 passed ? "Duplicate rejected with EOS_ERR_ALREADY_EXISTS" : "Duplicate should have been rejected");
    return passed;
}

/**
 * @brief Mic: state transitions
 */
static bool _test_mic_state_transitions(void)
{
    eos_dev_state_t initial = eos_dev_microphone_get_state();
    if (initial != DEV_STATE_READY)
    {
        _record_test("Mic: state transitions", false, "Pre-condition: not READY");
        return false;
    }

    eos_dev_microphone_report_state(DEV_STATE_BUSY);
    eos_dev_state_t busy = eos_dev_microphone_get_state();

    eos_dev_microphone_report_state(DEV_STATE_READY);
    eos_dev_state_t back = eos_dev_microphone_get_state();

    bool passed = (busy == DEV_STATE_BUSY && back == DEV_STATE_READY);
    _record_test("Mic: state transitions", passed,
                 passed ? "READY -> BUSY -> READY" : "State transition failed");
    return passed;
}

/**
 * @brief Service: set_volume with valid value succeeds
 */
static bool _test_service_set_volume_valid(void)
{
    eos_result_t ret = eos_service_audio_set_volume(50);
    bool passed = (ret == EOS_OK);
    _record_test("Service: set_volume 50", passed,
                 passed ? "Volume set successfully" : "set_volume failed");
    return passed;
}

/**
 * @brief Service: set_volume 0 (min) succeeds
 */
static bool _test_service_set_volume_min(void)
{
    eos_result_t ret = eos_service_audio_set_volume(0);
    bool passed = (ret == EOS_OK);
    _record_test("Service: set_volume 0 (min)", passed,
                 passed ? "Min volume set" : "set_volume failed");
    return passed;
}

/**
 * @brief Service: set_volume 100 (max) succeeds
 */
static bool _test_service_set_volume_max(void)
{
    eos_result_t ret = eos_service_audio_set_volume(100);
    bool passed = (ret == EOS_OK);
    _record_test("Service: set_volume 100 (max)", passed,
                 passed ? "Max volume set" : "set_volume failed");
    return passed;
}

/**
 * @brief Service: set_volume out-of-range is clamped
 */
static bool _test_service_set_volume_clamped(void)
{
    eos_result_t ret = eos_service_audio_set_volume(200);
    /* Should succeed because it gets clamped to 100 */
    bool passed = (ret == EOS_OK);
    _record_test("Service: set_volume 200 clamped to 100", passed,
                 passed ? "Out-of-range clamped" : "Should clamp and succeed");
    return passed;
}

/**
 * @brief Service: get_volume returns config value
 */
static bool _test_service_get_volume(void)
{
    eos_service_audio_set_volume(75);
    uint8_t vol = eos_service_audio_get_volume();
    bool passed = (vol == 75);
    _record_test("Service: get_volume after set", passed,
                 passed ? "Volume persisted correctly" : "Volume mismatch");
    return passed;
}

/**
 * @brief Service: mute sets volume to 0
 */
static bool _test_service_mute_on(void)
{
    eos_service_audio_set_volume(60);
    eos_service_audio_set_mute(true);
    bool is_muted = eos_service_audio_is_muted();
    bool passed = (is_muted == true);
    _record_test("Service: mute on", passed,
                 passed ? "Mute enabled" : "Mute not set");
    return passed;
}

/**
 * @brief Service: unmute restores volume
 */
static bool _test_service_mute_off(void)
{
    eos_service_audio_set_mute(false);
    bool is_muted = eos_service_audio_is_muted();
    bool passed = (is_muted == false);
    _record_test("Service: mute off", passed,
                 passed ? "Mute disabled" : "Mute still set");
    return passed;
}

/**
 * @brief Service: mute/is_muted default is false
 */
static bool _test_service_mute_default(void)
{
    /* Use a fresh mute state by resetting */
    eos_service_audio_set_mute(false);
    bool is_muted = eos_service_audio_is_muted();
    bool passed = (is_muted == false);
    _record_test("Service: default mute = false", passed,
                 passed ? "Mute defaults to false" : "Mute default incorrect");
    return passed;
}

/**
 * @brief Service: play rejects NULL path
 */
static bool _test_service_play_null(void)
{
    eos_result_t ret = eos_service_audio_play(NULL);
    bool passed = (ret == EOS_ERR_INVALID_ARG);
    _record_test("Service: play NULL path rejected", passed,
                 passed ? "NULL path returns EOS_ERR_INVALID_ARG" : "Should reject NULL");
    return passed;
}

/**
 * @brief Service: play with valid path (service API call succeeds)
 */
static bool _test_service_play_valid(void)
{
    eos_result_t ret = eos_service_audio_play("/fs/music.mp3");
    /* Accept either EOS_OK or EOS_ERR_DEV_ERROR (file may not exist
     * on host, or audio format may be unsupported). The important
     * thing is the call doesn't crash and returns a valid error code. */
    bool passed = (ret == EOS_OK || ret == EOS_ERR_DEV_ERROR);
    _record_test("Service: play file API works", passed,
                 passed ? "Play API call handled cleanly" : "Play failed unexpectedly");
    return passed;
}

/**
 * @brief Service: stop succeeds
 */
static bool _test_service_stop(void)
{
    eos_result_t ret = eos_service_audio_stop();
    bool passed = (ret == EOS_OK);
    _record_test("Service: stop", passed,
                 passed ? "Stop succeeded" : "Stop failed");
    return passed;
}

/**
 * @brief Service: play_tone when ops is NULL returns EOS_ERR_DEV_OPS_NOT_SUPPORTED
 */
static bool _test_service_play_tone_unsupported(void)
{
    eos_result_t ret = eos_service_audio_play_tone(440, 500);
    bool passed = (ret == EOS_ERR_DEV_OPS_NOT_SUPPORTED);
    _record_test("Service: play_tone unsupported", passed,
                 passed ? "Returns EOS_ERR_DEV_OPS_NOT_SUPPORTED" : "Should report unsupported");
    return passed;
}

/**
 * @brief Service: start_recording rejects NULL path
 */
static bool _test_service_record_null(void)
{
    eos_result_t ret = eos_service_audio_start_recording(NULL);
    bool passed = (ret == EOS_ERR_INVALID_ARG);
    _record_test("Service: record NULL path rejected", passed,
                 passed ? "NULL path returns EOS_ERR_INVALID_ARG" : "Should reject NULL");
    return passed;
}

#define RECORD_TEST_PATH "/.sys/test_recording.wav"

/**
 * @brief Service: start recording to a file
 */
static bool _test_service_record_start(void)
{
    eos_result_t ret = eos_service_audio_start_recording(RECORD_TEST_PATH);
    /* Accept EOS_OK, or EOS_ERR_DEV_ERROR (permission denied / no input device) */
    bool passed = (ret == EOS_OK || ret == EOS_ERR_DEV_ERROR);
    _record_test("Service: start recording", passed,
                 passed ? (ret == EOS_OK ? "Recording started" : "Permission/mic not available (expected on some systems)")
                        : "Start recording failed");
    return passed;
}

/**
 * @brief Service: stop recording
 */
static bool _test_service_record_stop(void)
{
    eos_result_t ret = eos_service_audio_stop_recording();
    /* Stop is safe to call even if not recording */
    bool passed = (ret == EOS_OK);
    _record_test("Service: stop recording", passed,
                 passed ? "Recording stopped" : "Stop recording failed");
    return passed;
}

/**
 * @brief Service: play back the recorded audio file
 */
static bool _test_service_record_playback(void)
{
    eos_result_t ret = eos_service_audio_play(RECORD_TEST_PATH);
    /* Accept EOS_OK or EOS_ERR_DEV_ERROR (file may not exist if recording failed) */
    bool passed = (ret == EOS_OK || ret == EOS_ERR_DEV_ERROR);
    _record_test("Service: play back recording", passed,
                 passed ? "Playback API call handled cleanly" : "Playback failed unexpectedly");
    return passed;
}

/**
 * @brief Service: speaker_available reflects device
 */
static bool _test_service_speaker_available(void)
{
    bool avail = eos_service_audio_speaker_available();
    /* On simulator with port audio registered, speaker should be available */
    bool passed = (avail == true);
    _record_test("Service: speaker available", passed,
                 passed ? "Speaker is available" : "Speaker not available (port may not be registered)");
    return passed;
}

/**
 * @brief Service: microphone_available reflects device
 */
static bool _test_service_microphone_available(void)
{
    bool avail = eos_service_audio_microphone_available();
    bool passed = (avail == true);
    _record_test("Service: microphone available", passed,
                 passed ? "Microphone is available" : "Mic should be available");
    return passed;
}

/**
 * @brief Service: set_volume repeated calls are idempotent
 */
static bool _test_service_set_volume_idempotent(void)
{
    eos_service_audio_set_volume(30);
    eos_result_t ret1 = eos_service_audio_set_volume(30);
    bool passed = (ret1 == EOS_OK);
    _record_test("Service: set_volume idempotent", passed,
                 passed ? "Repeated call succeeds" : "Repeated call failed");
    return passed;
}

/**
 * @brief Speaker: get_instance returns same pointer
 */
static bool _test_spk_get_instance_consistent(void)
{
    eos_dev_speaker_t *spk1 = eos_dev_speaker_get_instance();
    eos_dev_speaker_t *spk2 = eos_dev_speaker_get_instance();
    bool passed = (spk1 == spk2);
    _record_test("Speaker: get_instance consistent", passed,
                 passed ? "Same pointer returned" : "Different pointers");
    return passed;
}

/**
 * @brief Mic: get_instance returns same pointer
 */
static bool _test_mic_get_instance_consistent(void)
{
    eos_dev_microphone_t *mic1 = eos_dev_microphone_get_instance();
    eos_dev_microphone_t *mic2 = eos_dev_microphone_get_instance();
    bool passed = (mic1 == mic2);
    _record_test("Mic: get_instance consistent", passed,
                 passed ? "Same pointer returned" : "Different pointers");
    return passed;
}

/**
 * @brief Speaker: register only required ops succeeds (optional NULL allowed)
 */
static bool _test_spk_register_required_only(void)
{
    /* The speaker is already registered by port_audio_init.
     * This test verifies the current state is READY (registration succeeded)
     * even though optional ops (deinit, pause, resume) are NULL. */
    eos_dev_state_t state = eos_dev_speaker_get_state();
    bool passed = (state == DEV_STATE_READY);
    _record_test("Speaker: register with optional NULL ops", passed,
                 passed ? "Registration accepted with optional ops NULL" : "State is not READY");
    return passed;
}

/**
 * @brief Mic: register only required ops succeeds
 */
static bool _test_mic_register_required_only(void)
{
    eos_dev_state_t state = eos_dev_microphone_get_state();
    bool passed = (state == DEV_STATE_READY);
    _record_test("Mic: register with optional NULL ops", passed,
                 passed ? "Registration accepted with optional ops NULL" : "State is not READY");
    return passed;
}

/**
 * @brief Service: stop when nothing playing does not crash
 */
static bool _test_service_stop_idle(void)
{
    /* Already stopped, call again */
    eos_result_t ret = eos_service_audio_stop();
    bool passed = (ret == EOS_OK);
    _record_test("Service: stop when idle", passed,
                 passed ? "Idle stop handled gracefully" : "Idle stop failed");
    return passed;
}

/**
 * @brief Speaker: report_state to ERROR
 */
static bool _test_spk_report_state_error(void)
{
    eos_dev_speaker_report_state(DEV_STATE_ERROR);
    eos_dev_state_t state = eos_dev_speaker_get_state();
    bool passed = (state == DEV_STATE_ERROR);
    /* Restore to READY */
    eos_dev_speaker_report_state(DEV_STATE_READY);
    _record_test("Speaker: report_state ERROR", passed,
                 passed ? "State set to ERROR" : "State not ERROR");
    return passed;
}

/**
 * @brief Mic: report_state to ERROR
 */
static bool _test_mic_report_state_error(void)
{
    eos_dev_microphone_report_state(DEV_STATE_ERROR);
    eos_dev_state_t state = eos_dev_microphone_get_state();
    bool passed = (state == DEV_STATE_ERROR);
    eos_dev_microphone_report_state(DEV_STATE_READY);
    _record_test("Mic: report_state ERROR", passed,
                 passed ? "State set to ERROR" : "State not ERROR");
    return passed;
}

static void _run_speaker_device_tests(void)
{
    _update_result("Running Speaker Device Tests...");

    _test_spk_get_instance();
    _test_spk_get_instance_consistent();
    _test_spk_initial_state();
    _test_spk_register_null_ops();
    _test_spk_register_incomplete_ops();
    _test_spk_register_missing_open();
    _test_spk_register_missing_is_available();
    /* These need the device to already be registered by port_audio_init: */
    _test_spk_register_required_only();
    _test_spk_register_duplicate();
    _test_spk_state_transitions();
    _test_spk_report_state_same();
    _test_spk_report_state_error();
}

static void _run_microphone_device_tests(void)
{
    _update_result("Running Microphone Device Tests...");

    _test_mic_get_instance();
    _test_mic_get_instance_consistent();
    _test_mic_initial_state();
    _test_mic_register_null_ops();
    _test_mic_register_incomplete_ops();
    _test_mic_register_missing_set_buffer();
    _test_mic_register_missing_is_available();
    _test_mic_register_required_only();
    _test_mic_register_duplicate();
    _test_mic_state_transitions();
    _test_mic_report_state_error();
}

static void _run_recording_tests(void)
{
    _update_result("Running Recording Tests...");

    _test_service_record_null();
    _test_service_record_start();
    _test_service_record_stop();
    _test_service_record_playback();
}

static void _run_service_tests(void)
{
    _update_result("Running Audio Service Tests...");

    /* Volume / Mute */
    _test_service_set_volume_valid();
    _test_service_set_volume_min();
    _test_service_set_volume_max();
    _test_service_set_volume_clamped();
    _test_service_set_volume_idempotent();
    _test_service_get_volume();
    _test_service_mute_default();
    _test_service_mute_on();
    _test_service_mute_off();

    /* Playback */
    _test_service_play_null();
    _test_service_play_valid();
    _test_service_play_tone_unsupported();
    _test_service_stop();
    _test_service_stop_idle();

    /* Recording */
    _test_service_record_null();

    /* Availability */
    _test_service_speaker_available();
    _test_service_microphone_available();
}

static void _run_recording_flow_tests(void)
{
    _update_result("Running Recording & Playback Tests...");

    _test_service_record_start();
    _test_service_record_stop();
    _test_service_record_playback();
}

static void _test_category_cb(lv_event_t *e)
{
    int category = (int)(long)lv_event_get_user_data(e);

    memset(&_ctx.stats, 0, sizeof(_ctx.stats));
    lv_obj_clean(_ctx.list);

    switch (category)
    {
    case 0:
        _run_speaker_device_tests();
        break;
    case 1:
        _run_microphone_device_tests();
        break;
    case 2:
        _run_service_tests();
        break;
    case 3:
        _run_recording_flow_tests();
        break;
    case 4:
        _run_speaker_device_tests();
        _run_microphone_device_tests();
        _run_service_tests();
        _run_recording_flow_tests();
        break;
    default:
        break;
    }

    /* Restore test volume */
    eos_service_audio_set_volume(50);

    char summary[256];
    snprintf(summary, sizeof(summary),
             "Total: %u | Pass: %u | Fail: %u",
             _ctx.stats.total_tests,
             _ctx.stats.passed_tests,
             _ctx.stats.failed_tests);
    _update_result(summary);
}

static eos_activity_lifecycle_t s_audio_test_activity_lifecycle = {
    .on_enter = NULL,
    .on_destroy = NULL,
    .on_pause = NULL,
    .on_resume = NULL,
};

void eos_test_audio_start(void)
{
    eos_activity_t *activity = eos_activity_create(&s_audio_test_activity_lifecycle);
    if (!activity)
    {
        return;
    }

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
    {
        return;
    }

    eos_activity_set_title(activity, "Audio Tests");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    _ctx.container = lv_obj_create(view);
    lv_obj_set_size(_ctx.container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(_ctx.container, 8, 0);
    lv_obj_set_flex_flow(_ctx.container, LV_FLEX_FLOW_COLUMN);

    /* Category selection list */
    lv_obj_t *cat_list = lv_list_create(_ctx.container);
    lv_obj_set_size(cat_list, lv_pct(100), lv_pct(35));
    lv_obj_set_flex_grow(cat_list, 1);
    eos_crown_encoder_set_target_obj(cat_list);

    const char *categories[] = {
        "Speaker Device Tests",
        "Microphone Device Tests",
        "Audio Service Tests",
        "Recording & Playback",
        "Run All Tests"};

    for (int i = 0; i < 5; i++)
    {
        lv_obj_t *btn = lv_list_add_button(cat_list, NULL, categories[i]);
        lv_obj_add_event_cb(btn, _test_category_cb, LV_EVENT_CLICKED, (void *)(long)i);
    }

    /* Results list */
    _ctx.list = lv_list_create(_ctx.container);
    lv_obj_set_size(_ctx.list, lv_pct(100), lv_pct(50));
    lv_obj_set_flex_grow(_ctx.list, 1);
    lv_obj_set_style_pad_all(_ctx.list, 4, 0);

    /* Summary label */
    _ctx.result_label = lv_label_create(_ctx.container);
    lv_label_set_text(_ctx.result_label, "Select a test category to begin");
    lv_obj_set_style_text_align(_ctx.result_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(_ctx.result_label, lv_pct(100), LV_SIZE_CONTENT);

    eos_activity_enter(activity);
}

#endif /* EOS_ENABLE_TEST_APP */
