/**
 * @file eos_service_permission.c
 * @brief Permission service implementation
 */
#include "eos_service_permission.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include "cJSON.h"
#include "eos_service_config.h"
#include "eos_event.h"
#include "eos_mem.h"
#define EOS_LOG_TAG "Permission"
#include "eos_log.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_CONFIG_KEY_PERM_GRANTS "perm_grants"

/* Static lookup tables ---------------------------------------*/

/** Permission category string keys (used in manifest and JSON storage) */
static const char *_perm_category_keys[EOS_PERM_CATEGORY_COUNT] = {
    [EOS_PERM_CATEGORY_LOCATION] = "location",
    [EOS_PERM_CATEGORY_SENSOR] = "sensor",
    [EOS_PERM_CATEGORY_NOTIFICATION] = "notification",
    [EOS_PERM_CATEGORY_STORAGE] = "storage",
    [EOS_PERM_CATEGORY_BLUETOOTH] = "bluetooth",
    [EOS_PERM_CATEGORY_AUDIO] = "audio",
    [EOS_PERM_CATEGORY_HEALTH] = "health",
    [EOS_PERM_CATEGORY_CONTACTS] = "contacts",
    [EOS_PERM_CATEGORY_CALENDAR] = "calendar",
};

/** Permission category display name string IDs */
static const lang_string_id_t _perm_category_name_ids[EOS_PERM_CATEGORY_COUNT] = {
    [EOS_PERM_CATEGORY_LOCATION] = STR_ID_PERM_NAME_LOCATION,
    [EOS_PERM_CATEGORY_SENSOR] = STR_ID_PERM_NAME_SENSOR,
    [EOS_PERM_CATEGORY_NOTIFICATION] = STR_ID_PERM_NAME_NOTIFICATION,
    [EOS_PERM_CATEGORY_STORAGE] = STR_ID_PERM_NAME_STORAGE,
    [EOS_PERM_CATEGORY_BLUETOOTH] = STR_ID_PERM_NAME_BLUETOOTH,
    [EOS_PERM_CATEGORY_AUDIO] = STR_ID_PERM_NAME_AUDIO,
    [EOS_PERM_CATEGORY_HEALTH] = STR_ID_PERM_NAME_HEALTH,
    [EOS_PERM_CATEGORY_CONTACTS] = STR_ID_PERM_NAME_CONTACTS,
    [EOS_PERM_CATEGORY_CALENDAR] = STR_ID_PERM_NAME_CALENDAR,
};

/** Permission category description string IDs */
static const lang_string_id_t _perm_category_desc_ids[EOS_PERM_CATEGORY_COUNT] = {
    [EOS_PERM_CATEGORY_LOCATION] = STR_ID_PERM_DESC_LOCATION,
    [EOS_PERM_CATEGORY_SENSOR] = STR_ID_PERM_DESC_SENSOR,
    [EOS_PERM_CATEGORY_NOTIFICATION] = STR_ID_PERM_DESC_NOTIFICATION,
    [EOS_PERM_CATEGORY_STORAGE] = STR_ID_PERM_DESC_STORAGE,
    [EOS_PERM_CATEGORY_BLUETOOTH] = STR_ID_PERM_DESC_BLUETOOTH,
    [EOS_PERM_CATEGORY_AUDIO] = STR_ID_PERM_DESC_AUDIO,
    [EOS_PERM_CATEGORY_HEALTH] = STR_ID_PERM_DESC_HEALTH,
    [EOS_PERM_CATEGORY_CONTACTS] = STR_ID_PERM_DESC_CONTACTS,
    [EOS_PERM_CATEGORY_CALENDAR] = STR_ID_PERM_DESC_CALENDAR,
};

/** Grant state label string IDs */
static const lang_string_id_t _perm_state_label_ids[] = {
    [EOS_PERM_STATE_DENIED] = STR_ID_PERM_STATE_DENIED,
    [EOS_PERM_STATE_ALLOW_ONCE] = STR_ID_PERM_STATE_ALLOW_ONCE,
    [EOS_PERM_STATE_ALLOW_FOREGROUND] = STR_ID_PERM_STATE_ALLOW_FOREGROUND,
    [EOS_PERM_STATE_ALLOW_ALWAYS] = STR_ID_PERM_STATE_ALLOW_ALWAYS,
};

/* Function Implementations -----------------------------------*/

static void _perm_app_uninstalled_cb(eos_event_t *e)
{
    const char *app_id = (const char *)eos_event_get_param(e);
    if (!app_id)
    {
        return;
    }
    EOS_LOG_D("Revoking permissions for uninstalled app: %s", app_id);
    eos_permission_revoke_all(app_id);
}

void eos_service_permission_init(void)
{
    eos_event_subscribe(EOS_EVENT_APP_UNINSTALLED, _perm_app_uninstalled_cb, NULL);
    EOS_LOG_I("Permission service initialized");
}

/* Public API -------------------------------------------------*/

