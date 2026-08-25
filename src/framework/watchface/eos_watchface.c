/**
 * @file eos_watchface.c
 * @brief Watchface manager
 */

#include "eos_watchface.h"

/* Includes ---------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EOS_LOG_TAG "Watchface"
#include "eos_log.h"
#include "eos_pkg_mgr.h"
#include "eos_watchface_builtin.h"
#include "eos_watchface_js.h"
#include "eos_msg_list.h"
#include "eos_watchface_list.h"
#include "eos_theme.h"
#include "eos_control_center.h"
#include "eos_mem.h"
#include "eos_service_config.h"
#include "eos_service_storage.h"
#include "eos_activity.h"
#include "eos_version.h"

/* Macros and Definitions -------------------------------------*/
#define EOS_WATCHFACE_LIST_DEFAULT_CAPACITY 1

/**
 * @brief Application structure
 */
typedef script_pkg_t eos_watchface_t;

typedef struct
{
    char **data;
    size_t size;
    size_t capacity;
} eos_watchface_list_t;

/* Variables --------------------------------------------------*/
static eos_watchface_list_t watchface_list;
static bool _is_watchface_initialized = false;
static eos_watchface_instance_t *_current_instance = NULL;
/* Function Implementations -----------------------------------*/
static void _switch_to_watchface(const char *watchface_id);

static void _show_global_ui(void)
{
    eos_msg_list_show();
    eos_control_center_show();
}

static void _hide_global_ui(void)
{
    eos_control_center_hide();
    eos_msg_list_hide();
}

size_t eos_watchface_list_size(void)
{
    return watchface_list.size;
}

const char *eos_watchface_list_get_id(size_t index)
{
    if (index >= watchface_list.size)
    {
        EOS_LOG_E("Index out of bounds: %zu", index);
        return NULL;
    }
    return watchface_list.data[index];
}

bool eos_watchface_list_contains(const char *watchface_id)
{
    if (watchface_id && strcmp(watchface_id, EOS_WATCHFACE_BUILTIN_FALLBACK_ID) == 0)
    {
        return true;
    }

    for (size_t i = 0; i < watchface_list.size; i++)
    {
        if (strcmp(watchface_list.data[i], watchface_id) == 0)
        {
            return true;
        }
    }
    return false;
}

void _eos_watchface_list_init(eos_watchface_list_t *list, size_t capacity)
{
    list->data = eos_malloc(capacity * sizeof(char *));
    list->size = 0;
    list->capacity = capacity;
}

void _eos_watchface_list_add(eos_watchface_list_t *list, const char *id)
{
    if (list->size == list->capacity)
    {
        list->capacity *= 2;
        list->data = eos_realloc(list->data, list->capacity * sizeof(char *));
    }
    list->data[list->size] = eos_strdup(id); // Copy string
    list->size++;
}

void _eos_watchface_list_free(eos_watchface_list_t *list)
{
    for (size_t i = 0; i < list->size; i++)
    {
        eos_free(list->data[i]);
    }
    eos_free(list->data);
}

eos_result_t _eos_watchface_list_get_installed()
{
    eos_dir_t dir;
    char name_buf[256];

    // Open application installation directory
    dir = eos_storage_dir_open(EOS_WATCHFACE_INSTALLED_DIR);
    if (!dir)
    {
        EOS_LOG_E("Failed to open watchface directory: %s", EOS_WATCHFACE_INSTALLED_DIR);
        return EOS_OK;
    }

    // Traverse all entries in the directory
    while (eos_storage_dir_read(dir, name_buf, sizeof(name_buf)) == EOS_OK)
    {
        // Skip "." and ".." directories
        if (strcmp(name_buf, ".") == 0 || strcmp(name_buf, "..") == 0)
        {
            continue;
        }

        // Build full path
        char full_path[EOS_FS_PATH_MAX];
        snprintf(full_path, sizeof(full_path), EOS_WATCHFACE_INSTALLED_DIR "%s", name_buf);

        // Check if it is a directory
        if (eos_storage_is_dir(full_path))
        {
            EOS_LOG_D("Found installed watchface: %s", name_buf);
            // Add to application list
            _eos_watchface_list_add(&watchface_list, name_buf);
        }
    }

    eos_storage_dir_close(dir);
    EOS_LOG_I("Loaded %zu installed watchfaces", watchface_list.size);
    return EOS_OK;
}

