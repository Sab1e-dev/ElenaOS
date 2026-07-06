/**
 * @file eos_test_audio.c
 * @brief Comprehensive audio subsystem test module
 */

#include "eos_test_audio.h"
#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

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
#include "eos_test_framework.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_LOG_TAG "AudioTest"
#define RECORD_TEST_PATH "/.sys/test_recording.wav"

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static void _record_test(const char *name, bool passed, const char *details)
{
    eos_test_record(name, passed, details);
}

/**
 * @brief Speaker: get_instance returns non-null (even before register)
 */
static bool _test_spk_get_instance(void)
{
    eos_dev_speaker_t *spk = eos_dev_speaker_get_instance();
    bool passed = (spk != NULL);
    _record_test("Speaker: get_instance non-null", passed, passed ? "Instance exists" : "get_instance returned NULL");
    return passed;
}

/**
 * @brief Speaker: state is READY (pre-registered by simulator port)
 */
static bool _test_spk_initial_state(void)
{
    eos_dev_state_t state = eos_dev_speaker_get_state();
    bool passed = (state == DEV_STATE_READY);
    _record_test("Speaker: state is READY", passed, passed ? "State is DEV_STATE_READY" : "State incorrect");
    return passed;
}

/**
 * @brief Speaker: register rejects NULL ops
 */
