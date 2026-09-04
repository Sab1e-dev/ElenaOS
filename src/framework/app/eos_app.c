/**
 * @file eos_app.c
 * @brief Application system
 */

#include "eos_app.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "eos_port.h"
#define EOS_LOG_TAG "App"
#include "eos_log.h"
#include "eos_pkg_mgr.h"
#include "eos_event.h"
#include "script_engine_core.h"
#include "cJSON.h"
#include "eos_app_list.h"
#include "eos_service_storage.h"
#include "eos_mem.h"
#include "eos_activity.h"
#include "eos_lang.h"
#include "eos_theme.h"
#include "eos_std_widgets.h"
#include "eos_accordion.h"
#include "eos_basic_widgets.h"
#include "eos_icon.h"
#include "eos_panel.h"
#include "eos_service_permission.h"
#include "eos_fault_panel.h"
#include "eos_font.h"
#include "eos_watchface.h"
#include "sni_callback_runtime.h"
#include "spm.h"
#include "eos_version.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_APP_LIST_DEFAULT_CAPACITY 1 // Default capacity of application list
/**
 * @brief Application list structure
 *
 * Dynamic array
 */
typedef struct
{
    char **data; /**< Application unique ID */
    size_t size; /**< Number of IDs stored in application list */
    size_t capacity; /**< Capacity of application list */
} eos_app_list_t;
static eos_app_list_t app_list;
static bool app_list_initialized = false;
/* Variables --------------------------------------------------*/

/* Function Implementations -----------------------------------*/

// Application order functions
static eos_result_t _eos_app_order_save(void);
static eos_result_t _eos_app_order_load(void);
static eos_result_t _eos_app_order_add(const char *app_id);
static eos_result_t _eos_app_order_remove(const char *app_id);

// App delete callbacks
static void _app_delete_cb(lv_event_t *e);
static void _app_delete_eos_cb(eos_event_t *e);

// Application order list
static cJSON *app_order_json = NULL;

/**
 * @brief Add application order save function
 */
static eos_result_t _eos_app_order_save(void)
{
    if (!app_order_json)
    {
        return EOS_FAILED;
    }

    // Create a copy of the array for saving to config
    cJSON *app_order_copy = cJSON_Duplicate(app_order_json, true);
    if (!app_order_copy)
    {
        return EOS_FAILED;
    }

    eos_result_t ret = eos_config_set_json(EOS_CONFIG_KEY_APP_ORDER_ARRAY, app_order_copy);

    return ret;
}

/**
 * @brief Add application order load function
 */
static eos_result_t _eos_app_order_load(void)
{
    if (app_order_json)
    {
        cJSON_Delete(app_order_json);
        app_order_json = NULL;
    }

    // Try to load app_order from config
    app_order_json = eos_config_get_json(EOS_CONFIG_KEY_APP_ORDER_ARRAY);

    if (!app_order_json || !cJSON_IsArray(app_order_json))
    {
        // If not found or not an array, create default
        if (app_order_json)
        {
            cJSON_Delete(app_order_json);
        }

        app_order_json = cJSON_CreateArray();
        for (int i = 0; i < EOS_SYS_APP_LAST; i++)
        {
            if (eos_sys_app_id_list[i])
                cJSON_AddItemToArray(app_order_json, cJSON_CreateString(eos_sys_app_id_list[i]));
        }
        return _eos_app_order_save();
    }

    // Ensure all system built-in apps exist in order list
    for (int si = 0; si < EOS_SYS_APP_LAST; si++)
    {
        const char *sys_id = eos_sys_app_id_list[si];
        if (!sys_id)
            continue;
        bool has_sys = false;
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, app_order_json)
        {
            if (cJSON_IsString(item) && strcmp(item->valuestring, sys_id) == 0)
            {
                has_sys = true;
                break;
            }
        }
        if (!has_sys)
        {
            cJSON_AddItemToArray(app_order_json, cJSON_CreateString(sys_id));
            _eos_app_order_save();
        }
    }

    return EOS_OK;
}