eos_result_t _eos_watchface_list_refresh()
{
    memset(&watchface_list, 0, sizeof(watchface_list));
    _eos_watchface_list_init(&watchface_list, EOS_WATCHFACE_LIST_DEFAULT_CAPACITY);
    _eos_watchface_list_add(&watchface_list, EOS_WATCHFACE_BUILTIN_FALLBACK_ID);
    if (_eos_watchface_list_get_installed() != EOS_OK)
    {
        EOS_LOG_E("Get installed watchfaces failed");
    }
    return EOS_OK;
}

eos_result_t eos_watchface_install(const char *eapk_path)
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
    if (strcmp(header.pkg_id, EOS_WATCHFACE_BUILTIN_FALLBACK_ID) == 0)
    {
        EOS_LOG_E("Builtin fallback watchface cannot be installed over");
        return EOS_FAILED;
    }
    if (header.min_api_level > ELENIX_OS_API_LEVEL)
    {
        EOS_LOG_E("Watchface '%s' requires API level %d, OS supports %d",
                  header.pkg_id,
                  header.min_api_level,
                  ELENIX_OS_API_LEVEL);
        return EOS_ERR_SDK_VERSION;
    }
    // Construct path
    char path[EOS_FS_PATH_MAX];
    snprintf(path, sizeof(path), EOS_WATCHFACE_INSTALLED_DIR "%s", header.pkg_id);
    char data_path[EOS_FS_PATH_MAX];
    snprintf(data_path, sizeof(data_path), EOS_WATCHFACE_DATA_DIR "%s", header.pkg_id);
    EOS_LOG_D("WATCHFACE_PATH: %s", path);
    // Check if application exists
    if (eos_storage_is_dir(path))
    {
        // If exists, delete it
        eos_storage_rm_recursive(path);
    }
    // Create folder with application name
    if (eos_storage_mkdir_if_not_exist(path) == EOS_OK)
    {
        EOS_LOG_I("Created dir: %s\n", path);
    }
    else
    {
        return EOS_ERR_FILE_ERROR;
    }
    // Install watchface
    script_pkg_type_t type = SCRIPT_TYPE_WATCHFACE;
    eos_result_t ret = eos_pkg_mgr_unpack(eapk_path, path, type);
    if (ret != EOS_OK)
    {
        EOS_LOG_E("Watchface unpack failed. Code: %d", ret);
        eos_storage_rm_recursive(path);
        return EOS_FAILED;
    }
    eos_storage_mkdir_if_not_exist(data_path);
    _eos_watchface_list_refresh();
    EOS_LOG_D("Watchface installed successfully: %s", header.pkg_name);
    return EOS_OK;
}

eos_result_t eos_watchface_uninstall(const char *watchface_id)
{
    if (!watchface_id || strcmp(watchface_id, EOS_WATCHFACE_BUILTIN_FALLBACK_ID) == 0)
    {
        EOS_LOG_E("Builtin fallback watchface cannot be uninstalled");
        return EOS_FAILED;
    }

    // Uninstall watchface
    char path[EOS_FS_PATH_MAX];
    snprintf(path, sizeof(path), EOS_WATCHFACE_INSTALLED_DIR "%s", watchface_id);
    char data_path[EOS_FS_PATH_MAX];
    snprintf(data_path, sizeof(data_path), EOS_WATCHFACE_DATA_DIR "%s", watchface_id);
    if (!eos_storage_is_dir(path))
    {
        EOS_LOG_E("Watchface does not exist: %s", watchface_id);
        return EOS_FAILED;
    }

    eos_result_t ret = eos_storage_rm_recursive(path);

    if (ret != EOS_OK)
    {
        EOS_LOG_E("Uninstall failed, code: %d", ret);
        return EOS_FAILED;
    }

    // Clean up watchface data
    if (eos_storage_is_dir(data_path))
    {
        ret = eos_storage_rm_recursive(data_path);
    }

    if (ret != EOS_OK)
    {
        EOS_LOG_E("Uninstall data failed, code: %d", ret);
        return EOS_FAILED;
    }
    _eos_watchface_list_refresh();
    EOS_LOG_D("Watchface uninstalled successfully: %s", watchface_id);
    return EOS_OK;
}

void eos_watchface_long_pressed_handler(lv_event_t *e)
{
    eos_watchface_list_enter();
}

