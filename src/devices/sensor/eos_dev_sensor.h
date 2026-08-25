/**
 * @file eos_dev_sensor.h
 * @brief Sensor device header file
 */

#ifndef EOS_DEV_SENSOR_H
#define EOS_DEV_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "eos_device.h"
#include "eos_error.h"
#include "eos_event.h"
#include "lvgl.h"

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/**
 * @brief Sensor type definitions
 */
typedef enum
{
    EOS_SENSOR_TYPE_UNKNOWN = 0,
    EOS_SENSOR_TYPE_ACCE, /**< Accelerometer */
    EOS_SENSOR_TYPE_GYRO, /**< Gyroscope */
    EOS_SENSOR_TYPE_HR, /**< Heart Rate Sensor */
    EOS_SENSOR_TYPE_SPO2, /**< SpO2 Sensor */
    EOS_SENSOR_TYPE_LIGHT, /**< Ambient Light Sensor */
    EOS_SENSOR_TYPE_PROXIMITY, /**< Proximity Sensor */
    EOS_SENSOR_TYPE_ECG, /**< ECG Sensor */
    EOS_SENSOR_TYPE_TEMP, /**< Temperature Sensor */
    EOS_SENSOR_TYPE_MAG, /**< Magnetometer */
    EOS_SENSOR_TYPE_BARO, /**< Barometer */
    EOS_SENSOR_TYPE_CAP, /**< Capacitance Sensor */
    EOS_SENSOR_TYPE_STEP, /**< Step Counter */
    EOS_SENSOR_TYPE_MAX
} eos_sensor_type_t;

/**
 * @brief Accelerometer data
 */
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} eos_sensor_data_acce_t;

/**
 * @brief Gyroscope data
 */
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} eos_sensor_data_gyro_t;

/**
 * @brief Magnetometer data
 */
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} eos_sensor_data_mag_t;

/**
 * @brief Temperature data
 */
typedef struct
{
    int32_t temp;
} eos_sensor_data_temp_t;

/**
 * @brief Barometer data
 */
typedef struct
{
    int32_t pressure;
} eos_sensor_data_baro_t;

/**
 * @brief Light sensor data
 */
typedef struct
{
    uint32_t lux;
} eos_sensor_data_light_t;

/**
 * @brief Proximity sensor data
 */
typedef struct
{
    uint16_t distance_mm;
} eos_sensor_data_proximity_t;

/**
 * @brief Heart rate sensor data
 */
typedef struct
{
    uint16_t heart_rate;
} eos_sensor_data_hr_t;

/**
 * @brief SpO2 sensor data
 */
typedef struct
{
    uint16_t spo2;
} eos_sensor_data_spo2_t;

/**
 * @brief ECG sensor data
 */
typedef struct
{
    uint16_t ecg;
} eos_sensor_data_ecg_t;

/**
 * @brief Capacitance sensor data
 */
typedef struct
{
    uint16_t cap;
} eos_sensor_data_cap_t;

/**
 * @brief Step counter data
 */
typedef struct
{
    uint32_t steps;
} eos_sensor_data_step_t;

/**
 * @brief Battery sensor data
 */
typedef struct
{
    uint8_t level;
    bool charging;
} eos_sensor_data_battery_t;

/**
 * @brief Sensor data union
 */
typedef union
{
    eos_sensor_data_acce_t acce;
    eos_sensor_data_gyro_t gyro;
    eos_sensor_data_mag_t mag;
    eos_sensor_data_temp_t temp;
    eos_sensor_data_baro_t baro;
    eos_sensor_data_light_t light;
    eos_sensor_data_proximity_t proximity;
    eos_sensor_data_hr_t hr;
    eos_sensor_data_spo2_t spo2;
    eos_sensor_data_ecg_t ecg;
    eos_sensor_data_cap_t cap;
    eos_sensor_data_step_t step;
    eos_sensor_data_battery_t battery;
} eos_sensor_data_t;

/**
 * @brief Raw sensor data with timestamp
 */
typedef struct
{
    eos_sensor_type_t type;
    eos_sensor_data_t data;
    uint32_t timestamp;
} eos_sensor_raw_data_t;

typedef struct eos_dev_sensor_t eos_dev_sensor_t;

/**
 * @brief Sensor data-ready callback — called by the device driver when the hardware
 *        FIFO watermark is reached or new data is available via interrupt.
 * @param type  Sensor type that has data ready
 * @param count Number of samples available in the hardware FIFO
 */
typedef void (*eos_sensor_data_ready_cb_t)(eos_sensor_type_t type, uint32_t count);

