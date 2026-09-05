/**
 * @file eos_touch.h
 * @brief Get touch device
 */

#ifndef EOS_TOUCH_H
#define EOS_TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/
/**
 * @brief Initialize touch input device configuration
 *
 * Sets scroll_limit and other gesture-related parameters
 * to appropriate values for the target device.
 */
void eos_touch_init(void);

/**
 * @brief Automated input control ownership state.
 */
typedef enum
{
    EOS_TOUCH_CONTROL_UNCONTROLLED = 0,
    EOS_TOUCH_CONTROL_CONTROLLED
} eos_touch_control_state_t;

/**
 * @brief Acquire the screen for automated input.
 * @return true when the screen is controlled after the call.
 */
bool eos_touch_control_acquire(void);

/**
 * @brief Release automated input control and clear pending input.
 */
void eos_touch_control_release(void);

/**
 * @brief Get the current automated input control state.
 */
eos_touch_control_state_t eos_touch_control_get_state(void);

/**
 * @brief Submit the latest logical sample from a platform touch adapter.
 *
 * The platform owns only controller access and coordinate conversion.  EOS
 * owns arbitration between this physical sample and synthetic input, and the
 * LVGL indev adapter reads the result through eos_touch_read().
 *
 * @param x Logical LVGL x coordinate.
 * @param y Logical LVGL y coordinate.
 * @param pressed Whether the physical contact is currently pressed.
 * @return true when the sample was accepted.
 */
bool eos_touch_submit_physical(int32_t x, int32_t y, bool pressed);

/**
 * @brief Read the unified physical/synthetic touch stream for LVGL.
 *
 * The function must be called by the registered LVGL pointer indev callback.
 * It is the only public read path needed by a platform adapter.
 *
 * @param data LVGL indev sample to fill.
 * @return true when a sample was filled.
 */
bool eos_touch_read(lv_indev_data_t *data);

/**
 * @brief Result of a synthetic touch injection request.
 */
typedef enum
{
    EOS_TOUCH_INJECT_OK = 0,
    EOS_TOUCH_INJECT_INVALID,
    EOS_TOUCH_INJECT_BUSY,
    EOS_TOUCH_INJECT_NO_ACTIVE,
    EOS_TOUCH_INJECT_NOT_READY,
    EOS_TOUCH_INJECT_QUEUE_FULL
} eos_touch_inject_result_t;

/**
 * @brief Queue a synthetic press sample for the normal LVGL pointer indev.
 * @param x Logical LVGL x coordinate.
 * @param y Logical LVGL y coordinate.
 * @return Injection result.
 */
eos_touch_inject_result_t eos_touch_inject_down(int32_t x, int32_t y);

/**
 * @brief Queue a synthetic contact/move sample for an active touch.
 * @param x Logical LVGL x coordinate.
 * @param y Logical LVGL y coordinate.
 * @return Injection result.
 */
eos_touch_inject_result_t eos_touch_inject_move(int32_t x, int32_t y);

/**
 * @brief Queue a synthetic release sample for an active touch.
 * @param x Logical LVGL x coordinate.
 * @param y Logical LVGL y coordinate.
 * @return Injection result.
 */
eos_touch_inject_result_t eos_touch_inject_up(int32_t x, int32_t y);

/**
 * @brief Queue a complete press/release sequence.
 * @param x Logical LVGL x coordinate.
 * @param y Logical LVGL y coordinate.
 * @return Injection result.
 */
eos_touch_inject_result_t eos_touch_inject_tap(int32_t x, int32_t y);

/**
 * @brief Queue an interpolated swipe sequence.
 * @param x1 Start logical LVGL x coordinate.
 * @param y1 Start logical LVGL y coordinate.
 * @param x2 End logical LVGL x coordinate.
 * @param y2 End logical LVGL y coordinate.
 * @param duration_ms Approximate contact duration, 1..5000 milliseconds.
 * @return Injection result.
 */
eos_touch_inject_result_t eos_touch_inject_swipe(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t duration_ms);

/**
 * @brief Read one queued synthetic sample from the normal indev path.
 * @param data LVGL indev sample to fill.
 * @return true when a synthetic sample was returned.
 */
bool eos_touch_read_injected(lv_indev_data_t *data);

/**
 * @brief Get the first touch device
 * @return lv_indev_t* Returns touch device pointer if successful, otherwise returns NULL
 */
lv_indev_t *eos_touch_get_indev(void);
#ifdef __cplusplus
}
#endif

#endif /* EOS_TOUCH_H */