// Add application to order list
static eos_result_t _eos_app_order_add(const char *app_id)
{
    if (!app_order_json || !app_id)
    {
        return EOS_FAILED;
    }

    // Check if already exists
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, app_order_json)
    {
        if (cJSON_IsString(item) && strcmp(item->valuestring, app_id) == 0)
        {
            return EOS_OK; // Already exists
        }
    }

    // Add to end of array
    cJSON_AddItemToArray(app_order_json, cJSON_CreateString(app_id));
    return _eos_app_order_save();
}

// Remove application from order list
static eos_result_t _eos_app_order_remove(const char *app_id)
{
    if (!app_order_json || !app_id)
    {
        return EOS_FAILED;
    }

    // System built-in apps cannot be removed
    for (int si = 0; si < EOS_SYS_APP_LAST; si++)
    {
        if (eos_sys_app_id_list[si] && strcmp(app_id, eos_sys_app_id_list[si]) == 0)
        {
            return EOS_OK;
        }
    }

    // Find application position in array
    int index = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, app_order_json)
    {
        if (cJSON_IsString(item) && strcmp(item->valuestring, app_id) == 0)
        {
            // Delete element using index
            cJSON_DeleteItemFromArray(app_order_json, index);
            return _eos_app_order_save();
        }
        index++;
    }

    return EOS_OK; // Not found is also considered success
}

// Move application to specified position
eos_result_t eos_app_order_move(const char *app_id, size_t new_index)
{
    if (!app_order_json || !app_id)
    {
        EOS_LOG_E("input NULL");
        return EOS_FAILED;
    }

    // Get current array size
    size_t array_size = cJSON_GetArraySize(app_order_json);
    if (new_index >= array_size)
    {
        EOS_LOG_E("Out of index");
        return EOS_FAILED; // Index out of range
    }

    // Find current position of application in array
    int current_index = -1;
    cJSON *item = NULL;
    for (int i = 0; i < array_size; i++)
    {
        item = cJSON_GetArrayItem(app_order_json, i);
        if (cJSON_IsString(item) && strcmp(item->valuestring, app_id) == 0)
        {
            current_index = i;
            break;
        }
    }

    if (current_index == -1)
    {
        EOS_LOG_E("App not found");
        return EOS_FAILED; // Application not found
    }

    // If already at specified position, return directly
    if (current_index == new_index)
    {
        EOS_LOG_D("App already in target index");
        return EOS_OK;
    }

    // Remove application
    cJSON *app_item = cJSON_DetachItemFromArray(app_order_json, current_index);

    // Insert at new position
    if (new_index < array_size - 1)
    {
        cJSON_InsertItemInArray(app_order_json, new_index, app_item);
    }
    else
    {
        cJSON_AddItemToArray(app_order_json, app_item);
    }

    return _eos_app_order_save();
}

uint32_t eos_app_get_installed(void)
{
    return app_list.size;
}

const char *eos_app_list_get_id(size_t index)
{
    if (index >= app_list.size)
    {
        EOS_LOG_E("Index out of bounds: %zu", index);
        return NULL;
    }
    return app_list.data[index];
}

bool eos_app_list_contains(const char *app_id)
{
    for (size_t i = 0; i < app_list.size; i++)
    {
        if (strcmp(app_list.data[i], app_id) == 0)
        {
            return true;
        }
    }
    return false;
}

const char *eos_app_list_get_existing_id(const char *id)
{
    for (size_t i = 0; i < app_list.size; i++)
    {
        if (strcmp(app_list.data[i], id) == 0)
        {
            return app_list.data[i];
        }
    }
    return NULL;
}

/**
 * @brief Initialize application list
 */
void _eos_app_list_init(eos_app_list_t *list, size_t capacity)
{
    list->data = eos_malloc(capacity * sizeof(char *));
    list->size = 0;
    list->capacity = capacity;
}

/**
 * @brief Add new application to application list
 */
void _eos_app_list_add(eos_app_list_t *list, const char *id)
{
    if (list->size == list->capacity)
    {
        list->capacity *= 2;
        list->data = eos_realloc(list->data, list->capacity * sizeof(char *));
    }
    list->data[list->size] = eos_strdup(id); // Copy string
    list->size++;
}

/**
 * @brief Free list data
 */
void _eos_app_list_free(eos_app_list_t *list)
{
    for (size_t i = 0; i < list->size; i++)
    {
        eos_free(list->data[i]);
    }
    eos_free(list->data);
}

/**
 * @brief Get installed applications from Flash
 */
