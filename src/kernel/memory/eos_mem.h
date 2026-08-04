/**
 * @file eos_mem.h
 * @brief Memory allocation
 */

#ifndef EOS_MEM_H
#define EOS_MEM_H

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
 * @brief Memory allocation function
 * @param size Memory size, unit: bytes
 * @return void* Returns memory address on success, otherwise returns NULL
 */
void *eos_malloc(size_t size);

/**
 * @brief Create a copy of the given string
 * @param s Source string
 * @return char* Returns newly allocated string pointer on success
 *
 * Returns NULL if memory allocation fails
 * @note Use `eos_free(str)` to release the copied string
 */
char *eos_strdup(const char *s);

/**
 * @brief Allocate a block of continuous memory and fill it with zeros
 * @param size Memory size, unit: bytes
 * @return void* Returns memory address on success, otherwise returns NULL
 */
void *eos_malloc_zeroed(size_t size);

/**
 * @brief Free target memory
 * @param ptr Target memory pointer
 */
void eos_free(void *ptr);

/**
 * @brief Reallocate target memory
 * @param ptr Target memory pointer
 * @param new_size New memory size
 * @return void* Returns memory address on success, otherwise returns NULL
 */
void *eos_realloc(void *ptr, size_t new_size);

/**
 * @brief Get total used bytes from tracked C-heap allocations
 * @return size_t Used bytes (0 if memory tracking is disabled)
 */
size_t eos_mem_get_used_bytes(void);

/**
 * @brief Get estimated free bytes (tracked free + port-reported headroom)
 * @return size_t Free bytes estimate
 */
size_t eos_mem_get_free_bytes(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_MEM_H */
