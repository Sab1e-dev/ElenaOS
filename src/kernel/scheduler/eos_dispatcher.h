/**
 * @file eos_dispatcher.h
 * @brief Task dispatcher
 */

#ifndef EOS_DISPATCHER_H
#define EOS_DISPATCHER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
typedef void (*eos_dispatcher_cb_t)(void *); /**< Callback definition */
/* Public function prototypes ---------------------------------*/
/**
 * @brief Initialize task dispatcher (must be called before GUI thread starts or during initialization phase)
 */
void eos_dispatcher_init(void);
/**
 * @brief Add callback to callback queue
 * @param cb Callback function
 * @param user_data User data
 * @note Can be called from any thread or ISR, returns as quickly as possible.
 */
bool eos_dispatcher_try_call(eos_dispatcher_cb_t cb, void *user_data);
/**
 * @brief Add a callback to the callback queue and ignore a full-queue result
 * @param cb Callback function
 * @param user_data Callback user data
 */
void eos_dispatcher_call(eos_dispatcher_cb_t cb, void *user_data);
/**
 * @brief Main handler, called periodically in GUI thread loop to execute all callbacks
 */
void eos_dispatch_tick(void);
#ifdef __cplusplus
}
#endif

#endif /* EOS_DISPATCHER_H */