eos_result_t _eos_app_list_get_installed(void)
{
    eos_dir_t dir;
    char name_buf[256];

    // Open application installation directory
    dir = eos_storage_dir_open(EOS_APP_INSTALLED_DIR);
    if (dir == NULL)
    {
        EOS_LOG_E("Failed to open app dir: %s", EOS_APP_INSTALLED_DIR);
        return EOS_FAILED;
    }

    // Iterate through all entries in the directory
    while (eos_storage_dir_read(dir, name_buf, sizeof(name_buf)) == EOS_OK)
    {
        // Skip "." and ".." directories
        if (strcmp(name_buf, ".") == 0 || strcmp(name_buf, "..") == 0)
        {
            continue;
        }

        // Build full path
        char full_path[EOS_FS_PATH_MAX];
        snprintf(full_path, sizeof(full_path), EOS_APP_INSTALLED_DIR "%s", name_buf);

        // Check if it is a directory
        if (eos_storage_is_dir(full_path))
        {
            EOS_LOG_D("Found installed app: %s", name_buf);
            _eos_app_list_add(&app_list, name_buf);
        }
    }

    eos_storage_dir_close(dir);

    EOS_LOG_I("Loaded %zu installed apps", app_list.size);
    return EOS_OK;
}

/**
 * @brief Refresh application list
 */
eos_result_t _eos_app_list_refresh()
{
    memset(&app_list, 0, sizeof(app_list));
    _eos_app_list_init(&app_list, EOS_APP_LIST_DEFAULT_CAPACITY);
    eos_result_t installed_ret = _eos_app_list_get_installed();
    if (installed_ret != EOS_OK)
    {
        /* A platform may not have a mounted app filesystem during early
         * bring-up. Keep built-in apps available in that case so the app
         * list is still usable. */
        EOS_LOG_W("Get installed app failed; keeping built-in apps");
    }

    // Add system built-in apps to app_list
    for (int i = 0; i < EOS_SYS_APP_LAST; i++)
    {
        const char *sys_id = eos_sys_app_id_list[i];
        if (!eos_app_list_contains(sys_id))
        {
            _eos_app_list_add(&app_list, sys_id);
        }
    }

    return installed_ret;
}

eos_result_t eos_app_install(const char *eapk_path)
{
    EOS_CHECK_PTR_RETURN_VAL(eapk_path, EOS_ERR_VAR_NULL);
    // Get package header
    eos_pkg_header_t header;
    if (eos_pkg_read_header(eapk_path, &header) != EOS_OK)
    {
        EOS_LOG_E("Read header failed: %s", eapk_path);
        return EOS_FAILED;
    }
    if (!eos_storage_is_valid_filename(header.pkg_id))
    {
        EOS_LOG_E("Invalid package id");
        return EOS_FAILED;
    }
    if (header.min_api_level > ELENIX_OS_API_LEVEL)
    {
        EOS_LOG_E("App '%s' requires API level %d, OS supports %d",
                  header.pkg_id,
                  header.min_api_level,
                  ELENIX_OS_API_LEVEL);
        return EOS_ERR_SDK_VERSION;
    }
    // Concatenate path
    char path[EOS_FS_PATH_MAX];
    snprintf(path, sizeof(path), EOS_APP_INSTALLED_DIR "%s", header.pkg_id);
    char data_path[EOS_FS_PATH_MAX];
    snprintf(data_path, sizeof(data_path), EOS_APP_DATA_DIR "%s", header.pkg_id);
    EOS_LOG_D("APP_PATH: %s", path);
    // Check if app exists
    if (eos_storage_is_dir(path))
    {
        // If exists, delete it
        eos_storage_rm_recursive(path);
    }
    // Create app directory
    if (eos_storage_mkdir_if_not_exist(path) == EOS_OK)
    {
        EOS_LOG_I("Created dir: %s\n", path);
    }
    else
    {
        EOS_LOG_E("Failed mkdir: %s\n", path);
    }
    // Install application
    script_pkg_type_t type = SCRIPT_TYPE_APPLICATION;
    eos_result_t ret = eos_pkg_mgr_unpack(eapk_path, path, type);
    if (ret != EOS_OK)
    {
        EOS_LOG_E("App unpack failed. Code: %d", ret);
        eos_storage_rm_recursive(path);
        return EOS_FAILED;
    }
    eos_storage_mkdir_if_not_exist(data_path);
    // Add to order list
    _eos_app_order_add(header.pkg_id);
    _eos_app_list_refresh();
    EOS_LOG_D("App installed successfully: %s", header.pkg_name);
    const char *app_id = eos_app_list_get_existing_id(header.pkg_id);
    EOS_LOG_D("app_id=%s\npkg_id=%s", app_id, header.pkg_id);
    eos_event_post(EOS_EVENT_APP_INSTALLED, (void *)app_id, NULL);
    return EOS_OK;
}

