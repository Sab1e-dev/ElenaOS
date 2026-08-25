/**
 * @file eos_dlist.h
 * @brief Doubly linked list
 */

#ifndef EOS_DLIST_H
#define EOS_DLIST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
typedef struct eos_dlist_t eos_dlist_t;

/**
 * @brief List iteration callback
 * @param data Current element data pointer
 * @param user_data User-provided data pointer
 * @return true Continue iteration
 * @return false Stop iteration
 */
typedef bool (*eos_dlist_iter_cb_t)(void *data, void *user_data);

/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a doubly linked list
 * @return eos_dlist_t* Returns list pointer if creation succeeds, otherwise returns NULL
 */
eos_dlist_t *eos_dlist_create(void);

/**
 * @brief Destroy doubly linked list and free all nodes
 * @param list List pointer
 */
void eos_dlist_destroy(eos_dlist_t *list);

/**
 * @brief Insert data at the front of the list
 * @param list List pointer
 * @param data Data pointer to insert
 * @return true Insert successful
 * @return false Insert failed
 */
bool eos_dlist_push_front(eos_dlist_t *list, void *data);

/**
 * @brief Insert data at the back of the list
 * @param list List pointer
 * @param data Data pointer to insert
 * @return true Insert successful
 * @return false Insert failed
 */
bool eos_dlist_push_back(eos_dlist_t *list, void *data);

/**
 * @brief Insert data at specified index
 * @param list List pointer
 * @param index Index to insert at (0-based)
 * @param data Data pointer to insert
 * @return true Insert successful
 * @return false Insert failed (e.g., index out of bounds or allocation failure)
 */
bool eos_dlist_insert_at(eos_dlist_t *list, size_t index, void *data);

/**
 * @brief Remove and return data from the front of the list
 * @param list List pointer
 * @return void* Data pointer of the removed node, returns NULL if list is empty or parameter is invalid
 */
void *eos_dlist_pop_front(eos_dlist_t *list);

/**
 * @brief Remove and return data from the back of the list
 * @param list List pointer
 * @return void* Data pointer of the removed node, returns NULL if list is empty or parameter is invalid
 */
void *eos_dlist_pop_back(eos_dlist_t *list);

/**
 * @brief Remove and return data at specified index
 * @param list List pointer
 * @param index Index to remove (0-based)
 * @return void* Data pointer of the removed node, returns NULL if index out of bounds or parameter is invalid
 */
void *eos_dlist_remove_at(eos_dlist_t *list, size_t index);

/**
 * @brief Remove first node containing the specified data (compares pointer address)
 * @param list List pointer
 * @param data Data pointer to remove
 * @return true Node found and removed
 * @return false Node not found or parameter is invalid
 */
bool eos_dlist_remove(eos_dlist_t *list, void *data);

/**
 * @brief Remove all nodes from the list (does not free data pointers)
 * @param list List pointer
 */
void eos_dlist_clear(eos_dlist_t *list);

/**
 * @brief Get data at the front of the list without removing
 * @param list List pointer
 * @return void* Data pointer at the front, returns NULL if list is empty or parameter is invalid
 */
void *eos_dlist_get_front(eos_dlist_t *list);

/**
 * @brief Get data at the back of the list without removing
 * @param list List pointer
 * @return void* Data pointer at the back, returns NULL if list is empty or parameter is invalid
 */
void *eos_dlist_get_back(eos_dlist_t *list);

/**
 * @brief Get data at specified index without removing
 * @param list List pointer
 * @param index Index (0-based)
 * @return void* Data pointer at the index, returns NULL if out of bounds or parameter is invalid
 */
void *eos_dlist_get_at(eos_dlist_t *list, size_t index);

/**
 * @brief Get current number of elements in the list
 * @param list List pointer
 * @return size_t Number of elements, returns 0 if parameter is invalid
 */
size_t eos_dlist_get_size(eos_dlist_t *list);

/**
 * @brief Check if the list is empty
 * @param list List pointer
 * @return true List is empty or parameter is invalid
 * @return false List is not empty
 */
bool eos_dlist_is_empty(eos_dlist_t *list);

/**
 * @brief Find the first occurrence of data in the list (compares pointer address)
 * @param list List pointer
 * @param data Data pointer to find
 * @return int Index of the found element, returns -1 if not found or parameter is invalid
 */
int eos_dlist_find(eos_dlist_t *list, void *data);

/**
 * @brief Reverse the list in-place
 * @param list List pointer
 * @return true Reverse successful
 * @return false Reverse failed (parameter is invalid)
 */
bool eos_dlist_reverse(eos_dlist_t *list);

/**
 * @brief Iterate through the list and call callback for each element
 * @param list List pointer
 * @param callback Callback function to call for each element
 * @param user_data User data passed to callback
 * @return true All elements iterated successfully
 * @return false Iteration stopped early or parameter is invalid
 */
bool eos_dlist_iterate(eos_dlist_t *list, eos_dlist_iter_cb_t callback, void *user_data);

/**
 * @brief Convert the list to an array
 * @param list List pointer
 * @param out_size Output pointer to receive the array size
 * @return void** Newly allocated array of data pointers, returns NULL on failure
 * @note Caller must free the returned array with eos_free()
 */
void **eos_dlist_to_array(eos_dlist_t *list, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DLIST_H */
