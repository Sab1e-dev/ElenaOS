/**
 * @file eos_perm_panel.c
 * @brief Permission request panel implementation
 *
 * Creates a full-screen overlay on lv_layer_top() with:
 *  - Solid dark background (fully covers content below including header)
 *  - Title: "Allow \"<App>\" to use your <Permission>?"
 *  - Description message
 *  - Three vertically-stacked buttons: Allow Once, Allow While Using App, Don't Allow
 *
 * The overlay_layer container (on lv_layer_sys, above the header_layer)
 * naturally obscures the app header without any hide/show coordination.
 * Side key and crown are disabled while the panel is shown (via overlay registration).
 */
#include "eos_perm_panel.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include "eos_theme.h"
#include "eos_icon.h"
#include "eos_lang.h"
#include "eos_font.h"
#include "eos_mem.h"
#include "eos_activity.h"
#include "eos_overlay_layer.h"
#include "eos_chrome_manager.h"
#define EOS_LOG_TAG "PermPanel"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/
#define _PERM_PANEL_PAD_HORIZ 40
#define _PERM_PANEL_PAD_TOP 80
#define _PERM_PANEL_PAD_BOTTOM 30
#define _PERM_PANEL_GAP 12

/* Forward declarations ---------------------------------------*/
static void _perm_panel_container_delete_cb(lv_event_t *e);
static void _perm_any_button_cb(lv_event_t *e);
static void _perm_panel_pull_back(void);
static void _perm_panel_hide(void);

/* Overlay descriptor for chrome manager ----------------------*/
static eos_perm_panel_t *_active_panel = NULL;

static const eos_chrome_overlay_t s_perm_overlay = {
    .pull_back = _perm_panel_pull_back,
    .hide = _perm_panel_hide,
    .on_focus = NULL,
    .is_open = NULL,
    .get_scrollable = NULL,
    .get_foreground_obj = NULL,
    .name = "PermissionPanel",
};

/* Function Implementations -----------------------------------*/

