/**
 * @file esh_builtin_hardware.c
 * @brief Hardware diagnostic ESH commands
 */

#include "esh_builtin_commands.h"

/* Includes ---------------------------------------------------*/
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "eos_config.h"
#include "eos_dev_battery.h"
#include "eos_dev_display.h"
#include "eos_dev_microphone.h"
#include "eos_dev_power.h"
#include "eos_dev_sensor.h"
#include "eos_dev_speaker.h"
#include "eos_dev_time.h"
#include "eos_dev_vibrator.h"
#include "eos_port.h"
#include "eos_service_audio.h"
#include "eos_service_battery.h"
#include "eos_service_config.h"
#include "eos_service_pm.h"
#include "eos_service_sensor.h"
#include "eos_touch.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/

/* Function Prototypes ----------------------------------------*/

/* Function Implementations -----------------------------------*/

static const char *_device_state_name(eos_dev_state_t state)
{
    switch (state)
    {
        case DEV_STATE_READY:
            return "ready";
        case DEV_STATE_BUSY:
            return "busy";
        case DEV_STATE_ERROR:
            return "error";
        default:
            return "none";
    }
}

static const char *_sensor_type_name(eos_sensor_type_t type)
{
    static const char *const names[EOS_SENSOR_TYPE_MAX] = {
        [EOS_SENSOR_TYPE_UNKNOWN] = "unknown",
        [EOS_SENSOR_TYPE_ACCE] = "acce",
        [EOS_SENSOR_TYPE_GYRO] = "gyro",
        [EOS_SENSOR_TYPE_HR] = "hr",
        [EOS_SENSOR_TYPE_SPO2] = "spo2",
        [EOS_SENSOR_TYPE_LIGHT] = "light",
        [EOS_SENSOR_TYPE_PROXIMITY] = "proximity",
        [EOS_SENSOR_TYPE_ECG] = "ecg",
        [EOS_SENSOR_TYPE_TEMP] = "temp",
        [EOS_SENSOR_TYPE_MAG] = "mag",
        [EOS_SENSOR_TYPE_BARO] = "baro",
        [EOS_SENSOR_TYPE_CAP] = "cap",
        [EOS_SENSOR_TYPE_STEP] = "step",
    };

    return type < EOS_SENSOR_TYPE_MAX && names[type] ? names[type] : "unknown";
}

static eos_sensor_type_t _parse_sensor_type(const char *name)
{
    eos_sensor_type_t type;

    if (!name)
    {
        return EOS_SENSOR_TYPE_UNKNOWN;
    }

    for (type = EOS_SENSOR_TYPE_ACCE; type < EOS_SENSOR_TYPE_MAX; type++)
    {
        if (strcmp(name, _sensor_type_name(type)) == 0)
        {
            return type;
        }
    }

    if (strcmp(name, "accel") == 0 || strcmp(name, "accelerometer") == 0)
    {
        return EOS_SENSOR_TYPE_ACCE;
    }

    return EOS_SENSOR_TYPE_UNKNOWN;
}

