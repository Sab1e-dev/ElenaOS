/**
 * @file eos_ww_common.h
 * @brief Common helpers shared by watchface widgets
 */

#ifndef EOS_WW_COMMON_H
#define EOS_WW_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_dev_sensor.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef struct eos_ww_status eos_ww_status_t;

/**
 * @brief Timer-driven update callback
 * @param s Status widget
 */
typedef void (*eos_ww_status_update_cb_t)(eos_ww_status_t *s);

/**
 * @brief Sensor-driven update callback
 * @param s Status widget
 * @param data Latest sensor sample
 */
typedef void (*eos_ww_status_sensor_cb_t)(eos_ww_status_t *s, const eos_sensor_raw_data_t *data);

/**
 * @brief Status widget: a flex container holding an optional icon label and a value label.
 *
 * Used by the simple read-only watchface widgets (digital clock, date,
 * battery, heart rate, steps, …). The widget owns a timer or a sensor
 * subscription and cleans both up automatically when the container is
 * deleted, so callers never need to manage the lifecycle by hand.
 */
struct eos_ww_status
{
    lv_obj_t *container; /**< Root container (returned to the caller) */
    lv_obj_t *icon; /**< Icon label, may be NULL */
    lv_obj_t *value; /**< Value label */
    lv_timer_t *timer; /**< Update timer, NULL when sensor/event-driven */
    eos_ww_status_update_cb_t update_cb; /**< Timer update callback */
    eos_ww_status_sensor_cb_t sensor_cb; /**< Sensor update callback */
    eos_sensor_type_t sensor_type; /**< Subscribed sensor type, UNKNOWN if none */
    void *user_data; /**< Free-form user data */
};

/* Public function prototypes ---------------------------------*/

/**
 * @brief Strip interactivity flags and reset padding/border/shadow on an object
 * @param obj Object to make static
 */
void eos_ww_make_static(lv_obj_t *obj);

/**
 * @brief Delete a timer automatically when the host object is deleted
 * @param host Host object the delete callback is attached to
 * @param timer Timer to delete
 */
void eos_ww_timer_autodelete(lv_obj_t *host, lv_timer_t *timer);

/**
 * @brief Get the 3-letter weekday abbreviation for a day-of-week index
 * @param day_of_week Day index (0 = Sunday .. 6 = Saturday, matching tm_wday)
 * @return Weekday abbreviation (e.g. "SUN")
 */
const char *eos_ww_weekday_abbr(uint8_t day_of_week);

/**
 * @brief Create a status widget (container + optional icon + value label)
 * @param parent Parent object
 * @param icon Icon glyph (e.g. RI_HEART_PULSE_FILL), or NULL for no icon
 * @return Status widget handle, or NULL on failure
 */
eos_ww_status_t *eos_ww_status_create(lv_obj_t *parent, const char *icon);

/**
 * @brief Get the root container of a status widget
 * @param s Status widget
 * @return Container object
 */
lv_obj_t *eos_ww_status_get_container(eos_ww_status_t *s);

/**
 * @brief Get the icon label of a status widget
 * @param s Status widget
 * @return Icon label, or NULL if none
 */
lv_obj_t *eos_ww_status_get_icon(eos_ww_status_t *s);

/**
 * @brief Get the value label of a status widget
 * @param s Status widget
 * @return Value label
 */
lv_obj_t *eos_ww_status_get_value(eos_ww_status_t *s);

/**
 * @brief Set the value label text (printf-style)
 * @param s Status widget
 * @param fmt Format string
 */
void eos_ww_status_set_value(eos_ww_status_t *s, const char *fmt, ...);

/**
 * @brief Set the icon label text
 * @param s Status widget
 * @param icon Icon glyph, or NULL to clear
 */
void eos_ww_status_set_icon(eos_ww_status_t *s, const char *icon);

/**
 * @brief Drive the widget with a periodic timer that calls the update callback
 * @param s Status widget
 * @param cb Update callback
 * @param interval_ms Timer period in milliseconds
 */
void eos_ww_status_start_timer(eos_ww_status_t *s, eos_ww_status_update_cb_t cb, uint32_t interval_ms);

/**
 * @brief Drive the widget with a sensor subscription that calls the callback
 * @param s Status widget
 * @param type Sensor type to subscribe
 * @param cb Update callback
 * @param min_interval_ms Minimum callback interval in milliseconds
 */
void eos_ww_status_start_sensor(eos_ww_status_t *s,
                                eos_sensor_type_t type,
                                eos_ww_status_sensor_cb_t cb,
                                uint32_t min_interval_ms);

#ifdef __cplusplus
}
#endif

#endif /* EOS_WW_COMMON_H */
