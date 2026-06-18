/**
 * @file eos_test_audio_effects.c
 * @brief VAR audio effects selection and playback test page
 */

#include "eos_config.h"
#if EOS_ENABLE_TEST_APP

#include "eos_test_audio_effects.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_activity.h"
#include "eos_app_header.h"
#include "eos_audio_effects.h"
#include "eos_audio_player.h"
#include "eos_crown.h"
#include "eos_dev_speaker.h"
#define EOS_LOG_TAG "AudioEffectsTest"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/
typedef struct
{
    lv_obj_t *container;
    lv_obj_t *list;
    lv_obj_t *status_label;
} _effects_ctx_t;

static _effects_ctx_t _ctx = {0};

/* Function Implementations -----------------------------------*/

static void _play_effect(lv_event_t *e)
{
    eos_audio_effect_id_t id = (eos_audio_effect_id_t)(long)lv_event_get_user_data(e);

    const eos_audio_dsc_t *dsc = eos_audio_effect_get(id);
    if (dsc == NULL)
    {
        EOS_LOG_W("Effect not found: %d", id);
        return;
    }

    static bool _player_inited = false;
    static eos_audio_player_t _player;
    if (!_player_inited)
    {
        eos_audio_player_init(&_player);
        _player_inited = true;
    }

    eos_result_t ret = eos_audio_player_play(&_player, dsc, EOS_AUDIO_SRC_VAR);
    if (ret == EOS_OK)
    {
        const char *name = eos_audio_effect_get_name(id);
        EOS_LOG_I("Playing effect: %s", name);
        if (_ctx.status_label && lv_obj_is_valid(_ctx.status_label))
        {
            lv_label_set_text_fmt(_ctx.status_label, "Playing: %s", name);
        }
    }
    else
    {
        EOS_LOG_W("Failed to play effect: %d", ret);
        if (_ctx.status_label && lv_obj_is_valid(_ctx.status_label))
        {
            lv_label_set_text(_ctx.status_label, "Playback failed - check speaker");
        }
    }
}

static void _activity_on_destroy(eos_activity_t *activity)
{
    LV_UNUSED(activity);
    memset(&_ctx, 0, sizeof(_ctx));
}

static eos_activity_lifecycle_t s_effects_lifecycle = {
    .on_enter = NULL,
    .on_destroy = _activity_on_destroy,
    .on_pause = NULL,
    .on_resume = NULL,
};

void eos_test_audio_effects_start(void)
{
    eos_activity_t *activity = eos_activity_create(&s_effects_lifecycle);
    if (!activity)
        return;

    lv_obj_t *view = eos_activity_get_view(activity);
    if (!view)
        return;

    eos_activity_set_title(activity, "VAR Effects");
    eos_activity_set_type(activity, EOS_ACTIVITY_TYPE_APP);

    lv_obj_t *list = lv_list_create(view);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(list, 16, 0);
    lv_obj_set_style_pad_row(list, 8, 0);

    lv_obj_t *title = lv_label_create(list);
    lv_label_set_text(title, LV_SYMBOL_AUDIO " Select an effect");

    lv_obj_t *hint = lv_label_create(list);
    lv_label_set_text(hint, "Tap to play.");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xA0A0A0), LV_PART_MAIN);

    for (int i = 0; i < EOS_AUDIO_EFFECT_COUNT; i++)
    {
        const char *name = eos_audio_effect_get_name((eos_audio_effect_id_t)i);
        lv_obj_t *btn = lv_list_add_button(list, LV_SYMBOL_PLAY, name);
        lv_obj_add_event_cb(btn, _play_effect, LV_EVENT_CLICKED, (void *)(long)i);
    }

    _ctx.status_label = lv_label_create(list);
    lv_obj_set_width(_ctx.status_label, lv_pct(100));
    lv_label_set_long_mode(_ctx.status_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(_ctx.status_label, "Ready - tap an effect to play");
    lv_obj_set_style_text_align(_ctx.status_label, LV_TEXT_ALIGN_CENTER, 0);

    _ctx.list = list;
    _ctx.container = view;

    eos_crown_encoder_set_target_obj(list);
    eos_activity_enter(activity);
}

#endif /* EOS_ENABLE_TEST_APP */
