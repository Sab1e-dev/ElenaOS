/**
 * @file eos_darray.h
 * @brief Dynamic array
 */

#ifndef EOS_DARRAY_H
#define EOS_DARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
typedef struct eos_darray_t eos_darray_t;
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a dynamic array and initialize capacity
 * @param init_capacity Initial capacity, defaults to 4 if 0
 * @return eos_darray_t* Returns created dynamic array pointer, returns NULL on failure
 */
eos_darray_t *eos_darray_create(size_t init_capacity);

/**
 * @brief Set data at specified index, automatically expands capacity if index exceeds current capacity
 * @param arr Dynamic array pointer
 * @param index Data index to set
 * @param data Data pointer to set
 * @return true Set successful
 * @return false Set failed (e.g., memory allocation failure or arr is NULL)
 */
bool eos_darray_set(eos_darray_t *arr, size_t index, void *data);

/**
 * @brief Get data at specified index, may automatically shrink capacity if shrink mechanism is enabled
 * @param arr Dynamic array pointer
 * @param index Data index to get
 * @return void* Returns data pointer at corresponding index, returns NULL if out of bounds or arr is NULL
 */
void *eos_darray_get(eos_darray_t *arr, size_t index);

/**
 * @brief Get current number of valid elements in dynamic array
 * @param arr Dynamic array pointer
 * @return size_t Returns current number of valid elements, returns 0 if arr is NULL
 */
size_t eos_darray_get_size(eos_darray_t *arr);

/**
 * @brief Destroy dynamic array and free memory
 * @param arr Dynamic array pointer
 */
void eos_darray_destroy(eos_darray_t *arr);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DARRAY_H */