static int _print_sensor_data(esh_cmd_ctx_t *ctx, const eos_sensor_raw_data_t *data)
{
    if (!data)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (esh_printf(ctx, "%s timestamp=%" PRIu32 " ", _sensor_type_name(data->type), data->timestamp) != EOS_OK)
    {
        return EOS_ERR_IO;
    }

    switch (data->type)
    {
        case EOS_SENSOR_TYPE_ACCE:
            return (int)esh_printf(ctx, "x=%d y=%d z=%d\r\n", data->data.acce.x, data->data.acce.y, data->data.acce.z);
        case EOS_SENSOR_TYPE_GYRO:
            return (int)esh_printf(ctx, "x=%d y=%d z=%d\r\n", data->data.gyro.x, data->data.gyro.y, data->data.gyro.z);
        case EOS_SENSOR_TYPE_MAG:
            return (int)esh_printf(ctx, "x=%d y=%d z=%d\r\n", data->data.mag.x, data->data.mag.y, data->data.mag.z);
        case EOS_SENSOR_TYPE_TEMP:
            return (int)esh_printf(ctx, "temp=%" PRId32 "\r\n", data->data.temp.temp);
        case EOS_SENSOR_TYPE_BARO:
            return (int)esh_printf(ctx, "pressure=%" PRId32 "\r\n", data->data.baro.pressure);
        case EOS_SENSOR_TYPE_LIGHT:
            return (int)esh_printf(ctx, "lux=%" PRIu32 "\r\n", data->data.light.lux);
        case EOS_SENSOR_TYPE_PROXIMITY:
            return (int)esh_printf(ctx, "distance=%" PRIu16 " mm\r\n", data->data.proximity.distance_mm);
        case EOS_SENSOR_TYPE_HR:
            return (int)esh_printf(ctx, "heart-rate=%" PRIu16 " bpm\r\n", data->data.hr.heart_rate);
        case EOS_SENSOR_TYPE_SPO2:
            return (int)esh_printf(ctx, "spo2=%" PRIu16 "%%\r\n", data->data.spo2.spo2);
        case EOS_SENSOR_TYPE_ECG:
            return (int)esh_printf(ctx, "ecg=%" PRIu16 "\r\n", data->data.ecg.ecg);
        case EOS_SENSOR_TYPE_CAP:
            return (int)esh_printf(ctx, "cap=%" PRIu16 "\r\n", data->data.cap.cap);
        case EOS_SENSOR_TYPE_STEP:
            return (int)esh_printf(ctx, "steps=%" PRIu32 "\r\n", data->data.step.steps);
        default:
            return (int)esh_printf(ctx, "data unavailable\r\n");
    }
}

int esh_builtin_cmd_sensor(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    eos_dev_sensor_t *dev;
    eos_sensor_type_t type;
    eos_sensor_raw_data_t data;
    const char *action;

    if (!ctx || !argv)
    {
        return EOS_ERR_INVALID_ARG;
    }

    action = argc == 1 ? "list" : argv[1];
    if (strcmp(action, "list") == 0)
    {
        for (dev = eos_dev_sensor_get_list_head(); dev; dev = dev->_next)
        {
            if (esh_printf(ctx,
                           "%s type=%s state=%s period=%" PRIu32 " ms\r\n",
                           dev->name ? dev->name : "(unnamed)",
                           _sensor_type_name(dev->type),
                           _device_state_name(eos_dev_sensor_get_state(dev)),
                           eos_sensor_get_sample_period(dev->type))
                != EOS_OK)
            {
                return EOS_ERR_IO;
            }
        }
        return EOS_OK;
    }

    if (argc < 3)
    {
        return (int)esh_printf(ctx, "sensor: usage: sensor list|read|enable|disable <name>\r\n");
    }

    type = _parse_sensor_type(argv[2]);
    dev = type == EOS_SENSOR_TYPE_UNKNOWN ? eos_dev_sensor_find(argv[2]) : eos_dev_sensor_get_default(type);
    if (!dev)
    {
        return (int)esh_printf(ctx, "sensor: not found: %s\r\n", argv[2]);
    }

    if (strcmp(action, "read") == 0)
    {
        if (eos_sensor_read_latest(dev->type, &data) != EOS_OK)
        {
            return (int)esh_printf(ctx, "sensor: no sample available: %s\r\n", argv[2]);
        }
        if (data.type != dev->type)
        {
            return (int)esh_printf(ctx, "sensor: no sample available: %s\r\n", argv[2]);
        }
        return _print_sensor_data(ctx, &data);
    }

    if (strcmp(action, "enable") == 0 || strcmp(action, "disable") == 0)
    {
        if (!dev->ops || (strcmp(action, "enable") == 0 ? !dev->ops->enable : !dev->ops->disable))
        {
            return (int)esh_printf(ctx, "sensor: operation not supported: %s\r\n", argv[2]);
        }

        if (strcmp(action, "enable") == 0)
        {
            dev->ops->enable(dev);
        }
        else
        {
            dev->ops->disable(dev);
        }
        return EOS_OK;
    }

    return (int)esh_printf(ctx, "sensor: unknown action: %s\r\n", action);
}

