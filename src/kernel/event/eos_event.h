/**
 * @file eos_event.h
 * @brief Event broadcast system - global broadcast using event ID as index
 */

#ifndef EOS_EVENT_H
#define EOS_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

/* Public macros ----------------------------------------------*/

#define EOS_EVENT_MAX (UINT32_MAX)

/* Public typedefs --------------------------------------------*/

/**
 * @brief Event structure (private, opaque to users)
 */
typedef struct _eos_event_t eos_event_t;

/**
 * @brief Event callback function type
 * @param e Event structure
 */
typedef void (*eos_event_cb_t)(eos_event_t *e);

/**
 * @brief Event type definitions
 * @note New events can be added here
 */
typedef enum
{
    EOS_EVENT_UNKNOWN = 0,
    EOS_EVENT_APP_UNINSTALLED, /**< Application has been uninstalled */
    EOS_EVENT_APP_INSTALLED, /**< Application has been installed */
    EOS_EVENT_SYSTEM_SLEEP, /**< System entered sleep mode */
    EOS_EVENT_SYSTEM_DISPLAY_ON, /**< System has been awakened */
    EOS_EVENT_SYSTEM_DISPLAY_AOD, /**< Screen always-on mode has been activated */
    EOS_EVENT_SYSTEM_CONFIG_UPDATE, /**< Configuration file has been updated */
    EOS_EVENT_SCRIPT_STARTED, /**< Script has started */
    EOS_EVENT_SCRIPT_EXITED, /**< Script has exited */
    EOS_EVENT_ACTIVITY_SCREEN_SWITCHED, /**< Activity page transition completed, param is current activity view */
    EOS_EVENT_LANGUAGE_CHANGED, /**< Language has been changed */
    EOS_EVENT_LAST
} eos_event_code_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Get event ID for user-defined events
 * @return New event ID starting from EOS_EVENT_LAST
 */
eos_event_code_t eos_event_register_id(void);

/**
 * @brief Subscribe to an event (basic version without lv_obj)
 * @param event_id Event ID to subscribe
 * @param cb Callback function to be called when event occurs
 * @param user_data User data passed to callback
 */
void eos_event_subscribe(eos_event_code_t event_id, eos_event_cb_t cb, void *user_data);

/**
 * @brief Subscribe to an event with lv_obj association
 * @param event_id Event ID to subscribe
 * @param cb Callback function to be called when event occurs
 * @param user_data User data passed to callback
 * @param obj lv_obj associated with this subscription (can be NULL, used as payload only)
 */
void eos_event_subscribe_ex(eos_event_code_t event_id, eos_event_cb_t cb, void *user_data, lv_obj_t *obj);

/**
 * @brief Post/Publish an event
 * @param event_id Event ID to post
 * @param param Event parameter
 * @param obj Event target object (can be NULL)
 */
void eos_event_post(eos_event_code_t event_id, void *param, lv_obj_t *obj);

/**
 * @brief Unsubscribe from an event
 * @param event_id Event ID
 * @param cb Callback function to remove
 */
void eos_event_unsubscribe(eos_event_code_t event_id, eos_event_cb_t cb);

/**
 * @brief Unsubscribe from an event with user_data distinction
 * @param event_id Event ID
 * @param cb Callback function to remove
 * @param user_data User data to match
 */
void eos_event_unsubscribe_with_user_data(eos_event_code_t event_id, eos_event_cb_t cb, void *user_data);

/**
 * @brief Unsubscribe all registrations for a specific callback
 * @param cb Callback function
 */
void eos_event_unsubscribe_all(eos_event_cb_t cb);

/**
 * @brief Unsubscribe from an event with obj distinction
 * @param event_id Event ID
 * @param cb Callback function to remove
 * @param obj Event target object to match
 */
void eos_event_unsubscribe_with_obj(eos_event_code_t event_id, eos_event_cb_t cb, lv_obj_t *obj);

/**
 * @brief Allow active cleanup to be triggered externally (e.g., called during system idle)
 */
void eos_event_cleanup_now(void);

/**
 * @brief Get user_data from event
 * @param e Event structure
 * @return user_data
 */
void *eos_event_get_user_data(eos_event_t *e);

/**
 * @brief Get param from event
 * @param e Event structure
 * @return param
 */
void *eos_event_get_param(eos_event_t *e);

/**
 * @brief Get obj from event
 * @param e Event structure
 * @return obj (can be NULL)
 */
lv_obj_t *eos_event_get_obj(eos_event_t *e);

#ifdef __cplusplus
}
#endif

#endif /* EOS_EVENT_H */
