/**
 * @file eos_fifo.h
 * @brief Ring buffer FIFO
 */

#ifndef EOS_FIFO_H
#define EOS_FIFO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef struct
{
    uint32_t write_count;
    uint32_t read_count;
    uint32_t overflow_count;
    uint16_t peak_usage;
} eos_fifo_stats_t;

typedef struct eos_fifo_t eos_fifo_t;

/* Public function prototypes ---------------------------------*/

/**
 * @brief Create a FIFO ring buffer
 * @param capacity Buffer capacity in bytes (each element is 1 byte)
 * @return eos_fifo_t* Returns FIFO pointer if creation succeeds, otherwise returns NULL
 */
eos_fifo_t *eos_fifo_create(uint16_t capacity);

/**
 * @brief Destroy FIFO and free all resources
 * @param fifo FIFO pointer
 */
void eos_fifo_destroy(eos_fifo_t *fifo);

/**
 * @brief Reset FIFO to empty state, clears all data and resets stats
 * @param fifo FIFO pointer
 */
void eos_fifo_reset(eos_fifo_t *fifo);

/**
 * @brief Write data to FIFO
 * @param fifo FIFO pointer
 * @param data Pointer to data to write
 * @param size Number of bytes to write
 * @return uint16_t Number of bytes actually written
 * @note May perform partial writes when FIFO is near-full. For multi-byte
 *       entries that must not be torn, prefer eos_fifo_write_atomic().
 */
uint16_t eos_fifo_write(eos_fifo_t *fifo, const void *data, uint16_t size);

/**
 * @brief Atomically write data to FIFO (all-or-nothing)
 * @param fifo FIFO pointer
 * @param data Pointer to data to write
 * @param size Number of bytes to write
 * @return uint16_t Number of bytes written (size on success, 0 if entry
 *         is larger than FIFO capacity)
 * @note If insufficient free space, the oldest entries are dropped to
 *       make room. The write is always atomic — either the full entry
 *       is written or nothing is (if size > capacity).
 */
uint16_t eos_fifo_write_atomic(eos_fifo_t *fifo, const void *data, uint16_t size);

/**
 * @brief Drop oldest entries from FIFO to free space
 * @param fifo FIFO pointer
 * @param count Number of bytes to drop
 */
void eos_fifo_drop(eos_fifo_t *fifo, uint16_t count);

/**
 * @brief Read data from FIFO
 * @param fifo FIFO pointer
 * @param buf Pointer to buffer for reading data
 * @param size Number of bytes to read
 * @return uint16_t Number of bytes actually read
 */
uint16_t eos_fifo_read(eos_fifo_t *fifo, void *buf, uint16_t size);

/**
 * @brief Peek at data from FIFO without removing it
 * @param fifo FIFO pointer
 * @param buf Pointer to buffer for reading data
 * @param size Number of bytes to peek
 * @return uint16_t Number of bytes actually peeked
 */
uint16_t eos_fifo_peek(eos_fifo_t *fifo, void *buf, uint16_t size);

/**
 * @brief Get number of bytes currently in FIFO
 * @param fifo FIFO pointer
 * @return uint16_t Number of bytes in FIFO, returns 0 if parameter is invalid
 */
uint16_t eos_fifo_get_count(eos_fifo_t *fifo);

/**
 * @brief Get remaining free space in FIFO
 * @param fifo FIFO pointer
 * @return uint16_t Number of free bytes
 */
uint16_t eos_fifo_get_free(eos_fifo_t *fifo);

/**
 * @brief Get total capacity of FIFO
 * @param fifo FIFO pointer
 * @return uint16_t Capacity in bytes, returns 0 if parameter is invalid
 */
uint16_t eos_fifo_get_capacity(eos_fifo_t *fifo);

/**
 * @brief Check if FIFO is empty
 * @param fifo FIFO pointer
 * @return true FIFO is empty or parameter is invalid
 * @return false FIFO is not empty
 */
bool eos_fifo_is_empty(eos_fifo_t *fifo);

/**
 * @brief Check if FIFO is full
 * @param fifo FIFO pointer
 * @return true FIFO is full or parameter is invalid
 * @return false FIFO is not full
 */
bool eos_fifo_is_full(eos_fifo_t *fifo);

/**
 * @brief Get FIFO statistics
 * @param fifo FIFO pointer
 * @param stats Pointer to store FIFO statistics, must not be NULL
 */
void eos_fifo_get_stats(eos_fifo_t *fifo, eos_fifo_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* EOS_FIFO_H */
