/**
 * @file eos_basic_widgets.h
 * @brief Basic widgets
 */

#ifndef EOS_BASIC_WIDGETS_H
#define EOS_BASIC_WIDGETS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_lang.h"
#include "eos_activity.h"
#include "eos_corner_radius.h"

/* Public macros ----------------------------------------------*/

#define EOS_LIST_CONTAINER_HEIGHT 100
#define EOS_LIST_OBJ_RADIUS 25
#define EOS_LIST_SECTION_PLACEHOLDER_HEIGHT 30
#define EOS_LIST_CONTAINER_PAD_ALL 24
#define EOS_ITEM_RADIUS 20

/* Public typedefs --------------------------------------------*/

/**
 * @brief Slider definition within list
 */
typedef struct
{
    lv_obj_t *slider;
    lv_obj_t *plus_btn;
    lv_obj_t *minus_btn;
    lv_obj_t *plus_label;
    lv_obj_t *minus_label;
    uint16_t plus_label_scale;
    uint16_t minus_label_scale;
} eos_list_slider_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Get the center coordinates of an object
 * @param obj Target object
 * @param x [out] Center point x coordinate
 * @param y [out] Center point y coordinate
 */
void eos_obj_get_coord_center(lv_obj_t *obj, lv_coord_t *x, lv_coord_t *y);
/**
 * @brief Create a standard button
 * @param parent Button's parent object
 * @param txt Button text content
 * @param clicked_cb Callback when button is pressed
 * @param event_user_data User data for callback event
 * @return lv_obj_t* Returns `lv_button` object if created successfully, otherwise NULL
 */
lv_obj_t *eos_button_create(lv_obj_t *parent, const char *txt, lv_event_cb_t clicked_cb, void *event_user_data);
/**
 * @brief Create an advanced button
 * @param parent Button's parent object
 * @param btn_color Button background color
 * @param txt Text
 * @param txt_color Text color
 * @param clicked_cb Callback when button is pressed
 * @param event_user_data User data for callback event
 * @return lv_obj_t* Returns `lv_button` object if created successfully, otherwise NULL
 */
lv_obj_t *eos_button_create_ex(lv_obj_t *parent,
                               lv_color_t btn_color,
                               const char *txt,
                               lv_color_t txt_color,
                               lv_event_cb_t clicked_cb,
                               void *event_user_data);
/**
 * @brief Create a draw buffer
 * @param w Width
 * @param h Height
 * @param cf Color format
 * @param stride Bytes per line, set to `0` for automatic calculation
 * @return lv_draw_buf_t* Returns draw buffer if created successfully, otherwise NULL
 */
lv_draw_buf_t *eos_draw_buf_create(uint32_t w, uint32_t h, lv_color_format_t cf, uint32_t stride);
/**
 * @brief Destroy a draw buffer
 */
void eos_draw_buf_destroy(lv_draw_buf_t *draw_buf);
/**
 * @brief Set switch state (with animation)
 * @param sw Target switch object
 * @param checked true = enabled, false = disabled
 */
void eos_switch_set_state(lv_obj_t *sw, bool checked);
/**
 * @brief Create a list with standard style and add placeholder
 * @param parent Parent object
 * @return lv_obj_t* List object
 */
lv_obj_t *eos_list_create(lv_obj_t *parent);
/**
 * @brief Check if list created by eos_list_create has pending transition animation
 * @param from Source Activity
 * @param to Target Activity
 * @param back Whether it's a return transition
 * @return bool true indicates dedicated list animation can be played
 */
bool eos_list_transition_should_animate(eos_activity_t *from, eos_activity_t *to, bool back);
/**
 * @brief Set up transition state for a list item click
 * @param list List object
 * @param button The clicked button / item within the list
 * @param activity Target subpage activity
 */
void eos_list_transition_setup(lv_obj_t *list, lv_obj_t *button, eos_activity_t *activity);
/**
 * @brief Play dedicated transition animation for eos_list_create list
 * @param at Animation timeline
 * @param from Source Activity
 * @param to Target Activity
 * @param back Whether it's a return transition
 */
void eos_list_transition_play(eos_anim_group_t *group, eos_activity_t *from, eos_activity_t *to, bool back);
/**
 * @brief Create a back button
 * @param parent Parent object
 * @param show_text Whether to show back text
 * @return lv_obj_t* Returns btn object if created successfully, otherwise NULL
 */
lv_obj_t *eos_back_btn_create(lv_obj_t *parent, bool show_text);
/**
 * @brief Add a button with icon to list
 * @param list Target list
 * @param icon Left side icon, supports image path
 * @param txt Button text
 * @return lv_obj_t* Returns button object pointer if created successfully
 *
 * Returns NULL if creation failed
 */
lv_obj_t *eos_list_add_button(lv_obj_t *list, const void *icon, const char *txt);
/**
 * @brief Add an entry button to list
 *
 * Example:
 * (Bluetooth   >)
 *
 * @param list Target list
 * @param txt  Left side string
 * @return lv_obj_t* Returns button object pointer if created successfully
 *
 * Returns NULL if creation failed
 */