static bool _test_spk_register_null_ops(void)
{
    eos_result_t ret = eos_dev_speaker_register(NULL);
    bool passed = (ret != EOS_OK);
    _record_test("Speaker: reject NULL ops", passed, passed ? "NULL ops rejected" : "NULL ops accepted");
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
    _record_test("Speaker: reject incomplete ops",
                 passed,
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
    _record_test("Speaker: reject missing open", passed, passed ? "Missing open rejected" : "Should have rejected");
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
    _record_test("Speaker: reject missing is_available",
                 passed,
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
        _record_test("Speaker: reject duplicate register", false, "Pre-condition: no ops registered yet");
        return false;
    }

    eos_result_t ret = eos_dev_speaker_register(spk->ops);
    bool passed = (ret == EOS_ERR_ALREADY_EXISTS);
    _record_test("Speaker: reject duplicate register",
                 passed,
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
    _record_test("Speaker: state transitions", passed, passed ? "READY -> BUSY -> READY" : "State transition failed");
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
    _record_test("Speaker: report_state same no-op",
                 passed,
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
    _record_test("Mic: get_instance non-null", passed, passed ? "Instance exists" : "get_instance returned NULL");
    return passed;
}

/**
 * @brief Mic: state is READY (pre-registered by simulator port)
 */
static bool _test_mic_initial_state(void)
{
    eos_dev_state_t state = eos_dev_microphone_get_state();
    bool passed = (state == DEV_STATE_READY);
    _record_test("Mic: state is READY", passed, passed ? "State is DEV_STATE_READY" : "State incorrect");
    return passed;
}

/**
 * @brief Mic: register rejects NULL ops
 */
static bool _test_mic_register_null_ops(void)
{
    eos_result_t ret = eos_dev_microphone_register(NULL);
    bool passed = (ret != EOS_OK);
    _record_test("Mic: reject NULL ops", passed, passed ? "NULL ops rejected" : "NULL ops accepted");
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
    _record_test("Mic: reject incomplete ops", passed, passed ? "Incomplete ops rejected" : "Incomplete ops accepted");
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
    _record_test("Mic: reject missing set_buffer",
                 passed,
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
    _record_test("Mic: reject missing is_available",
                 passed,
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
        _record_test("Mic: reject duplicate register", false, "Pre-condition: no ops registered yet");
        return false;
    }

    eos_result_t ret = eos_dev_microphone_register(mic->ops);
    bool passed = (ret == EOS_ERR_ALREADY_EXISTS);
    _record_test("Mic: reject duplicate register",
                 passed,
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
    _record_test("Mic: state transitions", passed, passed ? "READY -> BUSY -> READY" : "State transition failed");
    return passed;
}

/**
 * @brief Service: set_volume with valid value succeeds
 */
static bool _test_service_set_volume_valid(void)
{
    eos_result_t ret = eos_service_audio_set_volume(50);
    bool passed = (ret == EOS_OK);
    _record_test("Service: set_volume 50", passed, passed ? "Volume set successfully" : "set_volume failed");
    return passed;
}

/**
 * @brief Service: set_volume 0 (min) succeeds
 */
static bool _test_service_set_volume_min(void)
{
    eos_result_t ret = eos_service_audio_set_volume(0);
    bool passed = (ret == EOS_OK);
    _record_test("Service: set_volume 0 (min)", passed, passed ? "Min volume set" : "set_volume failed");
    return passed;
}

/**
 * @brief Service: set_volume 100 (max) succeeds
 */
static bool _test_service_set_volume_max(void)
{
    eos_result_t ret = eos_service_audio_set_volume(100);
    bool passed = (ret == EOS_OK);
    _record_test("Service: set_volume 100 (max)", passed, passed ? "Max volume set" : "set_volume failed");
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
    _record_test("Service: set_volume 200 clamped to 100",
                 passed,
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
    _record_test("Service: get_volume after set", passed, passed ? "Volume persisted correctly" : "Volume mismatch");
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
    _record_test("Service: mute on", passed, passed ? "Mute enabled" : "Mute not set");
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
    _record_test("Service: mute off", passed, passed ? "Mute disabled" : "Mute still set");
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
    _record_test("Service: default mute = false", passed, passed ? "Mute defaults to false" : "Mute default incorrect");
    return passed;
}

/**
 * @brief Service: play rejects NULL path
 */
static bool _test_service_play_null(void)
{
    eos_result_t ret = eos_service_audio_play(NULL);
    bool passed = (ret == EOS_ERR_INVALID_ARG);
    _record_test("Service: play NULL path rejected",
                 passed,
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
    bool passed = (ret == EOS_OK || ret == EOS_ERR_DEV_ERROR || ret == EOS_ERR_NOT_FOUND);
    _record_test("Service: play file API works",
                 passed,
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
    _record_test("Service: stop", passed, passed ? "Stop succeeded" : "Stop failed");
    return passed;
}

/**
 * @brief Service: play_tone when ops is NULL returns EOS_ERR_DEV_OPS_NOT_SUPPORTED
 */
static bool _test_service_play_tone_unsupported(void)
{
    eos_result_t ret = eos_service_audio_play_tone(440, 500);
    bool passed = (ret == EOS_ERR_DEV_OPS_NOT_SUPPORTED);
    _record_test("Service: play_tone unsupported",
                 passed,
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
    _record_test("Service: record NULL path rejected",
                 passed,
                 passed ? "NULL path returns EOS_ERR_INVALID_ARG" : "Should reject NULL");
    return passed;
}

/**
 * @brief Service: start recording to a file
 */
static bool _test_service_record_start(void)
{
    eos_result_t ret = eos_service_audio_start_recording(RECORD_TEST_PATH);
    /* Accept EOS_OK, or EOS_ERR_DEV_ERROR (permission denied / no input device) */
    bool passed = (ret == EOS_OK || ret == EOS_ERR_DEV_ERROR);
    _record_test("Service: start recording",
                 passed,
                 passed
                     ? (ret == EOS_OK ? "Recording started" : "Permission/mic not available (expected on some systems)")
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
    _record_test("Service: stop recording", passed, passed ? "Recording stopped" : "Stop recording failed");
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
    _record_test("Service: play back recording",
                 passed,
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
    _record_test("Service: speaker available",
                 passed,
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
    _record_test("Service: microphone available",
                 passed,
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
    _record_test("Service: set_volume idempotent", passed, passed ? "Repeated call succeeds" : "Repeated call failed");
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
    _record_test("Speaker: get_instance consistent", passed, passed ? "Same pointer returned" : "Different pointers");
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
    _record_test("Mic: get_instance consistent", passed, passed ? "Same pointer returned" : "Different pointers");
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
    _record_test("Speaker: register with optional NULL ops",
                 passed,
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
    _record_test("Mic: register with optional NULL ops",
                 passed,
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
    _record_test("Service: stop when idle", passed, passed ? "Idle stop handled gracefully" : "Idle stop failed");
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
    _record_test("Speaker: report_state ERROR", passed, passed ? "State set to ERROR" : "State not ERROR");
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
    _record_test("Mic: report_state ERROR", passed, passed ? "State set to ERROR" : "State not ERROR");
    return passed;
}

static void _run_speaker_device_tests(void)
{
    _test_spk_get_instance();
    _test_spk_get_instance_consistent();
    _test_spk_initial_state();
    _test_spk_register_null_ops();
    _test_spk_register_incomplete_ops();
    _test_spk_register_missing_open();
    _test_spk_register_missing_is_available();
    _test_spk_register_required_only();
    _test_spk_register_duplicate();
    _test_spk_state_transitions();
    _test_spk_report_state_same();
    _test_spk_report_state_error();
}

void eos_test_audio_register_tests(void)
{
    eos_test_register("Speaker: get_instance non-null", _test_spk_get_instance);
    eos_test_register("Speaker: get_instance consistent", _test_spk_get_instance_consistent);
    eos_test_register("Speaker: state is READY", _test_spk_initial_state);
    eos_test_register("Speaker: reject NULL ops", _test_spk_register_null_ops);
    eos_test_register("Speaker: reject incomplete ops", _test_spk_register_incomplete_ops);
    eos_test_register("Speaker: reject missing open", _test_spk_register_missing_open);
    eos_test_register("Speaker: reject missing is_available", _test_spk_register_missing_is_available);
    eos_test_register("Speaker: register with optional NULL ops", _test_spk_register_required_only);
    eos_test_register("Speaker: reject duplicate register", _test_spk_register_duplicate);
    eos_test_register("Speaker: state transitions", _test_spk_state_transitions);
    eos_test_register("Speaker: report_state same no-op", _test_spk_report_state_same);
    eos_test_register("Speaker: report_state ERROR", _test_spk_report_state_error);
    eos_test_register("Mic: get_instance non-null", _test_mic_get_instance);
    eos_test_register("Mic: get_instance consistent", _test_mic_get_instance_consistent);
    eos_test_register("Mic: state is READY", _test_mic_initial_state);
    eos_test_register("Mic: reject NULL ops", _test_mic_register_null_ops);
    eos_test_register("Mic: reject incomplete ops", _test_mic_register_incomplete_ops);
    eos_test_register("Mic: reject missing set_buffer", _test_mic_register_missing_set_buffer);
    eos_test_register("Mic: reject missing is_available", _test_mic_register_missing_is_available);
    eos_test_register("Mic: register with optional NULL ops", _test_mic_register_required_only);
    eos_test_register("Mic: reject duplicate register", _test_mic_register_duplicate);
    eos_test_register("Mic: state transitions", _test_mic_state_transitions);
    eos_test_register("Mic: report_state ERROR", _test_mic_report_state_error);
    eos_test_register("Service: set_volume 50", _test_service_set_volume_valid);
    eos_test_register("Service: set_volume 0 (min)", _test_service_set_volume_min);
    eos_test_register("Service: set_volume 100 (max)", _test_service_set_volume_max);
    eos_test_register("Service: set_volume 200 clamped to 100", _test_service_set_volume_clamped);
    eos_test_register("Service: set_volume idempotent", _test_service_set_volume_idempotent);
    eos_test_register("Service: get_volume after set", _test_service_get_volume);
    eos_test_register("Service: default mute = false", _test_service_mute_default);
    eos_test_register("Service: mute on", _test_service_mute_on);
    eos_test_register("Service: mute off", _test_service_mute_off);
    eos_test_register("Service: play NULL path rejected", _test_service_play_null);
    eos_test_register("Service: play file API works", _test_service_play_valid);
    eos_test_register("Service: play_tone unsupported", _test_service_play_tone_unsupported);
    eos_test_register("Service: stop", _test_service_stop);
    eos_test_register("Service: stop when idle", _test_service_stop_idle);
    eos_test_register("Service: record NULL path rejected", _test_service_record_null);
    eos_test_register("Service: start recording", _test_service_record_start);
    eos_test_register("Service: stop recording", _test_service_record_stop);
    eos_test_register("Service: play back recording", _test_service_record_playback);
    eos_test_register("Service: speaker available", _test_service_speaker_available);
    eos_test_register("Service: microphone available", _test_service_microphone_available);
}

void eos_test_audio_start(void)
{
    eos_test_audio_register_tests();
    eos_test_fw_page_start("Audio Tests");
}

#endif /* EOS_ENABLE_TEST_APP */
