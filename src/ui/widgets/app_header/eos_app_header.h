/**
 * @file eos_app_header.h
 * @brief Application top navigation header
 */

#ifndef EOS_APP_HEADER_H
#define EOS_APP_HEADER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_lang.h"
#include "eos_activity.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief Hide app header
 */
void eos_app_header_hide(void);
/**
 * @brief Show app header
 * @param a Activity to show app header
 */
void eos_app_header_show(eos_activity_t *a);
/**
 * @brief Animatically show or hide app header
 * @param a Target activity (used to refresh title and color when showing, can be NULL when hiding)
 * @param visible Whether to show
 * @param duration_ms Animation duration (milliseconds), immediate switch when 0
 */
void eos_app_header_set_visible_animated(eos_activity_t *a, bool visible, uint32_t duration_ms);

/**
 * @brief Slide app header vertically to show or hide it
 * @param a Target activity (used to refresh title and color when showing, can be NULL when hiding)
 * @param visible Whether to show
 * @param duration_ms Animation duration (milliseconds), immediate switch when 0
 *
 * When hiding, the header slides upward out of the screen.
 * When showing, the header slides downward back into place.
 */
void eos_app_header_slide_visible_animated(eos_activity_t *a, bool visible, uint32_t duration_ms);
/**
 * @brief Initialize app header
 *
 * App header will be placed in lv_layer_top() layer
 *
 * To hide app header use `eos_app_header_hide`
 *
 * To show app header use `eos_app_header_show`
 *
 * @warning App header can only be initialized once
 */
void eos_app_header_init(void);
/**
 * @brief Check if app header is currently visible
 */
bool eos_app_header_is_visible(void);
/**
 * @brief Attach app header to specified view
 * @param view View to attach
 */
void eos_app_header_attach_to_view(lv_obj_t *view);
/**
 * @brief Detach app header from view, restore to original parent object
 */
void eos_app_header_detach_from_view(void);
/**
 * @brief Check if app header is currently attached to a view
 */
bool eos_app_header_is_attached_to_view(void);
/**
 * @brief Play title change animation
 * @param from From which activity to switch
 * @param to Switch to which activity
 * @param need_anim Whether animation is needed
 * @param reverse_anim Whether to reverse animation
 */
void _play_title_changed_anim(eos_activity_t *from, eos_activity_t *to, bool need_anim, bool reverse_anim, lv_anim_timeline_t *at);
#ifdef __cplusplus
}
#endif

#endif /* EOS_APP_HEADER_H */
