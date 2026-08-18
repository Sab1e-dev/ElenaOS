/**
 * @file sni_api_eos_ww.c
 * @brief SNI binding for the watchface widget namespace (eos.ww)
 */

#include "sni_api_eos_ww.h"

/* Includes ---------------------------------------------------*/
#include "sni_type_bridge.h"
#include "eos_ww_digital_clock.h"
#include "eos_ww_date.h"
#include "eos_ww_weekday_ring.h"
#include "eos_ww_moon_phase.h"
#include "eos_ww_date_window.h"
#include "eos_ww_battery.h"
#include "eos_ww_battery_arc.h"
#include "eos_ww_charging.h"
#include "eos_ww_heart_rate.h"
#include "eos_ww_heart_rate_arc.h"
#include "eos_ww_steps.h"
#include "eos_ww_spo2.h"
#include "eos_ww_temperature.h"
#include "eos_ww_activity_rings.h"
#include "eos_ww_barometer.h"
#include "eos_ww_compass.h"
#include "eos_ww_tick_ring.h"
#include "eos_ww_numeral_ring.h"
#include "eos_ww_center_cap.h"
#include "eos_ww_progress_ring.h"
#include "eos_ww_background.h"
#define EOS_LOG_TAG "SNI_WW"
#include "eos_log.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static bool _is_number(const jerry_value_t v)
{
    return jerry_value_is_number(v);
}

#define _GET_PARENT(idx)                                                 \
    lv_obj_t *parent = NULL;                                             \
    if (!sni_tb_js2c_parent(args_p[(idx)], (void **)&parent) || !parent) \
    {                                                                    \
        return sni_api_throw_error("Invalid parent object");             \
    }

#define _RETURN_WIDGET(create_expr)                                \
    do                                                             \
    {                                                              \
        lv_obj_t *obj = (create_expr);                             \
        if (!obj)                                                  \
        {                                                          \
            return sni_api_throw_error("Failed to create widget"); \
        }                                                          \
        return sni_tb_c2js(&obj, SNI_H_LV_OBJ);                    \
    } while (0)

/* Time/date --------------------------------------------------*/

jerry_value_t sni_api_eos_ww_digital_clock(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args_p[],
                                           const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.digitalClock(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_digital_clock_create(parent));
}

jerry_value_t sni_api_eos_ww_date(const jerry_call_info_t *call_info_p,
                                  const jerry_value_t args_p[],
                                  const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.date(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_date_create(parent));
}