eos_result_t eos_app_uninstall(const char *app_id)
{
    EOS_LOG_D("Uninstall: %s", app_id);

    char path[EOS_FS_PATH_MAX];
    snprintf(path, sizeof(path), EOS_APP_INSTALLED_DIR "%s", app_id);
    char data_path[EOS_FS_PATH_MAX];
    snprintf(data_path, sizeof(data_path), EOS_APP_DATA_DIR "%s", app_id);

    if (!eos_storage_is_dir(path))
    {
        EOS_LOG_E("App does not exist: %s", app_id);
        return EOS_FAILED;
    }

    eos_result_t ret = eos_storage_rm_recursive(path);

    if (ret != EOS_OK)
    {
        EOS_LOG_E("Uninstall failed, code: %d", ret);
        return EOS_FAILED;
    }

    if (eos_storage_is_dir(data_path))
    {
        ret = eos_storage_rm_recursive(data_path);
        if (ret != EOS_OK)
        {
            EOS_LOG_E("Remove data failed, code: %d", ret);
        }
    }

    _eos_app_order_remove(app_id);
    _eos_app_list_refresh();

    /* Clean up permission grants for this app */
    eos_permission_revoke_all(app_id);

    eos_event_post(EOS_EVENT_APP_UNINSTALLED, (void *)app_id, NULL);

    EOS_LOG_D("App uninstalled successfully: %s", app_id);
    return EOS_OK;
}

static void _app_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    const char *obj_app_id = lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(obj);
    EOS_LOG_D("_app_delete_cb target obj=%p", obj);
    lv_obj_remove_event_cb(obj, _app_delete_cb);
    eos_event_unsubscribe(EOS_EVENT_APP_UNINSTALLED, _app_delete_eos_cb);
}

static void _app_delete_eos_cb(eos_event_t *e)
{
    const char *deleted_app_id = eos_event_get_param(e);
    lv_obj_t *obj = eos_event_get_obj(e);
    const char *obj_app_id = eos_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(obj);
    EOS_LOG_D("_app_delete_eos_cb target obj=%p", obj);
    if (strcmp(deleted_app_id, obj_app_id) == 0)
    {
        lv_obj_remove_event_cb(obj, _app_delete_cb);
        eos_event_unsubscribe(EOS_EVENT_APP_UNINSTALLED, _app_delete_eos_cb);
        lv_obj_delete_async(obj);
    }
}

void eos_app_obj_auto_delete(lv_obj_t *obj, const char *app_id)
{
    EOS_CHECK_PTR_RETURN(obj);
    EOS_LOG_D("Auto del regesited: %s, ptr: %p", app_id, obj);
    lv_obj_add_event_cb(obj, _app_delete_cb, LV_EVENT_DELETE, (void *)app_id);
    eos_event_subscribe_ex(EOS_EVENT_APP_UNINSTALLED, _app_delete_eos_cb, (void *)app_id, obj);
}

