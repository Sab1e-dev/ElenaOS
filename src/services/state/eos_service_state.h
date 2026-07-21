/**
 * @file eos_service_state.h
 * @brief System state service - manages runtime persistent state
 */

#ifndef EOS_SERVICE_STATE_H
#define EOS_SERVICE_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_error.h"
#include "eos_storage_paths.h"

/* Public macros ----------------------------------------------*/
/** @brief Main state file path */
#define EOS_STATE_FILE_PATH EOS_SYS_DIR "state.json"

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Initialize state service
 */
void eos_service_state_init(void);

/**
 * @brief Set boolean type state item
 * @param key State item key
 * @param value Boolean value
 * @return Operation result
 */
eos_result_t eos_state_set_bool(const char *key, bool value);

/**
 * @brief Set string type state item
 * @param key State item key
 * @param value String value
 * @return Operation result
 */
eos_result_t eos_state_set_string(const char *key, const char *value);

/**
 * @brief Set number type state item
 * @param key State item key
 * @param value Number value
 * @return Operation result
 */
eos_result_t eos_state_set_number(const char *key, double value);

/**
 * @brief Get boolean type state item
 * @param key State item key
 * @param default_value Default value (returned when state item does not exist or type mismatch)
 * @return Retrieved boolean value or default value
 */
bool eos_state_get_bool(const char *key, bool default_value);

/**
 * @brief Get string type state item
 * @param key State item key
 * @param default_value Default value (returned when state item does not exist or type mismatch)
 * @return Retrieved string value or default value
 * @warning The returned string needs to be freed using `eos_free` when no longer needed
 */
char *eos_state_get_string(const char *key, const char *default_value);

/**
 * @brief Get number type state item
 * @param key State item key
 * @param default_value Default value (returned when state item does not exist or type mismatch)
 * @return Retrieved number value or default value
 */
double eos_state_get_number(const char *key, double default_value);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SERVICE_STATE_H */