/**
 * @file eos_service_battery.h
 * @brief Battery service
 */

#ifndef EOS_SERVICE_BATTERY_H
#define EOS_SERVICE_BATTERY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_error.h"
#include "eos_dev_battery.h"
#include "eos_event.h"

/* Public macros ----------------------------------------------*/

/* State keys for persistent storage -----*/
/**
 * @brief State key for battery capacity
 */
#define EOS_STATE_KEY_CAPACITY "battery_capacity"

/**
 * @brief State key for battery cycle count
 */
#define EOS_STATE_KEY_CYCLE_COUNT "cycle_count"

/* Public typedefs --------------------------------------------*/

/**
 * @brief Battery mode enumeration
 */
typedef enum
{
    EOS_BATTERY_MODE_NORMAL = 0,
    EOS_BATTERY_MODE_LOW_POWER,
    EOS_BATTERY_MODE_CHARGING,
    EOS_BATTERY_MODE_ACTIVE,
    EOS_BATTERY_MODE_COUNT,
} eos_battery_mode_t;

/**
 * @brief Battery policy structure
 */
typedef struct
{
    uint32_t interval_ms;
    uint8_t threshold_percent;
} eos_battery_policy_t;

/**
 * @brief Battery raw data from hardware
 *
 * This structure contains the raw battery measurement data
 * as reported by the battery hardware device.
 */
typedef struct
{
    int8_t percent; /**< Battery percentage (0-100), -1 if not available */
    int16_t voltage_mv; /**< Battery voltage in millivolts, -1 if not available */
    int16_t current_ma; /**< Battery current in milliamps, positive=discharge, negative=charge */
    bool charging; /**< true if battery is charging */
} eos_battery_raw_t;

/**
 * @brief Battery processed state
 *
 * This structure contains the processed battery state including
 * timestamp and validity flag.
 */
typedef struct
{
    int8_t percent; /**< Battery percentage (0-100) */
    int16_t voltage_mv; /**< Battery voltage in millivolts */
    int16_t current_ma; /**< Battery current in milliamps */
    bool charging; /**< true if battery is charging */
    uint32_t ts; /**< Timestamp when this state was recorded */
    bool valid; /**< Validity flag, true if this state is valid */
} eos_battery_state_t;

/**
 * @brief Battery percentage calculation callback
 *
 * This callback type is used for custom voltage-to-percentage
 * calculation algorithms.
 *
 * @param voltage_mv Battery voltage in millivolts
 * @return Calculated percentage (0-100), or negative on error
 */
typedef int (*eos_battery_calc_fn_t)(int voltage_mv);

/* Public function prototypes --------------------------------*/

/**
 * @brief Initialize battery service
 *
 * Initializes the battery service and registers for battery update events.
 * This function should be called during system initialization.
 *
 * @return EOS_OK on success, error code on failure
 */
eos_result_t eos_service_battery_init(void);

/**
 * @brief Report raw battery data from device
 *
 * Called by the battery device layer to report new battery measurements.
 * This function is typically called from the request_update callback
 * registered by the battery device.
 *
 * @param raw Pointer to raw battery data
 * @return None
 *
 * @note This function is thread-safe and can be called from interrupt context.
 */
void eos_battery_report_raw(const eos_battery_raw_t *raw);

/**
 * @brief Get current battery percentage
 *
 * @return Battery percentage (0-100), or -1 if not available
 */
int8_t eos_battery_get_percent(void);

/**
 * @brief Get current battery voltage
 *
 * @return Battery voltage in millivolts, or -1 if not available
 */
int16_t eos_battery_get_voltage_mv(void);

/**
 * @brief Check if battery is charging
 *
 * @return true if charging, false otherwise
 */
bool eos_battery_is_charging(void);

/**
 * @brief Get current battery state
 *
 * @param state Pointer to state structure to fill
 * @return true on success, false if service not initialized or state invalid
 */
bool eos_battery_get_state(eos_battery_state_t *state);

/**
 * @brief Get battery design capacity
 *
 * @return Design capacity in mAh
 */
uint32_t eos_battery_get_design_capacity(void);

/**
 * @brief Get battery current capacity
 *
 * @return Current estimated capacity in mAh
 */
uint32_t eos_battery_get_current_capacity(void);

/**
 * @brief Get battery cycle count
 *
 * @return Battery cycle count
 */
uint32_t eos_battery_get_cycle_count(void);

/**
 * @brief Set custom voltage-to-percent calculation function
 *
 * @param fn Custom calculation function, or NULL to use default
 * @return None
 */
void eos_battery_set_calc_fn(eos_battery_calc_fn_t fn);

/**
 * @brief Set policy for a specific battery mode
 *
 * @param mode Battery mode
 * @param policy Pointer to policy structure
 * @return EOS_OK on success, EOS_ERR_INVALID_ARG if mode is invalid
 */
eos_result_t eos_battery_set_policy(eos_battery_mode_t mode, const eos_battery_policy_t *policy);

/**
 * @brief Get policy for a specific battery mode
 *
 * @param mode Battery mode
 * @param policy Pointer to policy structure to fill
 * @return EOS_OK on success, EOS_ERR_INVALID_ARG if mode is invalid
 */
eos_result_t eos_battery_get_policy(eos_battery_mode_t mode, eos_battery_policy_t *policy);

/**
 * @brief Get event ID for battery state changes
 *
 * Use this event ID to register for battery state change notifications.
 * The event will be broadcast when battery percentage or charging state changes.
 *
 * @return Event ID for battery state change events
 */
eos_event_code_t eos_battery_get_event_id(void);

/**
 * @brief Get battery history entry count
 *
 * @return Number of entries in battery history
 */
uint32_t eos_battery_get_history_count(void);

/**
 * @brief Get battery history entry by index
 *
 * @param index History entry index (0 to history_count-1)
 * @param entry Pointer to state structure to fill
 * @return true on success, false on invalid index or service not initialized
 */
bool eos_battery_get_history_entry(uint32_t index, eos_battery_state_t *entry);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SERVICE_BATTERY_H */
