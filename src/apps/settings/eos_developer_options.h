/**
 * @file eos_developer_options.h
 * @brief Developer options features (OBJS display, touch tracking, FPS monitor)
 */

#ifndef EOS_DEVELOPER_OPTIONS_H
#define EOS_DEVELOPER_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize developer options system (called once at startup)
 */
void eos_developer_options_init(void);

/**
 * @brief Update developer options overlays (called each main loop iteration)
 */
void eos_developer_options_update(void);

/**
 * @brief Enable or disable FPS display overlay
 * @param enabled true to show, false to hide
 */
void eos_developer_options_set_fps_enabled(bool enabled);

/**
 * @brief Get FPS display enabled state
 * @return true if enabled, false otherwise
 */
bool eos_developer_options_get_fps_enabled(void);

/**
 * @brief Enable or disable OBJS display overlay
 * @param enabled true to show, false to hide
 */
void eos_developer_options_set_objs_enabled(bool enabled);

/**
 * @brief Get OBJS display enabled state
 * @return true if enabled, false otherwise
 */
bool eos_developer_options_get_objs_enabled(void);

/**
 * @brief Enable or disable touch coordinate display overlay
 * @param enabled true to show, false to hide
 */
void eos_developer_options_set_touch_enabled(bool enabled);

/**
 * @brief Get touch coordinate display enabled state
 * @return true if enabled, false otherwise
 */
bool eos_developer_options_get_touch_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DEVELOPER_OPTIONS_H */