lv_obj_t *eos_list_add_entry_button(lv_obj_t *list, const char *txt);
/**
 * @brief Add an entry button to list
 *
 * Example:
 * (Bluetooth   >)
 *
 * @param list Target list
 * @param id  Left side string ID
 * @return lv_obj_t* Returns button object pointer if created successfully
 *
 * Returns NULL if creation failed
 */
lv_obj_t *eos_list_add_entry_button_str_id(lv_obj_t *list, lang_string_id_t id);
/**
 * @brief Add a placeholder with specified pixel height to list
 * @param list Target list
 * @param height Placeholder height (px)
 * @return lv_obj_t* Returns created placeholder object pointer if created successfully
 *
 * Returns NULL if creation failed
 */
lv_obj_t *eos_list_add_placeholder(lv_obj_t *list, uint32_t height);
/**
 * @brief Add a switch to list
 * @param list Target list
 * @param txt Switch function description
 * @return lv_obj_t* Returns switch object (standard lv_switch object) if created successfully
 *
 * Returns NULL if creation failed
 */
lv_obj_t *eos_list_add_switch(lv_obj_t *list, const char *txt);
/**
 * @brief Add title text to list
 * @param txt Text content
 * @return lv_obj_t* Returns label object if created successfully, otherwise NULL
 */
lv_obj_t *eos_list_add_title(lv_obj_t *list, const char *txt);
/**
 * @brief Add comment text to list
 * @param txt Text content
 * @return lv_obj_t* Returns label object if created successfully, otherwise NULL
 */
lv_obj_t *eos_list_add_comment(lv_obj_t *list, const char *txt);
/**
 * @brief Create a round icon
 * @param parent Icon parent object
 * @param bg_color Round icon background color
 * @param icon_src Icon
 * @return lv_obj_t* Returns round icon object if created successfully, otherwise NULL
 */
lv_obj_t *eos_round_icon_create(lv_obj_t *parent, lv_color_t bg_color, const void *icon_src);
/**
 * @brief Add a button with round icon to list
 *
 * btn{  [icon]  [txt]  }
 *
 * @param list Target list
 * @param bg_color Round icon background color
 * @param icon Icon
 * @param txt Button description text
 * @return lv_obj_t* Returns button object (standard lv_button object) if created successfully
 *
 * Returns NULL if creation failed
 */
lv_obj_t *eos_list_add_round_icon_button(lv_obj_t *list, lv_color_t bg_color, const void *icon, const char *txt);
/**
 * @brief Add a button with round icon to list
 *
 * btn{  [icon]  [txt]  }
 *
 * @param list Target list
 * @param bg_color Round icon background color
 * @param icon Icon
 * @param id String ID for multi-language support
 * @return lv_obj_t* Returns button object (standard lv_button object) if created successfully
 *
 * Returns NULL if creation failed
 */
lv_obj_t *eos_list_add_round_icon_button_str_id(lv_obj_t *list,
                                                lv_color_t bg_color,
                                                const void *icon_src,
                                                lang_string_id_t id);
/**
 * @brief Create a slider in list
 * @param list Target list
 * @param txt Slider function description text (top left corner)
 * @return eos_list_slider_t* Returns slider object if created successfully
 *
 * Returns NULL if creation failed
 */
eos_list_slider_t *eos_list_add_slider(lv_obj_t *list, const char *txt);
/**
 * @brief Set scale of slider "decrease text"
 * @param ls Target list slider
 * @param scale Scale value (0～512)
 */
void eos_list_slider_set_minus_label_scale(eos_list_slider_t *ls, uint16_t scale);
/**
 * @brief Set scale of slider "increase text"
 * @param ls Target list slider
 * @param scale Scale value (0～512)
 */
void eos_list_slider_set_plus_label_scale(eos_list_slider_t *ls, uint16_t scale);
/**
 * @brief Create a container in list
 * @param list Target list
 * @return lv_obj_t* Returns container object if created successfully
 *
 * Returns NULL if creation failed
 */
lv_obj_t *eos_list_add_container(lv_obj_t *list);
/**
 * @brief Create a row with left-right aligned controls
 * @param parent Parent object
 * @param left_text Left side text, can be NULL
 * @param right_text Right side text, can be NULL
 * @param left_img_path Left side image path, can be NULL if not needed
 * @param icon_w Image width
 * @param icon_h Image height
 * @return lv_obj_t* Returns row object for further operations
 */
lv_obj_t *eos_row_create(lv_obj_t *parent,
                         const char *left_text,
                         const char *right_text,
                         const char *left_img_path,
                         int icon_w,
                         int icon_h);
/**
 * @brief Create a container with title (title is outside container)
 * @param list Target list
 * @param title Title string
 * @return lv_obj_t* Returns inner container if created successfully
 *
 * Returns NULL if creation failed
 */
lv_obj_t *eos_list_add_title_container(lv_obj_t *list, const char *title);
#ifdef __cplusplus
}
#endif

#endif /* EOS_BASIC_WIDGETS_H */