static void _switch_to_watchface(const char *watchface_id)
{
    EOS_CHECK_PTR_RETURN(watchface_id);

    EOS_LOG_I("Switching to watchface: %s", watchface_id);

    // Create new instance
    eos_watchface_instance_t *new_instance = NULL;

    if (strcmp(watchface_id, EOS_WATCHFACE_BUILTIN_FALLBACK_ID) == 0)
    {
        new_instance = eos_watchface_builtin_create();
    }
    else
    {
        new_instance = eos_watchface_js_create(watchface_id);
    }

    if (!new_instance || !new_instance->activity)
    {
        EOS_LOG_E("Failed to create watchface instance: %s", watchface_id);
        return;
    }

    // Save old instance pointer before replacing
    eos_watchface_instance_t *old_instance = _current_instance;

    // Replace root Activity (this will completely destroy old activity: on_destroy + view + free)
    eos_result_t ret = eos_activity_replace_root(new_instance->activity);
    if (ret != EOS_OK)
    {
        EOS_LOG_E("Failed to replace root activity");
        eos_free(new_instance); // Free new instance on failure
        return;
    }

    // Update current instance and free old instance (activity already freed by replace_root)
    _current_instance = new_instance;

    // Free old instance now (safe because replace_root already cleaned up its activity)
    if (old_instance)
    {
        eos_free(old_instance);
    }
}

eos_activity_t *eos_watchface_get_activity(void)
{
    if (_current_instance)
    {
        return _current_instance->activity;
    }
    return NULL;
}

void eos_watchface_switch_to(const char *watchface_id)
{
    EOS_CHECK_PTR_RETURN(watchface_id);

    EOS_LOG_I("Watchface switching to %s", watchface_id);

    // Save configuration first
    eos_config_set_string(EOS_CONFIG_KEY_WATCHFACE_ID_STR, watchface_id);

    EOS_LOG_I("Watchface Popping watchface list from stack");
    eos_activity_back();

    // Replace root activity with new watchface
    _switch_to_watchface(watchface_id);
}

void eos_watchface_check_and_reload(void)
{
    EOS_CHECK_PTR_RETURN(_current_instance);

    char new_wf_id[EOS_WATCHFACE_ID_LEN_MAX];
    char *selected_wf_id = eos_config_get_string(EOS_CONFIG_KEY_WATCHFACE_ID_STR, "cn.sab1e.clock");
    snprintf(new_wf_id, sizeof(new_wf_id), "%s", selected_wf_id);
    eos_free(selected_wf_id);

    bool changed = strcmp(new_wf_id, _current_instance->id) != 0;

    if (changed)
    {
        EOS_LOG_D("Watchface changed to %s, reloading", new_wf_id);
        _switch_to_watchface(new_wf_id);
    }
}

eos_result_t eos_watchface_init(void)
{
    EOS_LOG_D("Initializing watchface");

    if (!_is_watchface_initialized)
    {
        _eos_watchface_list_refresh();
        _is_watchface_initialized = true;
    }
    else
    {
        EOS_LOG_E("Watchface already initialized");
        return EOS_FAILED;
    }

    // Get configured watchface ID
    char wf_id[EOS_WATCHFACE_ID_LEN_MAX];
    char *selected_wf_id = eos_config_get_string(EOS_CONFIG_KEY_WATCHFACE_ID_STR, "cn.sab1e.clock");
    snprintf(wf_id, sizeof(wf_id), "%s", selected_wf_id);
    eos_free(selected_wf_id);

    // Validate watchface ID
    if (!eos_watchface_list_contains(wf_id))
    {
        EOS_LOG_W("Watchface not found, fallback to builtin: %s", wf_id);
        snprintf(wf_id, sizeof(wf_id), "%s", EOS_WATCHFACE_BUILTIN_FALLBACK_ID);
    }

    // Create initial watchface instance
    if (strcmp(wf_id, EOS_WATCHFACE_BUILTIN_FALLBACK_ID) == 0)
    {
        _current_instance = eos_watchface_builtin_create();
    }
    else
    {
        _current_instance = eos_watchface_js_create(wf_id);
    }

    if (!_current_instance || !_current_instance->activity)
    {
        EOS_LOG_E("Failed to create initial watchface instance");
        return EOS_FAILED;
    }

    EOS_LOG_I("Watchface initialized successfully: %s", wf_id);

    /* Register crash recovery handler for JS watchface callback crashes */
    eos_watchface_js_crash_handler_init();

    return EOS_OK;
}
