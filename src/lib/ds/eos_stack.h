/**
 * @file eos_stack.h
 * @brief Stack
 */

#ifndef EOS_STACK_H
#define EOS_STACK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
typedef struct eos_stack_t eos_stack_t;

typedef enum
{
    EOS_STACK_CAPACITY_FIXED = 0,
    EOS_STACK_CAPACITY_DYNAMIC,
} eos_stack_capacity_mode_t;
/* Public function prototypes --------------------------------*/

/**
 * @brief Create a stack
 * @param init_capacity Initial capacity, defaults to 4 when 0 is passed
 * @return eos_stack_t* Returns stack pointer if creation succeeds, returns NULL on failure
 */
eos_stack_t *eos_stack_create(size_t init_capacity);

/**
 * @brief Create a stack with specified capacity mode
 * @param init_capacity Initial capacity, defaults to 4 when 0 is passed
 * @param mode Capacity mode: fixed capacity or dynamic capacity
 * @return eos_stack_t* Returns stack pointer if creation succeeds, returns NULL on failure
 */
eos_stack_t *eos_stack_create_with_mode(size_t init_capacity, eos_stack_capacity_mode_t mode);

/**
 * @brief Push to stack
 * @param stack Stack pointer
 * @param data Data pointer
 * @return true Push successful
 * @return false Push failed
 */
bool eos_stack_push(eos_stack_t *stack, void *data);

/**
 * @brief Pop from stack
 * @param stack Stack pointer
 * @return void* Top element pointer, returns NULL if stack is empty or parameter is invalid
 */
void *eos_stack_pop(eos_stack_t *stack);

/**
 * @brief Get top element without popping
 * @param stack Stack pointer
 * @return void* Top element pointer, returns NULL if stack is empty or parameter is invalid
 */
void *eos_stack_peek(eos_stack_t *stack);

/**
 * @brief Get current number of elements in stack
 * @param stack Stack pointer
 * @return size_t Number of elements, returns 0 if parameter is invalid
 */
size_t eos_stack_get_size(eos_stack_t *stack);

/**
 * @brief Get stack capacity mode
 * @param stack Stack pointer
 * @return eos_stack_capacity_mode_t Stack capacity mode, returns fixed mode if parameter is invalid
 */
eos_stack_capacity_mode_t eos_stack_get_capacity_mode(eos_stack_t *stack);

/**
 * @brief Destroy stack and free memory
 * @param stack Stack pointer
 */
void eos_stack_destroy(eos_stack_t *stack);

/**
 * @brief Get element at specified index (0 = bottom, size-1 = top)
 * @param stack Stack pointer
 * @param index Element index (0-based from bottom)
 * @return void* Element pointer at index, returns NULL if index out of bounds or parameter invalid
 */
void *eos_stack_get_at(eos_stack_t *stack, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* EOS_STACK_H */
