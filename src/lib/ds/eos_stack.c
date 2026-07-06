/**
 * @file eos_stack.c
 * @brief Stack
 */

#include "eos_stack.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#define EOS_LOG_TAG "Stack"
#include "eos_log.h"
#include "eos_mem.h"

/* Macros and Definitions -------------------------------------*/
#define _SHRINK_ENABLE 1
#define _SHRINK_THRESHOLD (4)
#define _SHRINK_PROPORTION (2)
#define _CAPACITY_GROWTH (2)

struct eos_stack_t
{
    void **buffer;
    size_t size;
    size_t capacity;
    size_t min_capacity;
    eos_stack_capacity_mode_t mode;
};

/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

static bool _stack_expand(eos_stack_t *stack)
{
    size_t new_capacity = stack->capacity * _CAPACITY_GROWTH;
    void **new_buffer = eos_malloc_zeroed(new_capacity * sizeof(void *));
    EOS_CHECK_PTR_RETURN_VAL(new_buffer, false);

    for (size_t i = 0; i < stack->size; i++)
    {
        new_buffer[i] = stack->buffer[i];
    }

    eos_free(stack->buffer);
    stack->buffer = new_buffer;
    EOS_LOG_I("stack expanded: %zu -> %zu", stack->capacity, new_capacity);
    stack->capacity = new_capacity;
    return true;
}

static bool _stack_shrink(eos_stack_t *stack)
{
    EOS_CHECK_PTR_RETURN_VAL(stack, false);

    if (stack->capacity <= stack->min_capacity)
        return true;
    if (stack->size > stack->capacity / _SHRINK_THRESHOLD)
        return true;

    size_t new_capacity = stack->capacity / _SHRINK_PROPORTION;
    if (new_capacity < stack->min_capacity)
        new_capacity = stack->min_capacity;

    void **new_buffer = eos_malloc_zeroed(new_capacity * sizeof(void *));
    EOS_CHECK_PTR_RETURN_VAL(new_buffer, false);

    for (size_t i = 0; i < stack->size; i++)
    {
        new_buffer[i] = stack->buffer[i];
    }

    eos_free(stack->buffer);
    stack->buffer = new_buffer;
    EOS_LOG_I("stack shrinked: %zu -> %zu", stack->capacity, new_capacity);
    stack->capacity = new_capacity;
    return true;
}

eos_stack_t *eos_stack_create(size_t init_capacity)
{
    return eos_stack_create_with_mode(init_capacity, EOS_STACK_CAPACITY_DYNAMIC);
}

eos_stack_t *eos_stack_create_with_mode(size_t init_capacity, eos_stack_capacity_mode_t mode)
{
    if (init_capacity == 0)
        init_capacity = 4;

    eos_stack_t *stack = eos_malloc_zeroed(sizeof(eos_stack_t));
    EOS_CHECK_PTR_RETURN_VAL(stack, NULL);

    stack->buffer = eos_malloc_zeroed(init_capacity * sizeof(void *));
    EOS_CHECK_PTR_RETURN_VAL_FREE(stack->buffer, NULL, stack);

    stack->size = 0;
    stack->capacity = init_capacity;
    stack->min_capacity = init_capacity;
    stack->mode = mode;
    EOS_LOG_I("stack created, mode=%s", mode == EOS_STACK_CAPACITY_DYNAMIC ? "dynamic" : "fixed");
    return stack;
}

bool eos_stack_push(eos_stack_t *stack, void *data)
{
    EOS_CHECK_PTR_RETURN_VAL(stack, false);

    if (stack->size == stack->capacity)
    {
        if (stack->mode == EOS_STACK_CAPACITY_FIXED)
        {
            EOS_LOG_W("Push failed: fixed-capacity stack is full");
            return false;
        }

        if (!_stack_expand(stack))
            return false;
    }

    stack->buffer[stack->size++] = data;
    EOS_LOG_I("Push data[%p]", data);
    return true;
}

void *eos_stack_pop(eos_stack_t *stack)
{
    EOS_CHECK_PTR_RETURN_VAL(stack, NULL);
    if (stack->size == 0)
        return NULL;

    void *data = stack->buffer[stack->size - 1];
    stack->buffer[stack->size - 1] = NULL;
    stack->size--;

#if _SHRINK_ENABLE
    if (stack->mode == EOS_STACK_CAPACITY_DYNAMIC)
        _stack_shrink(stack);
#endif

    EOS_LOG_I("Pop data[%p]", data);
    return data;
}

void *eos_stack_peek(eos_stack_t *stack)
{
    EOS_CHECK_PTR_RETURN_VAL(stack, NULL);
    if (stack->size == 0)
        return NULL;

    return stack->buffer[stack->size - 1];
}

size_t eos_stack_get_size(eos_stack_t *stack)
{
    EOS_CHECK_PTR_RETURN_VAL(stack, 0);
    return stack->size;
}

eos_stack_capacity_mode_t eos_stack_get_capacity_mode(eos_stack_t *stack)
{
    EOS_CHECK_PTR_RETURN_VAL(stack, EOS_STACK_CAPACITY_FIXED);
    return stack->mode;
}

void eos_stack_destroy(eos_stack_t *stack)
{
    EOS_CHECK_PTR_RETURN(stack);
    eos_free(stack->buffer);
    eos_free(stack);
}

void *eos_stack_get_at(eos_stack_t *stack, size_t index)
{
    EOS_CHECK_PTR_RETURN_VAL(stack, NULL);
    if (index >= stack->size)
    {
        return NULL;
    }
    return stack->buffer[index];
}
