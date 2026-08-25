/**
 * @file eos_dev_display.h
 * @brief Display device
 */

#ifndef EOS_DEV_DISPLAY_H
#define EOS_DEV_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_device.h"
#include "eos_error.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef struct
{
    void (*set_brightness)(uint8_t brightness);
    void (*power_on)(void);
    void (*power_off)(void);
} eos_dev_display_ops_t;

typedef struct
{
    const eos_dev_display_ops_t *ops;
    eos_dev_state_t _state;
} eos_dev_display_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Get display device instance
 * @return Display device instance
 */
eos_dev_display_t *eos_dev_display_get_instance(void);

/**
 * @brief Register display device with OPS
 * @param ops Pointer to display OPS structure
 * @return EOS_OK if successful, error code otherwise
 */
eos_result_t eos_dev_display_register(const eos_dev_display_ops_t *ops);

/**
 * @brief Get display device state
 * @return Current device state
 */
eos_dev_state_t eos_dev_display_get_state(void);

/**
 * @brief Report display device state (called by driver)
 * @param state New device state
 */
void eos_dev_display_report_state(eos_dev_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DEV_DISPLAY_H */
