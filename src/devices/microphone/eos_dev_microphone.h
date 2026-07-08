/**
 * @file eos_dev_microphone.h
 * @brief Microphone device - PCM capture via shared ring buffer
 *
 * Producer-consumer model: the DMA writes directly into a pre-registered
 * ring buffer.  The service layer polls the DMA write-offset to calculate
 * available data, reads directly from the shared buffer, and tracks its
 * own consumer position.  No inter-layer memcpy required.
 */
#ifndef EOS_DEV_MICROPHONE_H
#define EOS_DEV_MICROPHONE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_device.h"
#include "eos_error.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

typedef struct
{
    int (*init)(void); /**< Optional */
    int (*deinit)(void); /**< Optional */
    int (*open)(uint32_t sample_rate, uint8_t channels, uint8_t bits_per_sample); /**< Required */
    int (*close)(void); /**< Required */
    int (*start)(void); /**< Required: begin DMA capture */
    int (*stop)(void); /**< Required: halt DMA capture */
    int (*set_gain)(uint8_t gain); /**< Optional, 0-100 */
    bool (*is_available)(void); /**< Required */
    /**
     * @brief Register the ring buffer that DMA shall fill.
     * @param buf   Start of buffer (must be DMA-accessible)
     * @param size  Total buffer size in bytes (power-of-2 recommended)
     * @return 0 on success, negative on error
     */
    int (*set_buffer)(uint8_t *buf, uint32_t size); /**< Required */
    /**
     * @brief Return the current DMA write offset (absolute bytes from buf start).
     * Wraps at buffer size.  Thread-safe; may be called from any context.
     * Consumer calculates available bytes from (write - read + size) % size.
     */
    uint32_t (*get_write_offset)(void); /**< Required */
} eos_dev_microphone_ops_t;

typedef struct
{
    const eos_dev_microphone_ops_t *ops;
    eos_dev_state_t _state;
} eos_dev_microphone_t;

/* Public function prototypes --------------------------------*/

eos_dev_microphone_t *eos_dev_microphone_get_instance(void);
eos_result_t eos_dev_microphone_register(const eos_dev_microphone_ops_t *ops);
eos_dev_state_t eos_dev_microphone_get_state(void);
void eos_dev_microphone_report_state(eos_dev_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DEV_MICROPHONE_H */