int esh_builtin_cmd_battery(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    eos_battery_state_t state;

    (void)argv;

    if (!ctx || argc != 1)
    {
        return (int)esh_printf(ctx, "battery: usage: battery\r\n");
    }

    if (!eos_battery_get_state(&state))
    {
        return (int)esh_printf(ctx, "battery: unavailable\r\n");
    }

    return (int)esh_printf(ctx,
                           "battery: %d%% voltage=%d mV current=%d mA charging=%s capacity=%" PRIu32 "/%" PRIu32
                           " mAh cycles=%" PRIu32 "\r\n",
                           state.percent,
                           state.voltage_mv,
                           state.current_ma,
                           state.charging ? "yes" : "no",
                           eos_battery_get_current_capacity(),
                           eos_battery_get_design_capacity(),
                           eos_battery_get_cycle_count());
}

int esh_builtin_cmd_power(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    eos_dev_power_t *dev;
    const char *pm_state;

    (void)argv;

    if (!ctx || argc != 1)
    {
        return (int)esh_printf(ctx, "power: usage: power\r\n");
    }

    switch (eos_pm_get_state())
    {
        case EOS_PM_DISPLAY_ON:
            pm_state = "display-on";
            break;
        case EOS_PM_DISPLAY_AOD:
            pm_state = "aod";
            break;
        case EOS_PM_SLEEP:
            pm_state = "sleep";
            break;
        default:
            pm_state = "unknown";
            break;
    }

    dev = eos_dev_power_get_instance();
    return (int)esh_printf(ctx,
                           "power: manager=%s device=%s\r\n",
                           pm_state,
                           dev ? _device_state_name(eos_dev_power_get_state()) : "unavailable");
}

int esh_builtin_cmd_display(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    eos_dev_display_t *dev;
    uint8_t brightness;

    (void)argv;

    if (!ctx || argc != 1)
    {
        return (int)esh_printf(ctx, "display: usage: display\r\n");
    }

    dev = eos_dev_display_get_instance();
    brightness = (uint8_t)eos_config_get_number(EOS_CONFIG_KEY_DISPLAY_BRIGHTNESS_NUMBER, 100.0);
    return (int)esh_printf(ctx,
                           "display: state=%s brightness=%" PRIu8 "%% power_ops=%s\r\n",
                           dev ? _device_state_name(eos_dev_display_get_state()) : "unavailable",
                           brightness,
                           dev && dev->ops ? "available" : "unavailable");
}

int esh_builtin_cmd_touch(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    lv_indev_t *indev;
    lv_point_t point;

    (void)argv;

    if (!ctx || argc != 1)
    {
        return (int)esh_printf(ctx, "touch: usage: touch\r\n");
    }

    indev = eos_touch_get_indev();
    if (!indev)
    {
        return (int)esh_printf(ctx, "touch: unavailable\r\n");
    }

    lv_indev_get_point(indev, &point);
    return (int)esh_printf(ctx,
                           "touch: state=%s point=(%d,%d)\r\n",
                           lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED ? "pressed" : "released",
                           point.x,
                           point.y);
}

int esh_builtin_cmd_time(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    eos_dev_time_t *dev;
    eos_datetime_t datetime;

    (void)argv;

    if (!ctx || argc != 1)
    {
        return (int)esh_printf(ctx, "time: usage: time\r\n");
    }

    dev = eos_dev_time_get_instance();
    if (!dev || !dev->ops || !dev->ops->get_datetime)
    {
        return (int)esh_printf(ctx, "time: unavailable\r\n");
    }

    datetime = dev->ops->get_datetime();
    return (int)esh_printf(ctx,
                           "%04" PRIu16 "-%02" PRIu8 "-%02" PRIu8 " %02" PRIu8 ":%02" PRIu8 ":%02" PRIu8 ".%03" PRIu16
                           "\r\n",
                           datetime.year,
                           datetime.month,
                           datetime.day,
                           datetime.hour,
                           datetime.min,
                           datetime.sec,
                           datetime.ms);
}

static bool _parse_u32(const char *text, uint32_t *value)
{
    char *end;
    unsigned long parsed;

    if (!text || !value || text[0] == '\0')
    {
        return false;
    }

    parsed = strtoul(text, &end, 10);
    if (*end != '\0' || parsed > UINT32_MAX)
    {
        return false;
    }

    *value = (uint32_t)parsed;
    return true;
}

