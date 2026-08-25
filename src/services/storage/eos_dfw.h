/**
 * @file eos_dfw.h
 * @brief Deferred File Writer
 */

#ifndef EOS_DFW_H
#define EOS_DFW_H

#include "eos_config.h"

#if EOS_DFW_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Synchronize buffer to file system
 */
void eos_dfw_sync(void);

/**
 * @brief Write data
 *
 * First search the buffer list for a file with the same path:
 *
 * - If found, write directly to buffer and return
 *
 * - If not found, create a new buffer and add it to the buffer queue
 * @param path Target file path
 * @param data Data
 * @param data_size Data size
 * @return true Task has been added to queue, will be written to file when `eos_dfw_sync()` is called next time
 * @return false Failed
 */
bool eos_dfw_write(const char *path, const uint8_t *data, size_t data_size);

/**
 * @brief Read file data
 * @param path File path
 * @return uint8_t* Returns file content on success, returns NULL on failure
 * @note Need to manually free the read data pointer
 */
uint8_t *eos_dfw_read(const char *path);

/**
 * @brief Initialize deferred file writer
 */
void eos_dfw_init(void);
#ifdef __cplusplus
}
#endif

#endif /* EOS_DFW_ENABLE */

#endif /* EOS_DFW_H */
