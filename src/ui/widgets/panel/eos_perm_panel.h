/**
 * @file eos_perm_panel.h
 * @brief Permission request panel - Android-like runtime permission consent dialog
 */
#ifndef EOS_PERM_PANEL_H
#define EOS_PERM_PANEL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_service_permission.h"

/* Public macros ----------------------------------------------*/

#define EOS_PERM_PANEL_BUTTON_HEIGHT 80

/* Public typedefs --------------------------------------------*/

/**
 * @brief Permission panel configuration
 */
typedef struct
{
    const char *app_name; /**< App display name */
    eos_perm_category_t category; /**< Permission being requested */
    lv_event_cb_t allow_once_cb; /**< "Allow Once" button callback */
    lv_event_cb_t allow_foreground_cb; /**< "Allow While Using App" button callback */
    lv_event_cb_t deny_cb; /**< "Don't Allow" button callback */
} eos_perm_panel_cfg_t;

/**
 * @brief Permission panel structure
 */
typedef struct
{
    lv_obj_t *container; /**< Full-screen overlay container */
    lv_obj_t *title; /**< Title label */
    lv_obj_t *message; /**< Description message */
    lv_obj_t *allow_once_btn; /**< "Allow Once" button */
    lv_obj_t *allow_foreground_btn; /**< "Allow While Using App" button */
    lv_obj_t *deny_btn; /**< "Don't Allow" button */
    eos_perm_panel_cfg_t cfg; /**< Panel configuration copy */
} eos_perm_panel_t;

/* Public function prototypes --------------------------------*/

/**
 * @brief Create a permission request panel on the top layer
 * @param cfg Permission panel configuration
 * @return eos_perm_panel_t* Panel structure, NULL on failure
 * @note Panel is created on lv_layer_top() to avoid animation conflicts
 */
eos_perm_panel_t *eos_perm_panel_create(const eos_perm_panel_cfg_t *cfg);

/**
 * @brief Delete permission panel
 * @param panel Permission panel structure
 */
void eos_perm_panel_delete(eos_perm_panel_t *panel);

#ifdef __cplusplus
}
#endif

#endif /* EOS_PERM_PANEL_H */