eos_perm_panel_t *eos_perm_panel_create(const eos_perm_panel_cfg_t *cfg)
{
    if (!cfg || !cfg->app_name)
        return NULL;

    eos_perm_panel_t *p = (eos_perm_panel_t *)eos_malloc(sizeof(eos_perm_panel_t));
    if (!p)
        return NULL;
    memset(p, 0, sizeof(eos_perm_panel_t));
    p->cfg = *cfg;

    /* Register as overlay to disable side key / crown */
    _active_panel = p;
    eos_chrome_manager_notify_overlay_opened(&s_perm_overlay);

    /* Full-screen container on overlay layer (above header_layer) -*/
    p->container = lv_obj_create(eos_overlay_get_overlay_layer());
    lv_obj_remove_style_all(p->container);
    lv_obj_set_size(p->container, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(p->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p->container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(p->container, EOS_COLOR_BLACK, 0);
    lv_obj_set_style_bg_opa(p->container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p->container, 0, 0);
    lv_obj_set_style_pad_hor(p->container, _PERM_PANEL_PAD_HORIZ, 0);
    lv_obj_set_style_pad_top(p->container, _PERM_PANEL_PAD_TOP, 0);
    lv_obj_set_style_pad_bottom(p->container, _PERM_PANEL_PAD_BOTTOM, 0);
    lv_obj_set_scrollbar_mode(p->container, LV_SCROLLBAR_MODE_OFF);

    /* Block scroll/click-through and ensure topmost z-order */
    lv_obj_add_flag(p->container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(p->container);

    /* Title label ------------------------------------------------*/
    p->title = lv_label_create(p->container);
    lv_obj_set_width(p->title, lv_pct(100));
    lv_label_set_long_mode(p->title, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(p->title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(p->title, 24, 0);
    lv_obj_set_style_pad_bottom(p->title, _PERM_PANEL_GAP, 0);
    eos_label_set_font_size(p->title, EOS_FONT_SIZE_LARGE);
    lv_obj_set_style_text_color(p->title, EOS_COLOR_WHITE, 0);

    const char *perm_name = eos_permission_category_name(cfg->category);
    lv_label_set_text_fmt(p->title,
                          eos_lang_get_text(STR_ID_PERM_REQUEST_TITLE),
                          cfg->app_name,
                          perm_name ? perm_name : "");

    /* Message/description label ----------------------------------*/
    p->message = lv_label_create(p->container);
    lv_obj_set_width(p->message, lv_pct(100));
    lv_label_set_long_mode(p->message, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(p->message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(p->message, EOS_COLOR_GREY, 0);
    lv_obj_set_style_pad_bottom(p->message, _PERM_PANEL_GAP, 0);

    const char *desc = eos_permission_category_desc(cfg->category);
    lv_label_set_text(p->message, desc ? desc : "");

    /* Push buttons to bottom with a spacer -----------------------*/
    lv_obj_t *spacer = lv_obj_create(p->container);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_flex_grow(spacer, 1);

    /* Actions container (vertical button stack) ------------------*/
    lv_obj_t *actions = lv_obj_create(p->container);
    lv_obj_remove_style_all(actions);
    lv_obj_set_size(actions, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(actions, 10, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_style_pad_all(actions, 0, 0);

    /* Allow Once button ------------------------------------------*/
    p->allow_once_btn = lv_button_create(actions);
    lv_obj_set_size(p->allow_once_btn, lv_pct(100), EOS_PERM_PANEL_BUTTON_HEIGHT);
    lv_obj_set_style_radius(p->allow_once_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(p->allow_once_btn, EOS_THEME_BUTTON_COLOR, 0);

    lv_obj_t *allow_once_label = lv_label_create(p->allow_once_btn);
    lv_label_set_text(allow_once_label, eos_lang_get_text(STR_ID_PERM_ALLOW_ONCE));
    lv_obj_set_style_text_color(allow_once_label, EOS_COLOR_WHITE, 0);
    lv_obj_center(allow_once_label);
    lv_obj_add_event_cb(p->allow_once_btn, _perm_any_button_cb, LV_EVENT_CLICKED, p);
    lv_obj_add_event_cb(p->allow_once_btn, cfg->allow_once_cb, LV_EVENT_CLICKED, p);

    /* Allow While Using App button -------------------------------*/
    p->allow_foreground_btn = lv_button_create(actions);
    lv_obj_set_size(p->allow_foreground_btn, lv_pct(100), EOS_PERM_PANEL_BUTTON_HEIGHT);
    lv_obj_set_style_radius(p->allow_foreground_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(p->allow_foreground_btn, EOS_THEME_BUTTON_COLOR, 0);

    lv_obj_t *allow_fg_label = lv_label_create(p->allow_foreground_btn);
    lv_label_set_text(allow_fg_label, eos_lang_get_text(STR_ID_PERM_ALLOW_FOREGROUND));
    lv_obj_set_style_text_color(allow_fg_label, EOS_COLOR_WHITE, 0);
    lv_obj_center(allow_fg_label);
    lv_obj_add_event_cb(p->allow_foreground_btn, _perm_any_button_cb, LV_EVENT_CLICKED, p);
    lv_obj_add_event_cb(p->allow_foreground_btn, cfg->allow_foreground_cb, LV_EVENT_CLICKED, p);

    /* Don't Allow button -----------------------------------------*/
    p->deny_btn = lv_button_create(actions);
    lv_obj_set_size(p->deny_btn, lv_pct(100), EOS_PERM_PANEL_BUTTON_HEIGHT);
    lv_obj_set_style_radius(p->deny_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(p->deny_btn, EOS_THEME_BUTTON_COLOR, 0);

    lv_obj_t *deny_label = lv_label_create(p->deny_btn);
    lv_label_set_text(deny_label, eos_lang_get_text(STR_ID_PERM_DENY));
    lv_obj_set_style_text_color(deny_label, EOS_COLOR_WHITE, 0);
    lv_obj_center(deny_label);
    lv_obj_add_event_cb(p->deny_btn, _perm_any_button_cb, LV_EVENT_CLICKED, p);
    lv_obj_add_event_cb(p->deny_btn, cfg->deny_cb, LV_EVENT_CLICKED, p);

    /* Auto-free on container delete */
    lv_obj_add_event_cb(p->container, _perm_panel_container_delete_cb, LV_EVENT_DELETE, p);

    EOS_LOG_D("Permission panel created for app '%s', category %d", cfg->app_name, cfg->category);
    return p;
}

void eos_perm_panel_delete(eos_perm_panel_t *panel)
{
    if (!panel)
        return;

    /* Remove from overlay stack (re-enables side key / crown) */
    if (_active_panel == panel)
    {
        _active_panel = NULL;
        eos_chrome_manager_notify_overlay_closed(&s_perm_overlay);
    }

    if (panel->container && lv_obj_is_valid(panel->container))
    {
        lv_obj_remove_event_cb_with_user_data(panel->container, _perm_panel_container_delete_cb, panel);
        lv_obj_delete(panel->container);
    }

    eos_free(panel);
}

/* Internal callbacks -----------------------------------------*/

/**
 * @brief Common handler for all three action buttons.
 *        The specific callbacks (allow_once / allow_foreground / deny) are also registered
 *        on the same button and will fire after this one.
 *        Header show/hide is no longer needed — the overlay_layer naturally obscures it.
 */
static void _perm_any_button_cb(lv_event_t *e)
{
    (void)e;
}

static void _perm_panel_container_delete_cb(lv_event_t *e)
{
    eos_perm_panel_t *panel = (eos_perm_panel_t *)lv_event_get_user_data(e);
    if (!panel)
        return;

    if (lv_event_get_code(e) == LV_EVENT_DELETE)
    {
        /* Container is being deleted - cleanup overlay state */
        if (_active_panel == panel)
        {
            _active_panel = NULL;
            eos_chrome_manager_notify_overlay_closed(&s_perm_overlay);
        }
        panel->container = NULL;
        eos_free(panel);
        EOS_LOG_D("PermPanel auto-freed");
    }
}

/* Overlay callbacks for chrome manager -----------------------*/

static void _perm_panel_pull_back(void)
{
    /* No-op: absorb crown/side key events while permission panel is shown.
     * The user must explicitly tap a button on the panel to dismiss it. */
}

static void _perm_panel_hide(void)
{
    if (!_active_panel)
        return;
    eos_perm_panel_delete(_active_panel);
}
