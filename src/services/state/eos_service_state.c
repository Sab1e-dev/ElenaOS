/**
 * @file eos_service_state.c
 * @brief System state service implementation
 */

#include "eos_service_state.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eos_service_storage.h"
#include "eos_storage_paths.h"
#define EOS_LOG_TAG "StateService"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/

/* Variables --------------------------------------------------*/
static bool _initialized = false;

/* Function Implementations -----------------------------------*/

void eos_service_state_init(void)
{
    if (_initialized)
    {
        return;
    }

    EOS_LOG_D("Init eos_state");
    eos_storage_mkdir_if_not_exist(EOS_SYS_DIR);
    eos_storage_json_create_if_not_exist(EOS_STATE_FILE_PATH, NULL);

    _initialized = true;
}

eos_result_t eos_state_set_bool(const char *key, bool value)
{
    EOS_CHECK_PTR_RETURN_VAL(key, EOS_ERR_INVALID_ARG);

    EOS_LOG_I("Set state \"%s\" = \"%s\"", key, value ? "true" : "false");
    return eos_storage_json_set_bool(EOS_STATE_FILE_PATH, key, value);
}

eos_result_t eos_state_set_string(const char *key, const char *value)
{
    EOS_CHECK_PTR_RETURN_VAL(key && value, EOS_ERR_INVALID_ARG);

    EOS_LOG_I("Set state \"%s\" = \"%s\"", key, value);
    return eos_storage_json_set_string(EOS_STATE_FILE_PATH, key, value);
}

eos_result_t eos_state_set_number(const char *key, double value)
{
    EOS_CHECK_PTR_RETURN_VAL(key, EOS_ERR_INVALID_ARG);

    EOS_LOG_I("Set state \"%s\" = \"%f\"", key, value);
    return eos_storage_json_set_number(EOS_STATE_FILE_PATH, key, value);
}

bool eos_state_get_bool(const char *key, bool default_value)
{
    return eos_storage_json_get_bool(EOS_STATE_FILE_PATH, key, default_value);
}

char *eos_state_get_string(const char *key, const char *default_value)
{
    return eos_storage_json_get_string(EOS_STATE_FILE_PATH, key, default_value);
}

double eos_state_get_number(const char *key, double default_value)
{
    return eos_storage_json_get_number(EOS_STATE_FILE_PATH, key, default_value);
}