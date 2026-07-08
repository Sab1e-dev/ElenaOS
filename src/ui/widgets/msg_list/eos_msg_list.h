/**
 * @file eos_msg_list.h
 * @brief Drop-down message list
 */

#ifndef EOS_MSG_LIST_H
#define EOS_MSG_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_chrome_manager.h"
#include "eos_swipe_panel.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef struct eos_msg_list_t eos_msg_list_t; // Forward declaration

/**
 * @brief Message list item structure
 * Hierarchy:
 * container{
 * row1[icon title_label   time_label]
 *      msg_label
 * }
 */
typedef struct
{
    eos_msg_list_t *msg_list;
    lv_obj_t *container;
    lv_obj_t *row1;
    lv_obj_t *icon;
    lv_obj_t *title_label;
    lv_obj_t *msg_label;
    lv_obj_t *time_label;
    const char *msg_str; /**< Message string */
    bool is_deleted;
} eos_msg_list_item_t;

/**
 * @brief Message list structure
 * Hierarchy:
 * swipe_obj{
 *      list{
 *          clear_all_btn
 *          msg_item
 *          ...
 *          msg_item
 *      }
 *      no_msg_label
 * }
 */
struct eos_msg_list_t
{
    eos_swipe_panel_t *swipe_panel; /**< Drag object pointer */
    lv_obj_t *list; /**< List object pointer */
    lv_obj_t *clear_all_btn; /**< Clear all messages button pointer */
    lv_obj_t *no_msg_label; /**< No message prompt label */
    uint16_t animating_count; /**< Number of animating messages after clear-all press */
};

/* Public function prototypes --------------------------------*/
/**
 * @brief Create message item
 * @param list Parent message list of the message item
 * @return eos_msg_list_item_t* Pointer to the created message item (dynamic memory allocation)
 */
eos_msg_list_item_t *eos_msg_list_item_create(eos_msg_list_t *list);
/**
 * @brief Delete message item
 * @param item Pointer to the message item to delete
 */
void eos_msg_list_item_delete(eos_msg_list_item_t *item);
/**
 * @brief Set message content
 * @param item Target message item
 * @param msg Message string
 */
void eos_msg_list_item_set_msg(eos_msg_list_item_t *item, const char *msg);

/**
 * @brief Set title
 * @param item Target message item
 * @param title Message title (APP) string
 */
void eos_msg_list_item_set_title(eos_msg_list_item_t *item, const char *title);

/**
 * @brief Set time text
 * @param item Target message item
 * @param time Message receive time string (e.g.: "12:30", "One hour ago")
 */
void eos_msg_list_item_set_time(eos_msg_list_item_t *item, const char *time);
/**
 * @brief Set icon
 * @param item Target message item
 * @param src Image source
 */
void eos_msg_list_item_icon_set_src(eos_msg_list_item_t *item, const char *src);
/**
 * @brief Delete message list
 * @param list Target list
 */
void eos_msg_list_delete(eos_msg_list_t *list);
/**
 * @brief Get message list instance
 * @return eos_msg_list_t*
 */
eos_msg_list_t *eos_msg_list_get_instance(void);
/**
 * @brief Initialize message list
 */
void eos_msg_list_init(void);
/**
 * @brief Close detail page if open (for chrome manager integration)
 */
void eos_msg_list_close_detail(void);
/**
 * @brief Show message list
 */
void eos_msg_list_show(void);
/**
 * @brief Hide message list
 */
void eos_msg_list_hide(void);
const eos_chrome_overlay_t *eos_msg_list_get_overlay_descriptor(void);
#ifdef __cplusplus
}
#endif

#endif /* EOS_MSG_LIST_H */
