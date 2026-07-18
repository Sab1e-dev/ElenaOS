/**
 * @file eos_overlay_layer.c
 * @brief Four-layer overlay system implementation on lv_layer_top()
 */
#include "eos_overlay_layer.h"

/* Includes ---------------------------------------------------*/
#define EOS_LOG_TAG "OverlayLayer"
#include "eos_log.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/

/*
 * 方案四 — overlay-vs-view draw tracking.
 * Toggle to 1 to log each time any overlay layer or the root screen is drawn.
 * Use with SNAP_DIAG in eos_anim.c to compare whether overlay layers
 * participate in the post-hide render pass.
 */
#define EOS_OVERLAY_DRAW_DIAG  0

/* Static variables -------------------------------------------*/
static lv_obj_t *_user_top_layer = NULL;
static lv_obj_t *_snapshot_layer = NULL;
static lv_obj_t *_header_layer = NULL;
static lv_obj_t *_overlay_layer = NULL;
static bool _initialized = false;

#if EOS_OVERLAY_DRAW_DIAG
/*
 * Generic draw-trace event callback attached to any overlay layer.
 * Prints when LVGL starts drawing within this layer's area.
 */
static void _diag_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    const char *layer_name = lv_obj_get_user_data(obj);

    if (code == LV_EVENT_DRAW_MAIN_BEGIN)
    {
        lv_area_t coords;
        lv_obj_get_coords(obj, &coords);
        EOS_LOG_E("[OVERLAY_DIAG] DRAW_BEGIN layer=%s obj=%p area=%d,%d-%d,%d",
                  layer_name ? layer_name : "?",
                  obj, coords.x1, coords.y1, coords.x2, coords.y2);
    }
}

/*
 * Attach draw-trace callback with a human-readable tag stored in user_data.
 */
static void _diag_attach(lv_obj_t *layer, const char *name)
{
    lv_obj_set_user_data(layer, (void *)name);
    lv_obj_add_event_cb(layer, _diag_draw_cb, LV_EVENT_DRAW_MAIN_BEGIN, NULL);
}
#endif /* EOS_OVERLAY_DRAW_DIAG */

/* Helper: create a full-screen transparent layer container ----*/
static lv_obj_t *_create_layer(void)
{
    lv_obj_t *layer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(layer, 0, 0);
    lv_obj_set_style_pad_all(layer, 0, 0);
    lv_obj_set_scrollbar_mode(layer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(layer, LV_OBJ_FLAG_SCROLLABLE);
    return layer;
}

/* Function Implementations -----------------------------------*/

void eos_overlay_layer_init(void)
{
    if (_initialized)
        return;

    /* Create from TOP to BOTTOM — each new layer is pushed to the background.
     * Final order (bottom→top): user_top / snapshot / header / overlay */

    _overlay_layer = _create_layer(); /* layer 3 — topmost */
    _header_layer = _create_layer();
    lv_obj_move_background(_header_layer); /* layer 2 — below overlay */

    _snapshot_layer = _create_layer();
    lv_obj_move_background(_snapshot_layer); /* layer 1 — below header */

    _user_top_layer = _create_layer();
    lv_obj_move_background(_user_top_layer); /* layer 0 — absolute bottom */

#if EOS_OVERLAY_DRAW_DIAG
    /* Attach draw-trace to each overlay layer by name */
    _diag_attach(_overlay_layer,    "overlay_layer");
    _diag_attach(_header_layer,     "header_layer");
    _diag_attach(_snapshot_layer,   "snapshot_layer");
    _diag_attach(_user_top_layer,   "user_top_layer");

    /* Also trace lv_layer_top() itself for comparison */
    lv_obj_t *top = lv_layer_top();
    if (top)
    {
        _diag_attach(top, "lv_layer_top");
    }

    /* Trace the active screen (root_screen) — this is where regular views live */
    lv_obj_t *scr = lv_screen_active();
    if (scr)
    {
        _diag_attach(scr, "active_screen");
    }
#endif

    _initialized = true;
    EOS_LOG_I("4-layer overlay system initialized on lv_layer_top()");
}

lv_obj_t *eos_overlay_get_user_top_layer(void)
{
    return _user_top_layer;
}

lv_obj_t *eos_overlay_get_snapshot_layer(void)
{
    return _snapshot_layer;
}

lv_obj_t *eos_overlay_get_header_layer(void)
{
    return _header_layer;
}

lv_obj_t *eos_overlay_get_overlay_layer(void)
{
    return _overlay_layer;
}