jerry_value_t sni_api_eos_ww_weekday_ring(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 3)
    {
        return sni_api_throw_error("Usage: ww.weekdayRing(parent, idleColor, activeColor)");
    }
    _GET_PARENT(0);
    if (!_is_number(args_p[1]) || !_is_number(args_p[2]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    _RETURN_WIDGET(eos_ww_weekday_ring_create(parent,
                                              (uint32_t)jerry_value_as_number(args_p[1]),
                                              (uint32_t)jerry_value_as_number(args_p[2])));
}

jerry_value_t sni_api_eos_ww_moon_phase(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.moonPhase(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_moon_phase_create(parent));
}

jerry_value_t sni_api_eos_ww_date_window(const jerry_call_info_t *call_info_p,
                                         const jerry_value_t args_p[],
                                         const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.dateWindow(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_date_window_create(parent));
}

/* Battery ----------------------------------------------------*/

jerry_value_t sni_api_eos_ww_battery(const jerry_call_info_t *call_info_p,
                                     const jerry_value_t args_p[],
                                     const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.battery(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_battery_create(parent));
}

jerry_value_t sni_api_eos_ww_battery_arc(const jerry_call_info_t *call_info_p,
                                         const jerry_value_t args_p[],
                                         const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 3)
    {
        return sni_api_throw_error("Usage: ww.batteryArc(parent, size, trackColor)");
    }
    _GET_PARENT(0);
    if (!_is_number(args_p[1]) || !_is_number(args_p[2]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    _RETURN_WIDGET(eos_ww_battery_arc_create(parent,
                                             (lv_coord_t)jerry_value_as_number(args_p[1]),
                                             (uint32_t)jerry_value_as_number(args_p[2])));
}

jerry_value_t sni_api_eos_ww_charging(const jerry_call_info_t *call_info_p,
                                      const jerry_value_t args_p[],
                                      const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.charging(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_charging_create(parent));
}

/* Health -----------------------------------------------------*/

jerry_value_t sni_api_eos_ww_heart_rate(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.heartRate(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_heart_rate_create(parent));
}

jerry_value_t sni_api_eos_ww_heart_rate_arc(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 3)
    {
        return sni_api_throw_error("Usage: ww.heartRateArc(parent, size, trackColor)");
    }
    _GET_PARENT(0);
    if (!_is_number(args_p[1]) || !_is_number(args_p[2]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    _RETURN_WIDGET(eos_ww_heart_rate_arc_create(parent,
                                                (lv_coord_t)jerry_value_as_number(args_p[1]),
                                                (uint32_t)jerry_value_as_number(args_p[2])));
}

jerry_value_t sni_api_eos_ww_steps(const jerry_call_info_t *call_info_p,
                                   const jerry_value_t args_p[],
                                   const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.steps(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_steps_create(parent));
}

jerry_value_t sni_api_eos_ww_spo2(const jerry_call_info_t *call_info_p,
                                  const jerry_value_t args_p[],
                                  const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.spo2(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_spo2_create(parent));
}

jerry_value_t sni_api_eos_ww_temperature(const jerry_call_info_t *call_info_p,
                                         const jerry_value_t args_p[],
                                         const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.temperature(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_temperature_create(parent));
}

jerry_value_t sni_api_eos_ww_activity_rings(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 3)
    {
        return sni_api_throw_error("Usage: ww.activityRings(parent, size, trackColor)");
    }
    _GET_PARENT(0);
    if (!_is_number(args_p[1]) || !_is_number(args_p[2]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    _RETURN_WIDGET(eos_ww_activity_rings_create(parent,
                                                (lv_coord_t)jerry_value_as_number(args_p[1]),
                                                (uint32_t)jerry_value_as_number(args_p[2])));
}

jerry_value_t sni_api_eos_ww_barometer(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args_p[],
                                       const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.barometer(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_barometer_create(parent));
}

jerry_value_t sni_api_eos_ww_compass(const jerry_call_info_t *call_info_p,
                                     const jerry_value_t args_p[],
                                     const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 3)
    {
        return sni_api_throw_error("Usage: ww.compass(parent, size, needleColor)");
    }
    _GET_PARENT(0);
    if (!_is_number(args_p[1]) || !_is_number(args_p[2]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    _RETURN_WIDGET(eos_ww_compass_create(parent,
                                         (lv_coord_t)jerry_value_as_number(args_p[1]),
                                         (uint32_t)jerry_value_as_number(args_p[2])));
}

/* Decorative -------------------------------------------------*/

jerry_value_t sni_api_eos_ww_tick_ring(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args_p[],
                                       const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 11)
    {
        return sni_api_throw_error("Usage: ww.tickRing(parent, radius, count, majorEvery, margin, majorLen, minorLen, "
                                   "majorWidth, minorWidth, majorColor, minorColor)");
    }
    _GET_PARENT(0);
    for (jerry_length_t i = 1; i < args_count; i++)
    {
        if (!_is_number(args_p[i]))
        {
            return sni_api_throw_error("Invalid argument type");
        }
    }
    _RETURN_WIDGET(eos_ww_tick_ring_create(parent,
                                           (lv_coord_t)jerry_value_as_number(args_p[1]),
                                           (uint32_t)jerry_value_as_number(args_p[2]),
                                           (uint32_t)jerry_value_as_number(args_p[3]),
                                           (lv_coord_t)jerry_value_as_number(args_p[4]),
                                           (lv_coord_t)jerry_value_as_number(args_p[5]),
                                           (lv_coord_t)jerry_value_as_number(args_p[6]),
                                           (lv_coord_t)jerry_value_as_number(args_p[7]),
                                           (lv_coord_t)jerry_value_as_number(args_p[8]),
                                           (uint32_t)jerry_value_as_number(args_p[9]),
                                           (uint32_t)jerry_value_as_number(args_p[10])));
}

jerry_value_t sni_api_eos_ww_numeral_ring(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 6)
    {
        return sni_api_throw_error("Usage: ww.numeralRing(parent, radius, count, digitRadius, digitSize, color)");
    }
    _GET_PARENT(0);
    if (!_is_number(args_p[1]) || !_is_number(args_p[2]) || !_is_number(args_p[3]) || !_is_number(args_p[4])
        || !_is_number(args_p[5]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    _RETURN_WIDGET(eos_ww_numeral_ring_create(parent,
                                              (lv_coord_t)jerry_value_as_number(args_p[1]),
                                              (uint8_t)jerry_value_as_number(args_p[2]),
                                              (lv_coord_t)jerry_value_as_number(args_p[3]),
                                              (lv_coord_t)jerry_value_as_number(args_p[4]),
                                              (uint32_t)jerry_value_as_number(args_p[5])));
}

jerry_value_t sni_api_eos_ww_center_cap(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 5)
    {
        return sni_api_throw_error("Usage: ww.centerCap(parent, outerD, innerD, outerColor, innerColor)");
    }
    _GET_PARENT(0);
    if (!_is_number(args_p[1]) || !_is_number(args_p[2]) || !_is_number(args_p[3]) || !_is_number(args_p[4]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    _RETURN_WIDGET(eos_ww_center_cap_create(parent,
                                            (lv_coord_t)jerry_value_as_number(args_p[1]),
                                            (lv_coord_t)jerry_value_as_number(args_p[2]),
                                            (uint32_t)jerry_value_as_number(args_p[3]),
                                            (uint32_t)jerry_value_as_number(args_p[4])));
}

jerry_value_t sni_api_eos_ww_progress_ring(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args_p[],
                                           const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 1)
    {
        return sni_api_throw_error("Usage: ww.progressRing(parent)");
    }
    _GET_PARENT(0);
    _RETURN_WIDGET(eos_ww_progress_ring_create(parent));
}

jerry_value_t sni_api_eos_ww_progress_ring_set_value(const jerry_call_info_t *call_info_p,
                                                     const jerry_value_t args_p[],
                                                     const jerry_length_t args_count)
{
    lv_obj_t *arc;
    (void)call_info_p;
    if (args_count != 2)
    {
        return sni_api_throw_error("Usage: ww.progressRingSetValue(ring, value)");
    }
    if (!sni_tb_js2c(args_p[0], SNI_H_LV_OBJ, &arc) || !_is_number(args_p[1]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    eos_ww_progress_ring_set_value(arc, (int32_t)jerry_value_as_number(args_p[1]));
    return jerry_undefined();
}

jerry_value_t sni_api_eos_ww_progress_ring_set_range(const jerry_call_info_t *call_info_p,
                                                     const jerry_value_t args_p[],
                                                     const jerry_length_t args_count)
{
    lv_obj_t *arc;
    (void)call_info_p;
    if (args_count != 3)
    {
        return sni_api_throw_error("Usage: ww.progressRingSetRange(ring, min, max)");
    }
    if (!sni_tb_js2c(args_p[0], SNI_H_LV_OBJ, &arc) || !_is_number(args_p[1]) || !_is_number(args_p[2]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    eos_ww_progress_ring_set_range(arc,
                                   (int32_t)jerry_value_as_number(args_p[1]),
                                   (int32_t)jerry_value_as_number(args_p[2]));
    return jerry_undefined();
}

jerry_value_t sni_api_eos_ww_background(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args_p[],
                                        const jerry_length_t args_count)
{
    (void)call_info_p;
    if (args_count != 5)
    {
        return sni_api_throw_error("Usage: ww.background(parent, width, height, radius, color)");
    }
    _GET_PARENT(0);
    if (!_is_number(args_p[1]) || !_is_number(args_p[2]) || !_is_number(args_p[3]) || !_is_number(args_p[4]))
    {
        return sni_api_throw_error("Invalid argument type");
    }
    _RETURN_WIDGET(eos_ww_background_create(parent,
                                            (lv_coord_t)jerry_value_as_number(args_p[1]),
                                            (lv_coord_t)jerry_value_as_number(args_p[2]),
                                            (lv_coord_t)jerry_value_as_number(args_p[3]),
                                            (uint32_t)jerry_value_as_number(args_p[4])));
}

/* Method table -----------------------------------------------*/

const sni_method_desc_t eos_ww_static_methods[] = {
    {.name = "digitalClock", .handler = sni_api_eos_ww_digital_clock},
    {.name = "date", .handler = sni_api_eos_ww_date},
    {.name = "weekdayRing", .handler = sni_api_eos_ww_weekday_ring},
    {.name = "moonPhase", .handler = sni_api_eos_ww_moon_phase},
    {.name = "dateWindow", .handler = sni_api_eos_ww_date_window},
    {.name = "battery", .handler = sni_api_eos_ww_battery},
    {.name = "batteryArc", .handler = sni_api_eos_ww_battery_arc},
    {.name = "charging", .handler = sni_api_eos_ww_charging},
    {.name = "heartRate", .handler = sni_api_eos_ww_heart_rate},
    {.name = "heartRateArc", .handler = sni_api_eos_ww_heart_rate_arc},
    {.name = "steps", .handler = sni_api_eos_ww_steps},
    {.name = "spo2", .handler = sni_api_eos_ww_spo2},
    {.name = "temperature", .handler = sni_api_eos_ww_temperature},
    {.name = "activityRings", .handler = sni_api_eos_ww_activity_rings},
    {.name = "barometer", .handler = sni_api_eos_ww_barometer},
    {.name = "compass", .handler = sni_api_eos_ww_compass},
    {.name = "tickRing", .handler = sni_api_eos_ww_tick_ring},
    {.name = "numeralRing", .handler = sni_api_eos_ww_numeral_ring},
    {.name = "centerCap", .handler = sni_api_eos_ww_center_cap},
    {.name = "progressRing", .handler = sni_api_eos_ww_progress_ring},
    {.name = "progressRingSetValue", .handler = sni_api_eos_ww_progress_ring_set_value},
    {.name = "progressRingSetRange", .handler = sni_api_eos_ww_progress_ring_set_range},
    {.name = "background", .handler = sni_api_eos_ww_background},
    {.name = NULL, .handler = NULL},
};