eos_result_t eos_app_init(void)
{
    EOS_LOG_D("Init eos_app");
    // Initialize - read application list from file system
    _eos_app_list_refresh();

    // Load application order
    _eos_app_order_load();

    // Clean up non-existent applications in JSON
    if (app_order_json)
    {
        cJSON *item = NULL;
        int index = 0;
        cJSON_ArrayForEach(item, app_order_json)
        {
            if (cJSON_IsString(item))
            {
                const char *app_id = item->valuestring;
                // Skip all system built-in applications
                bool is_sys = false;
                for (int si = 0; si < EOS_SYS_APP_LAST; si++)
                {
                    if (eos_sys_app_id_list[si] && strcmp(app_id, eos_sys_app_id_list[si]) == 0)
                    {
                        is_sys = true;
                        break;
                    }
                }

                if (!is_sys && !eos_app_list_contains(app_id))
                {
                    // Application does not exist, remove from JSON
                    cJSON_DeleteItemFromArray(app_order_json, index);
                    _eos_app_order_save();
                    // Since an element was deleted, need to re-traverse
                    break;
                }
            }
            index++;
        }

        // Automatically add installed applications not in app_order to order list (append to end)
        for (size_t i = 0; i < app_list.size; i++)
        {
            const char *installed_id = app_list.data[i];
            if (installed_id == NULL)
                continue;
            // _eos_app_order_add will skip existing ids internally
            _eos_app_order_add(installed_id);
        }
    }

    /* Register crash recovery handler */
    eos_app_crash_handler_init();

    return EOS_OK;
}

static const char *_eos_script_error_get_reason(eos_script_error_type_t error_type)
{
    switch (error_type)
    {
        case EOS_SCRIPT_FAULT_ERROR_EXCEPTION:
            return "Runtime exception";
        case EOS_SCRIPT_FAULT_ERROR_PARSE:
            return "Parse error";
        case EOS_SCRIPT_FAULT_ERROR_MODULE_LINK:
            return "Module link error";
        case EOS_SCRIPT_FAULT_UNRESPONSIVE:
            return "Unresponsive";
        case EOS_SCRIPT_FAULT_ENGINE_CRASH:
            return "Engine crash";
        default:
            return "Unknown error";
    }
}

