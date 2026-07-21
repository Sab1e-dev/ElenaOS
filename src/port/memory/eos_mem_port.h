/**
 * @file eos_mem_port.h
 * @brief Memory allocation
 */

#ifndef EOS_MEM_PORT_H
#define EOS_MEM_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "eos_port.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Memory allocation function
 * @param size Memory size in bytes
 * @return void* Returns memory address if allocation successful, otherwise returns NULL
 */
void *eos_malloc_core(size_t size);
/**
 * @brief Allocate a block of continuous memory and set to 0
 * @param size Memory size in bytes
 * @return void* Returns memory address if allocation successful, otherwise returns NULL
 */
void *eos_malloc_zeroed_core(size_t size);
/**
 * @brief Free target memory
 * @param ptr Target memory pointer
 */
void eos_free_core(void *ptr);
/**
 * @brief Reallocate target memory
 * @param ptr Target memory pointer
 * @param new_size New memory size
 * @return void* Returns memory address if allocation successful, otherwise returns NULL
 */
void *eos_realloc_core(void *ptr, size_t new_size);
#ifdef __cplusplus
}
#endif

#endif /* EOS_MEM_PORT_H */
