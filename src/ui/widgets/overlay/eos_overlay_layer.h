/**
 * @file eos_overlay_layer.h
 * @brief Four-layer overlay system on lv_layer_top()
 *
 * All layers are full-screen transparent containers with fixed sibling ordering.
 * From bottom to top:
 *
 *   lv_layer_top()
 *   +-- _user_top_layer    (layer 0 — app-level custom overlays [future SNI use])
 *   +-- _snapshot_layer    (layer 1 — activity transition snapshots, anim blockers)
 *   +-- _header_layer      (layer 2 — app header)
 *   +-- _overlay_layer     (layer 3 — system overlays: perm panel, lock, control center)
 *   +-- ... direct children (flashlight, test code — above everything)
 *
 * Guarantees via sibling ordering within the same LVGL layer:
 *   - Snapshots are BELOW the header → header stays visible during transitions
 *   - The header is BELOW overlays → overlays cover header naturally
 *   - No hide/show coordination needed between any layers
 */
#ifndef EOS_OVERLAY_LAYER_H
#define EOS_OVERLAY_LAYER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "lvgl.h"

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize the four overlay layers on lv_layer_top()
 * @note Must be called once during system init, before app_header_init()
 */
void eos_overlay_layer_init(void);

/**
 * @brief Get user top layer (layer 0 — bottommost, for future app-level overlays)
 */
lv_obj_t *eos_overlay_get_user_top_layer(void);

/**
 * @brief Get snapshot layer (layer 1 — transition snapshots, anim blockers)
 */
lv_obj_t *eos_overlay_get_snapshot_layer(void);

/**
 * @brief Get header layer (layer 2 — app header)
 */
lv_obj_t *eos_overlay_get_header_layer(void);

/**
 * @brief Get overlay layer (layer 3 — topmost, for system overlays)
 */
lv_obj_t *eos_overlay_get_overlay_layer(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_OVERLAY_LAYER_H */