void eos_app_handle_script_error(eos_script_error_type_t error_type,
                                 eos_result_t error_code,
                                 const char *app_id,
                                 const eos_script_error_handler_cfg_t *cfg)
{
    EOS_LOG_E("Script error handling - type:%d, code:%d, app_id:%s", error_type, error_code, app_id ? app_id : "NULL");

    const char *reason = _eos_script_error_get_reason(error_type);

    eos_fault_cfg_t fault_cfg = {
        .icon_type = EOS_FAULT_ICON_BUG,
        .icon_color = EOS_COLOR_RED,
        .title_id = STR_ID_ERROR,
        .message_text = eos_lang_get_text(STR_ID_APP_RUN_ERR),
        .confirm_btn_id = cfg && cfg->confirm_btn_id != 0 ? cfg->confirm_btn_id : 0,
        .confirm_btn_text = cfg && cfg->confirm_btn_text ? cfg->confirm_btn_text : NULL,
        .confirm_cb = cfg && cfg->confirm_cb ? cfg->confirm_cb : NULL,
        .cancel_btn_id = cfg && cfg->cancel_btn_id != 0 ? cfg->cancel_btn_id : STR_ID_BACK,
        .cancel_btn_text = cfg && cfg->cancel_btn_text ? cfg->cancel_btn_text : NULL,
        .cancel_cb = cfg && cfg->cancel_cb ? cfg->cancel_cb : NULL,
    };

    eos_activity_t *current = eos_activity_get_current();
    if (!current)
    {
        EOS_LOG_E("No current activity for fault panel");
        return;
    }

    eos_fault_panel_t *old_fp = (eos_fault_panel_t *)eos_activity_get_fault_panel(current);
    if (old_fp)
    {
        EOS_LOG_W("Replacing existing fault panel on activity");
        eos_fault_panel_delete(old_fp);
        eos_activity_set_fault_panel(current, NULL);
    }

    eos_fault_panel_t *fault_panel = eos_fault_panel_create_on_activity(current, &fault_cfg);
    if (!fault_panel)
    {
        EOS_LOG_E("Failed to create fault panel");
        return;
    }

    eos_activity_set_fault_panel(current, fault_panel);

    lv_obj_t *extra_slot = eos_fault_panel_get_extra_slot(fault_panel);
    if (extra_slot)
    {
        char info_str[256];
        const spm_error_t *last_error = spm_get_last_error();
        const char *error_info = last_error ? last_error->error_info : script_engine_get_error_info();
        if (error_info && error_info[0] != '\0')
        {
            snprintf(info_str,
                     sizeof(info_str),
                     "Code: %d\nID: %s\nErrorType: %s\nError: %s",
                     error_code,
                     app_id ? app_id : "Unknown",
                     reason,
                     error_info);
        }
        else
        {
            snprintf(info_str,
                     sizeof(info_str),
                     "Code: %d\nID: %s\nErrorType: %s",
                     error_code,
                     app_id ? app_id : "Unknown",
                     reason);
        }

        lv_obj_t *err_label = lv_label_create(extra_slot);
        lv_label_set_text(err_label, info_str);
        lv_obj_set_width(err_label, EOS_PANEL_CONTENT_WIDTH);
        lv_label_set_long_mode(err_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(err_label, EOS_COLOR_GREY_1, 0);
        eos_label_set_font_size(err_label, EOS_FONT_SIZE_SMALL);

        const script_error_location_t *error_location = NULL;
        uint32_t backtrace_count = 0;
        const script_error_location_t *backtrace = NULL;

        if (last_error)
        {
            error_location = &last_error->error_location;
            backtrace_count = last_error->backtrace_count;
            backtrace = last_error->backtrace;
        }
        else
        {
            error_location = script_engine_get_error_location();
            backtrace_count = script_engine_get_backtrace_count();
            if (backtrace_count > 0)
            {
                backtrace = script_engine_get_error_backtrace(NULL);
            }
        }

        if (error_location && error_location->source_name[0] != '\0')
        {
            char location_str[256];
            snprintf(location_str,
                     sizeof(location_str),
                     "Location: %s:%u:%u",
                     error_location->source_name,
                     error_location->line,
                     error_location->column);

            lv_obj_t *location_label = lv_label_create(extra_slot);
            lv_label_set_text(location_label, location_str);
            lv_obj_set_width(location_label, EOS_PANEL_CONTENT_WIDTH);
            lv_label_set_long_mode(location_label, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_color(location_label, EOS_COLOR_GREY_1, 0);
            eos_label_set_font_size(location_label, EOS_FONT_SIZE_SMALL);
        }

        if (backtrace_count > 0)
        {
            if (backtrace)
            {
                eos_accordion_t *accordion =
                    eos_accordion_create(extra_slot, eos_lang_get_text(STR_ID_APP_RUN_ERR_BACKTRACE));
                lv_obj_t *accordion_content = accordion->content;
                lv_obj_t *backtrace_label = lv_label_create(accordion_content);

                lv_obj_set_style_text_color(accordion->title_label, EOS_COLOR_GREY_1, 0);
                lv_obj_set_style_text_color(accordion->arrow_label, EOS_COLOR_GREY_1, 0);
                lv_obj_set_style_text_color(backtrace_label, EOS_COLOR_GREY_1, 0);

                eos_label_set_font_size(accordion->title_label, EOS_FONT_SIZE_SMALL);
                eos_label_set_font_size(accordion->arrow_label, EOS_FONT_SIZE_SMALL);
                eos_label_set_font_size(backtrace_label, EOS_FONT_SIZE_SMALL);

                char backtrace_str[1024] = {0};
                char temp_str[256];
                for (uint32_t i = 0; i < backtrace_count; i++)
                {
                    snprintf(temp_str,
                             sizeof(temp_str),
                             "#%u: %s:%u:%u\n",
                             i,
                             backtrace[i].source_name,
                             backtrace[i].line,
                             backtrace[i].column);
                    strncat(backtrace_str, temp_str, sizeof(backtrace_str) - strlen(backtrace_str) - 1);
                }
                lv_label_set_text(backtrace_label, backtrace_str);
            }
        }
    }
}

/* Engine Crash Recovery --------------------------------------*/

static void _app_restart_cb(lv_event_t *e)
{
    (void)e;
    const spm_crash_state_t *crash = spm_get_crash_state();
    char app_id[SPM_CRASH_ID_MAX] = {0};

    if (crash && crash->has_crash && crash->script_id[0])
    {
        snprintf(app_id, sizeof(app_id), "%s", crash->script_id);
    }

    spm_clear_crash_state();

    if (app_id[0])
    {
        EOS_LOG_I("Restarting app: %s", app_id);
        eos_activity_t *current = eos_activity_get_current();
        eos_app_restart_in_place(app_id, current);
    }
    else
    {
        EOS_LOG_W("Restart app failed: no app_id available, returning to watchface");
        eos_activity_back_to_watchface();
    }
}

static void _app_exit_cb(lv_event_t *e)
{
    (void)e;
    EOS_LOG_I("Exiting app after crash");
    spm_clear_crash_state();
    /* Try to return to the previous activity (app list) with animation.
     * The activity stack is still intact after engine reset — only JS
     * programs were destroyed.  If that fails (empty stack, already at
     * root, or transition in progress), fall back to watchface. */
    if (eos_activity_back() != EOS_OK)
    {
        EOS_LOG_W("Activity back failed, falling back to watchface");
        eos_activity_back_to_watchface();
    }
}

static void _capture_foreground_script_app_id(char *app_id, size_t app_id_size)
{
    eos_activity_t *current = eos_activity_get_current();
    const char *current_app_id;

    if (!app_id || app_id_size == 0 || !current || eos_activity_get_type(current) != EOS_ACTIVITY_TYPE_APP)
    {
        return;
    }

    current_app_id = eos_activity_get_app_id(current);
    if (current_app_id && current_app_id[0])
    {
        snprintf(app_id, app_id_size, "%s", current_app_id);
    }
}

static void _on_script_fatal(eos_event_t *event)
{
    (void)event;
    const spm_crash_state_t *crash = spm_get_crash_state();
    if (!crash || !crash->has_crash)
    {
        EOS_LOG_W("Script fatal event but no crash context found");
        return;
    }

    EOS_LOG_W("Handling script fatal: type=%d id=%s", crash->script_type, crash->script_id);

    if (crash->script_type == SCRIPT_TYPE_CONSOLE)
    {
        char foreground_app_id[SPM_CRASH_ID_MAX] = {0};

        /* Preserve the foreground script app identity before its Activity is
         * destroyed. The Console crash context only identifies "esh". */
        _capture_foreground_script_app_id(foreground_app_id, sizeof(foreground_app_id));

        /* A fatal error in the shared JerryScript heap destroys every SPM
         * program. The current Activity metadata survives that reset, so
         * remove the stale app stack before any later app command can see it
         * as the foreground application. */
        if (eos_activity_reset_to_root() != EOS_OK)
        {
            EOS_LOG_E("Failed to reset Activity stack after Console engine crash");
            spm_clear_crash_state();
            return;
        }

        /* The JS watchface also lost its program during the global reset.
         * Recreate its Activity and program now that the engine has been
         * initialized again. */
        if (eos_watchface_reload_after_engine_reset() != EOS_OK)
        {
            EOS_LOG_E("Failed to reload watchface after Console engine crash");
            spm_clear_crash_state();
            return;
        }

        spm_clear_crash_state();

        /* Restore the app that was visible before the global reset. This is a
         * fresh Activity and a fresh SPM program; no stale JS handles are
         * resumed. Native/system apps have no script app_id and remain on the
         * recreated watchface. */
        if (foreground_app_id[0])
        {
            EOS_LOG_I("Recreating foreground app after Console engine crash: %s", foreground_app_id);
            if (eos_app_launch_immediately(foreground_app_id) != EOS_OK)
            {
                EOS_LOG_E("Failed to recreate foreground app: %s", foreground_app_id);
            }
        }
        return;
    }

    if (crash->script_type == SCRIPT_TYPE_APPLICATION)
    {
        eos_activity_t *current = eos_activity_get_current();
        if (current && eos_activity_get_type(current) == EOS_ACTIVITY_TYPE_APP)
        {
            eos_script_error_handler_cfg_t cfg = {
                .confirm_btn_id = STR_ID_APP_RESTART,
                .confirm_cb = _app_restart_cb,
                .cancel_btn_id = STR_ID_APP_EXIT,
                .cancel_cb = _app_exit_cb,
            };
            eos_app_handle_script_error(EOS_SCRIPT_FAULT_ENGINE_CRASH,
                                        EOS_ERR_SCRIPT_EXCEPTION,
                                        crash->script_id,
                                        &cfg);
        }
    }
    /* Watchface case is handled in eos_watchface_js.c */

    /* NOTE: Do NOT clear crash state here — the restart/exit callbacks
     * need it when the user presses a button. Callbacks clear it. */
}

void eos_app_crash_handler_init(void)
{
    eos_event_subscribe(EOS_EVENT_SCRIPT_FATAL, _on_script_fatal, NULL);
    EOS_LOG_I("App crash handler initialized");
}