/**
 * @brief Sensor device operations
 * @note Device layer does NOT provide read operation. Data is pushed to service layer
 *       via eos_sensor_notify(). Service layer manages FIFO and broadcasts to subscribers.
 *
 *       The last three ops are OPTIONAL (NULL = not supported). When not implemented,
 *       the service layer falls back to software-timer-based polling via the port layer.
 */
typedef struct
{
    /* Required ops -----------------------------------------------*/
    void (*init)(eos_dev_sensor_t *dev);
    void (*deinit)(eos_dev_sensor_t *dev);
    void (*enable)(eos_dev_sensor_t *dev);
    void (*disable)(eos_dev_sensor_t *dev);
    void (*set_sample_rate)(eos_dev_sensor_t *dev, uint32_t hz);
    void (*get_sample_rate)(eos_dev_sensor_t *dev, uint32_t *hz);

    /* Optional hardware-FIFO ops (NULL = unsupported) ------------*/
    /**
     * @brief Set the hardware FIFO watermark threshold.
     *        When the sensor's internal FIFO reaches this many samples,
     *        the driver fires the data_ready callback.
     * @param dev     Sensor device
     * @param samples Watermark in number of samples (0 = disable watermark IRQ)
     */
    void (*set_fifo_watermark)(eos_dev_sensor_t *dev, uint16_t samples);

    /**
     * @brief Flush (clear) the hardware FIFO.
     * @param dev Sensor device
     */
    void (*flush_fifo)(eos_dev_sensor_t *dev);

    /**
     * @brief Register a data-ready callback for interrupt-driven batch reads.
     *        When the hardware FIFO watermark is reached, the driver calls `cb`
     *        (from ISR or a deferred task). The service layer then reads batched
     *        data and pushes it through eos_sensor_notify().
     * @param dev Sensor device
     * @param cb  Callback (NULL = unregister)
     */
    void (*set_data_ready_cb)(eos_dev_sensor_t *dev, eos_sensor_data_ready_cb_t cb);
} eos_dev_sensor_ops_t;

/**
 * @brief Sensor device structure
 */
struct eos_dev_sensor_t
{
    const eos_dev_sensor_ops_t *ops;
    const char *name;
    eos_sensor_type_t type;
    eos_dev_state_t _state;
    eos_event_code_t _event_id;
    struct eos_dev_sensor_t *_next;
};

/* Public function prototypes ---------------------------------*/

/**
 * @brief Register a sensor device
 * @param name Device name
 * @param type Sensor type
 * @param ops Device operations pointer
 * @return eos_result_t Operation result
 */
eos_result_t eos_dev_sensor_register(const char *name, eos_sensor_type_t type, const eos_dev_sensor_ops_t *ops);

/**
 * @brief Find sensor device by name
 * @param name Device name
 * @return eos_dev_sensor_t* Device pointer or NULL
 */
eos_dev_sensor_t *eos_dev_sensor_find(const char *name);

/**
 * @brief Find sensor device by type
 * @param type Sensor type
 * @return eos_dev_sensor_t* Device pointer or NULL
 */
eos_dev_sensor_t *eos_dev_sensor_find_by_type(eos_sensor_type_t type);

/**
 * @brief Get default sensor device by type
 * @param type Sensor type
 * @return eos_dev_sensor_t* Device pointer or NULL
 */
eos_dev_sensor_t *eos_dev_sensor_get_default(eos_sensor_type_t type);

/**
 * @brief Get sensor device state
 * @param dev Device pointer
 * @return eos_dev_state_t Device state
 */
eos_dev_state_t eos_dev_sensor_get_state(eos_dev_sensor_t *dev);

/**
 * @brief Report sensor device state
 * @param dev Device pointer
 * @param state New state
 */
void eos_dev_sensor_report_state(eos_dev_sensor_t *dev, eos_dev_state_t state);

/**
 * @brief Get sensor device event ID
 * @param dev Device pointer
 * @return eos_event_code_t Event ID
 */
eos_event_code_t eos_dev_sensor_get_event_id(eos_dev_sensor_t *dev);

/**
 * @brief Get sensor device type
 * @param dev Device pointer
 * @return eos_sensor_type_t Sensor type
 */
eos_sensor_type_t eos_dev_sensor_get_type(eos_dev_sensor_t *dev);

/**
 * @brief Get the head of sensor list for iteration
 * @return eos_dev_sensor_t* Pointer to the first sensor in the list
 */
eos_dev_sensor_t *eos_dev_sensor_get_list_head(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DEV_SENSOR_H */
