/**
 * @file eos_cqueue.h
 * @brief Resizable circular queue FIFO
 */

#ifndef EOS_CQUEUE_H
#define EOS_CQUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
typedef struct eos_cqueue_t eos_cqueue_t;
/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a circular queue
 * @param init_capacity Initial capacity
 * @return eos_cqueue_t* Returns circular queue pointer if creation succeeds, otherwise returns NULL
 */
eos_cqueue_t *eos_cqueue_create(size_t init_capacity);
/**
 * @brief Enqueue to circular queue
 * @param cq Target circular queue
 * @param data Data to store
 * @return true Enqueue successful
 * @return false Enqueue failed
 */
bool eos_cqueue_enqueue(eos_cqueue_t *cq, void *data);
/**
 * @brief Dequeue from circular queue
 * @param cq Target circular queue
 * @return void* Returns pointer to dequeued data if successful, otherwise returns NULL
 */
void *eos_cqueue_dequeue(eos_cqueue_t *cq);
/**
 * @brief Destroy circular queue
 */
void eos_cqueue_destroy(eos_cqueue_t *cq);
/**
 * @brief Get circular queue size
 * @param cq Circular queue pointer
 * @return size_t Total number of elements in the queue, returns 0 if queue pointer is invalid
 */
size_t eos_cqueue_get_size(eos_cqueue_t *cq);
/**
 * @brief Read element from circular queue by index
 * @param cq Circular queue pointer
 * @param index Index (0 corresponds to head)
 * @return Queue element pointer, returns NULL if index out of bounds
 */
void *eos_cqueue_peek(eos_cqueue_t *cq, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* EOS_CQUEUE_H */