int esh_builtin_cmd_vibrator(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    eos_dev_vibrator_t *dev;
    uint32_t duration = 200U;
    uint32_t strength = 128U;

    if (!ctx || !argv || argc < 1 || argc > 4)
    {
        return EOS_ERR_INVALID_ARG;
    }

    dev = eos_dev_vibrator_get_instance();
    if (argc == 1)
    {
        return (int)esh_printf(ctx,
                               "vibrator: state=%s ops=%s\r\n",
                               dev ? _device_state_name(eos_dev_vibrator_get_state()) : "unavailable",
                               dev && dev->ops ? "available" : "unavailable");
    }

    if (strcmp(argv[1], "test") != 0 || (argc > 2 && !_parse_u32(argv[2], &duration))
        || (argc > 3 && !_parse_u32(argv[3], &strength)) || strength > UINT8_MAX)
    {
        return (int)esh_printf(ctx, "vibrator: usage: vibrator [test [duration_ms] [strength]]\r\n");
    }

    if (!dev || !dev->ops || !dev->ops->on || !dev->ops->off)
    {
        return (int)esh_printf(ctx, "vibrator: unavailable\r\n");
    }

    dev->ops->on((uint8_t)strength);
    eos_delay(duration);
    dev->ops->off();
    return EOS_OK;
}

int esh_builtin_cmd_audio(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    eos_result_t result;

    if (!ctx || !argv || argc < 1)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc == 1)
    {
        return (int)esh_printf(ctx,
                               "audio: speaker=%s microphone=%s volume=%" PRIu8 "%% mute=%s\r\n",
                               eos_service_audio_speaker_available() ? "available" : "unavailable",
                               eos_service_audio_microphone_available() ? "available" : "unavailable",
                               eos_service_audio_get_volume(),
                               eos_service_audio_is_muted() ? "yes" : "no");
    }

    if (strcmp(argv[1], "tone") == 0 && argc == 4)
    {
        uint32_t frequency;
        uint32_t duration;

        if (!_parse_u32(argv[2], &frequency) || !_parse_u32(argv[3], &duration) || frequency > UINT16_MAX)
        {
            return (int)esh_printf(ctx, "audio: usage: audio tone <frequency_hz> <duration_ms>\r\n");
        }
        result = eos_service_audio_play_tone((uint16_t)frequency, duration);
    }
    else if (strcmp(argv[1], "play") == 0 && argc == 3)
    {
        char path[256];
        if (!esh_builtin_resolve_path(ctx->esh, argv[2], path, sizeof(path)))
        {
            return (int)esh_printf(ctx, "audio: invalid file path\r\n");
        }
        result = eos_service_audio_play(path);
    }
    else if (strcmp(argv[1], "stop") == 0 && argc == 2)
    {
        result = eos_service_audio_stop();
    }
    else if (strcmp(argv[1], "pause") == 0 && argc == 2)
    {
        result = eos_service_audio_pause();
    }
    else if (strcmp(argv[1], "resume") == 0 && argc == 2)
    {
        result = eos_service_audio_resume();
    }
    else
    {
        return (int)esh_printf(ctx, "audio: usage: audio [tone|play|stop|pause|resume] ...\r\n");
    }

    return result == EOS_OK ? EOS_OK : (int)esh_printf(ctx, "audio: operation failed\r\n");
}

int esh_builtin_cmd_ble(esh_cmd_ctx_t *ctx, int argc, char *argv[])
{
    bool enabled;

    if (!ctx || !argv || argc < 1 || argc > 2)
    {
        return EOS_ERR_INVALID_ARG;
    }

    if (argc == 2 && strcmp(argv[1], "enable") == 0)
    {
        eos_bluetooth_enable();
        return (int)eos_config_set_bool(EOS_CONFIG_KEY_BLUETOOTH_BOOL, true);
    }

    if (argc == 2 && strcmp(argv[1], "disable") == 0)
    {
        eos_bluetooth_disable();
        return (int)eos_config_set_bool(EOS_CONFIG_KEY_BLUETOOTH_BOOL, false);
    }

    if (argc != 1)
    {
        return (int)esh_printf(ctx, "ble: usage: ble [enable|disable]\r\n");
    }

    enabled = eos_config_get_bool(EOS_CONFIG_KEY_BLUETOOTH_BOOL, false);
    return (int)esh_printf(ctx, "ble: configured=%s\r\n", enabled ? "enabled" : "disabled");
}
