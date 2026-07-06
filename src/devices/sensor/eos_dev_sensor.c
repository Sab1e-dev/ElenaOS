/**
 * @file eos_dev_sensor.c
 * @brief Sensor device implementation
 */

/* Includes ---------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include "eos_dev_sensor.h"
#include "eos_event.h"
#include "eos_port_critical.h"
#include "eos_mem.h"
/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/
static eos_dev_sensor_t *_sensor_list = NULL;
static eos_dev_sensor_t *_default_sensors[EOS_SENSOR_TYPE_MAX] = {NULL};

/* Function Implementations -----------------------------------*/

eos_result_t eos_dev_sensor_register(const char *name, eos_sensor_type_t type, const eos_dev_sensor_ops_t *ops)
{
    if (!name || !ops || type >= EOS_SENSOR_TYPE_MAX || type <= EOS_SENSOR_TYPE_UNKNOWN)
        return EOS_ERR_INVALID_ARG;

    eos_critical_ctx_t ctx = eos_critical_enter();

    eos_dev_sensor_t *iter = _sensor_list;
    while (iter)
    {
        if (strcmp(iter->name, name) == 0)
        {
            eos_critical_leave(ctx);
            return EOS_ERR_ALREADY_EXISTS;
        }
        iter = iter->_next;
    }

    eos_dev_sensor_t *sensor = (eos_dev_sensor_t *)eos_malloc(sizeof(eos_dev_sensor_t));
    if (!sensor)
    {
        eos_critical_leave(ctx);
        return EOS_ERR_MEM;
    }

    memset(sensor, 0, sizeof(eos_dev_sensor_t));
    sensor->name = name;
    sensor->type = type;
    sensor->ops = ops;
    sensor->_state = DEV_STATE_READY;
    sensor->_event_id = eos_event_register_id();
    sensor->_next = _sensor_list;
    _sensor_list = sensor;

    if (_default_sensors[type] == NULL)
    {
        _default_sensors[type] = sensor;
    }

    eos_critical_leave(ctx);
    return EOS_OK;
}

eos_dev_sensor_t *eos_dev_sensor_find(const char *name)
{
    if (!name)
        return NULL;

    eos_critical_ctx_t ctx = eos_critical_enter();

    eos_dev_sensor_t *iter = _sensor_list;
    while (iter)
    {
        if (strcmp(iter->name, name) == 0)
        {
            eos_critical_leave(ctx);
            return iter;
        }
        iter = iter->_next;
    }

    eos_critical_leave(ctx);
    return NULL;
}

eos_dev_sensor_t *eos_dev_sensor_find_by_type(eos_sensor_type_t type)
{
    if (type >= EOS_SENSOR_TYPE_MAX || type <= EOS_SENSOR_TYPE_UNKNOWN)
        return NULL;

    eos_critical_ctx_t ctx = eos_critical_enter();

    eos_dev_sensor_t *iter = _sensor_list;
    while (iter)
    {
        if (iter->type == type)
        {
            eos_critical_leave(ctx);
            return iter;
        }
        iter = iter->_next;
    }

    eos_critical_leave(ctx);
    return NULL;
}

eos_dev_sensor_t *eos_dev_sensor_get_default(eos_sensor_type_t type)
{
    if (type >= EOS_SENSOR_TYPE_MAX || type <= EOS_SENSOR_TYPE_UNKNOWN)
        return NULL;

    return _default_sensors[type];
}

eos_dev_state_t eos_dev_sensor_get_state(eos_dev_sensor_t *dev)
{
    if (!dev)
        return DEV_STATE_NONE;

    eos_critical_ctx_t ctx = eos_critical_enter();
    eos_dev_state_t state = dev->_state;
    eos_critical_leave(ctx);

    return state;
}

void eos_dev_sensor_report_state(eos_dev_sensor_t *dev, eos_dev_state_t state)
{
    if (!dev)
        return;

    eos_critical_ctx_t ctx = eos_critical_enter();
    dev->_state = state;
    eos_critical_leave(ctx);
}

eos_event_code_t eos_dev_sensor_get_event_id(eos_dev_sensor_t *dev)
{
    if (!dev)
        return EOS_EVENT_UNKNOWN;

    return dev->_event_id;
}

eos_sensor_type_t eos_dev_sensor_get_type(eos_dev_sensor_t *dev)
{
    if (!dev)
        return EOS_SENSOR_TYPE_UNKNOWN;

    return dev->type;
}

eos_dev_sensor_t *eos_dev_sensor_get_list_head(void)
{
    return _sensor_list;
}