const char *eos_permission_category_key(eos_perm_category_t cat)
{
    if (cat >= EOS_PERM_CATEGORY_COUNT)
        return NULL;
    return _perm_category_keys[cat];
}

lang_string_id_t eos_permission_category_name_id(eos_perm_category_t cat)
{
    if (cat >= EOS_PERM_CATEGORY_COUNT)
        return STR_ID_PERM_NAME_LOCATION;
    return _perm_category_name_ids[cat];
}

lang_string_id_t eos_permission_category_desc_id(eos_perm_category_t cat)
{
    if (cat >= EOS_PERM_CATEGORY_COUNT)
        return STR_ID_PERM_DESC_LOCATION;
    return _perm_category_desc_ids[cat];
}

lang_string_id_t eos_permission_state_label_id(eos_perm_state_t state)
{
    if (state > EOS_PERM_STATE_ALLOW_ALWAYS)
        return STR_ID_PERM_STATE_DENIED;
    return _perm_state_label_ids[state];
}

const char *eos_permission_category_name(eos_perm_category_t cat)
{
    return eos_lang_get_text(eos_permission_category_name_id(cat));
}

const char *eos_permission_category_desc(eos_perm_category_t cat)
{
    return eos_lang_get_text(eos_permission_category_desc_id(cat));
}

eos_perm_category_t eos_permission_name_to_category(const char *name)
{
    if (!name)
        return EOS_PERM_CATEGORY_COUNT;
    for (int i = 0; i < EOS_PERM_CATEGORY_COUNT; i++)
    {
        if (strcmp(name, _perm_category_keys[i]) == 0)
        {
            return (eos_perm_category_t)i;
        }
    }
    return EOS_PERM_CATEGORY_COUNT;
}

eos_perm_state_t eos_permission_get(const char *app_id, eos_perm_category_t cat)
{
    if (!app_id || cat >= EOS_PERM_CATEGORY_COUNT)
    {
        return EOS_PERM_STATE_DENIED;
    }

    const char *cat_key = _perm_category_keys[cat];
    cJSON *grants = eos_config_get_json(EOS_CONFIG_KEY_PERM_GRANTS);
    if (!grants)
    {
        return EOS_PERM_STATE_DENIED;
    }

    cJSON *app_obj = cJSON_GetObjectItemCaseSensitive(grants, app_id);
    if (!app_obj)
    {
        cJSON_Delete(grants);
        return EOS_PERM_STATE_DENIED;
    }

    cJSON *cat_item = cJSON_GetObjectItemCaseSensitive(app_obj, cat_key);
    if (!cat_item || !cJSON_IsNumber(cat_item))
    {
        cJSON_Delete(grants);
        return EOS_PERM_STATE_DENIED;
    }

    int val = cat_item->valueint;
    cJSON_Delete(grants);

    if (val < 0 || val > EOS_PERM_STATE_ALLOW_ALWAYS)
    {
        return EOS_PERM_STATE_DENIED;
    }
    return (eos_perm_state_t)val;
}

bool eos_permission_set(const char *app_id, eos_perm_category_t cat, eos_perm_state_t state)
{
    if (!app_id || cat >= EOS_PERM_CATEGORY_COUNT || state > EOS_PERM_STATE_ALLOW_ALWAYS)
    {
        return false;
    }

    const char *cat_key = _perm_category_keys[cat];
    cJSON *grants = eos_config_get_json(EOS_CONFIG_KEY_PERM_GRANTS);
    if (!grants)
    {
        grants = cJSON_CreateObject();
    }

    /* Ensure the top-level grants object exists */
    cJSON *app_obj = cJSON_GetObjectItemCaseSensitive(grants, app_id);
    if (!app_obj)
    {
        app_obj = cJSON_AddObjectToObject(grants, app_id);
    }

    /* Set/replace the category state */
    cJSON *cat_item = cJSON_GetObjectItemCaseSensitive(app_obj, cat_key);
    if (cat_item)
    {
        cJSON_ReplaceItemInObject(app_obj, cat_key, cJSON_CreateNumber(state));
    }
    else
    {
        cJSON_AddNumberToObject(app_obj, cat_key, state);
    }

    eos_result_t result = eos_config_set_json(EOS_CONFIG_KEY_PERM_GRANTS, grants);
    /* Note: eos_config_set_json takes ownership of grants via AddItem/ReplaceItem,
     * then _eos_config_save_and_broadcast deletes the root tree. Do NOT delete grants here. */
    return result == EOS_OK;
}

void eos_permission_revoke_all(const char *app_id)
{
    if (!app_id)
        return;

    cJSON *grants = eos_config_get_json(EOS_CONFIG_KEY_PERM_GRANTS);
    if (!grants)
        return;

    cJSON_DeleteItemFromObjectCaseSensitive(grants, app_id);
    eos_config_set_json(EOS_CONFIG_KEY_PERM_GRANTS, grants);
    /* Note: eos_config_set_json takes ownership of grants, do not delete here */

    EOS_LOG_D("All permissions revoked for app: %s", app_id);
}
