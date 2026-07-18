/**
 * @file eos_overlay_layer.c
 * @brief Four-layer overlay system implementation on lv_layer_top()
 */
#include "eos_overlay_layer.h"

/* Includes ---------------------------------------------------*/
#define EOS_LOG_TAG "OverlayLayer"
#include "eos_log.h"
#include "eos_mem.h"

/* Static variables -------------------------------------------*/
static lv_obj_t *_user_top_layer = NULL;
static lv_obj_t *_snapshot_layer = NULL;
static lv_obj_t *_header_layer = NULL;
static lv_obj_t *_overlay_layer = NULL;
static bool _initialized = false;

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
