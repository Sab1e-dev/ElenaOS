/**
 * @file sni_api_eos_permission.c
 * @brief SNI permission API implementation
 *
 * eos.permission.request(perm_name, callback)  - async, shows consent panel
 * eos.permission.check(perm_name)              - sync, returns grant state string
 *
 * Multiple simultaneous requests are queued and processed one at a time.
 * Requests for a category that is already active or queued are silently
 * dropped (no duplicate panels).
 */
#include "sni_api_eos_permission.h"

/* Includes ---------------------------------------------------*/
#include <string.h>
#include "sni_type_bridge.h"
#include "eos_service_permission.h"
#include "eos_perm_panel.h"
#include "eos_activity.h"
#include "script_engine_core.h"
#include "spm.h"
#include "eos_dispatcher.h"
#include "eos_mem.h"
#include "eos_log.h"

/* ---- Active request (panel currently shown) ---- */
typedef struct
{
    bool active;
    jerry_value_t callback; /* JS callback to invoke */
    eos_perm_category_t category;
    char *app_id; /* Copy of app ID for button callbacks */
    char *app_name; /* Copy of app name for panel display */
} perm_request_ctx_t;

/* ---- Queue node for pending requests ---- */
typedef struct perm_queue_node
{
    struct perm_queue_node *next;
    jerry_value_t callback;
    eos_perm_category_t category;
    char *app_id;
    char *app_name;
} perm_queue_node_t;

/* ---- Dispatcher context for async already-granted callback ---- */
typedef struct
{
    jerry_value_t cb;
    char *app_id;
    const char *result_str;
} async_granted_ctx_t;

static perm_request_ctx_t _perm_req = {0};
static perm_queue_node_t *_perm_queue_head = NULL;
static perm_queue_node_t *_perm_queue_tail = NULL;

/* ---- Forward declarations ---- */
static void _invoke_callback(jerry_value_t cb, const char *result, const char *app_id);
static void _cleanup_request(void);
static void _dequeue_and_show(void);
static bool _queue_has_category(eos_perm_category_t cat);
static void _queue_clear(void);
static void _perm_allow_once_cb(lv_event_t *e);
static void _perm_allow_foreground_cb(lv_event_t *e);
static void _perm_deny_cb(lv_event_t *e);
static void _async_granted_cb(void *user_data);
static void _schedule_async_granted(jerry_value_t callback, const char *app_id, const char *result_str);

/* ---- Helper: convert string to category ---- */

static eos_perm_category_t _resolve_category(const char *name)
{
    eos_perm_category_t cat = eos_permission_name_to_category(name);
    if (cat >= EOS_PERM_CATEGORY_COUNT)
    {
        EOS_LOG_W("Unknown permission name: %s", name);
    }
    return cat;
}

/* ---- Helper: convert grant state to JS string ---- */

static const char *_state_to_string(eos_perm_state_t state)
{
    switch (state)
    {
        case EOS_PERM_STATE_ALLOW_ONCE:
            return "once";
        case EOS_PERM_STATE_ALLOW_FOREGROUND:
            return "foreground";
        case EOS_PERM_STATE_ALLOW_ALWAYS:
            return "always";
        case EOS_PERM_STATE_DENIED:
        default:
            return "denied";
    }
}

/* ---- Queue helpers ---- */

static bool _queue_has_category(eos_perm_category_t cat)
{
    if (_perm_req.active && _perm_req.category == cat)
        return true;

    perm_queue_node_t *node = _perm_queue_head;
    while (node)
    {
        if (node->category == cat)
            return true;
        node = node->next;
    }
    return false;
}

static void _queue_clear(void)
{
    perm_queue_node_t *node = _perm_queue_head;
    while (node)
    {
        perm_queue_node_t *next = node->next;
        if (!jerry_value_is_undefined(node->callback))
            jerry_value_free(node->callback);
        if (node->app_id)
            eos_free(node->app_id);
        if (node->app_name)
            eos_free(node->app_name);
        eos_free(node);
        node = next;
    }
    _perm_queue_head = NULL;
    _perm_queue_tail = NULL;
}

static bool _enqueue(eos_perm_category_t cat, const char *app_id, const char *app_name, jerry_value_t callback)
{
    perm_queue_node_t *node = eos_malloc_zeroed(sizeof(perm_queue_node_t));
    if (!node)
        return false;

    node->category = cat;
    node->app_id = eos_strdup(app_id);
    node->app_name = eos_strdup(app_name);
    node->callback = jerry_value_copy(callback);

    if (_perm_queue_tail)
    {
        _perm_queue_tail->next = node;
        _perm_queue_tail = node;
    }
    else
    {
        _perm_queue_head = _perm_queue_tail = node;
    }

    EOS_LOG_D("Permission request queued: app=%s cat=%d queue_depth>=1", app_id, cat);
    return true;
}

/* Caller takes ownership of the returned node (must free after use) */
static perm_queue_node_t *_dequeue(void)
{
    if (!_perm_queue_head)
        return NULL;

    perm_queue_node_t *node = _perm_queue_head;
    _perm_queue_head = node->next;
    if (!_perm_queue_head)
        _perm_queue_tail = NULL;
    node->next = NULL;
    return node;
}

/* ---- Panel lifecycle ---- */

static void _show_panel(void)
{
    eos_perm_panel_cfg_t cfg = {
        .app_name = _perm_req.app_name,
        .category = _perm_req.category,
        .allow_once_cb = _perm_allow_once_cb,
        .allow_foreground_cb = _perm_allow_foreground_cb,
        .deny_cb = _perm_deny_cb,
    };

    eos_perm_panel_t *panel = eos_perm_panel_create(&cfg);
    if (!panel)
    {
        EOS_LOG_E("Failed to create permission panel for app=%s cat=%d", _perm_req.app_id, _perm_req.category);
        _cleanup_request();
        _dequeue_and_show();
        return;
    }

    EOS_LOG_D("Permission panel shown: app=%s cat=%d", _perm_req.app_id, _perm_req.category);
}

/**
 * @brief Dismiss current panel, then show the next queued request (if any).
 *
 * Must be called AFTER _cleanup_request() so _perm_req is free.
 */
static void _dequeue_and_show(void)
{
    perm_queue_node_t *node = _dequeue();
    if (!node)
        return;

    /* Populate _perm_req from queue node */
    _perm_req.active = true;
    _perm_req.category = node->category;
    _perm_req.callback = node->callback; /* transfer ownership */
    _perm_req.app_id = node->app_id; /* transfer ownership */
    _perm_req.app_name = node->app_name; /* transfer ownership */

    eos_free(node); /* free the node shell, not the transferred fields */

    _show_panel();
}

/* ---- Internal: JS callback invocation ---- */

static void _async_granted_cb(void *user_data)
{
    async_granted_ctx_t *ctx = (async_granted_ctx_t *)user_data;
    if (!ctx)
        return;

    _invoke_callback(ctx->cb, ctx->result_str, ctx->app_id);
    if (!jerry_value_is_undefined(ctx->cb))
        jerry_value_free(ctx->cb);
    if (ctx->app_id)
        eos_free(ctx->app_id);
    eos_free(ctx);
}

static void _schedule_async_granted(jerry_value_t callback, const char *app_id, const char *result_str)
{
    async_granted_ctx_t *ctx = eos_malloc_zeroed(sizeof(async_granted_ctx_t));
    if (!ctx)
        return;

    ctx->cb = jerry_value_copy(callback);
    ctx->app_id = eos_strdup(app_id);
    ctx->result_str = result_str;

    eos_dispatcher_call(_async_granted_cb, ctx);
}

static void _invoke_callback(jerry_value_t cb, const char *result, const char *app_id)
{
    if (jerry_value_is_undefined(cb))
        return;

    script_program_t *prog = spm_get_program_by_id(app_id);
    if (!prog)
        return;

    jerry_value_t result_val = jerry_string_sz(result);
    jerry_value_t ret = spm_call(prog, cb, jerry_undefined(), &result_val, 1);
    jerry_value_free(result_val);

    if (jerry_value_is_error(ret))
    {
        EOS_LOG_E("Error invoking permission JS callback");
        jerry_value_free(ret);
    }
}

static void _cleanup_request(void)
{
    if (!_perm_req.active)
        return;

    if (!jerry_value_is_undefined(_perm_req.callback))
    {
        jerry_value_free(_perm_req.callback);
    }
    if (_perm_req.app_id)
    {
        eos_free(_perm_req.app_id);
    }
    if (_perm_req.app_name)
    {
        eos_free(_perm_req.app_name);
    }
    memset(&_perm_req, 0, sizeof(_perm_req));
}

/* ---- Public API handlers ---- */

jerry_value_t sni_api_eos_permission_request(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args_p[],
                                             const jerry_length_t args_count)
{
    (void)call_info_p;

    /* Validate arguments: request(perm_name, callback) */
    if (args_count < 2 || !jerry_value_is_string(args_p[0]) || !jerry_value_is_function(args_p[1]))
    {
        return sni_api_throw_error("Usage: permission.request(permName, callback)");
    }

    /* Get the calling app's ID and name (internal pointers, do NOT free) */
    const char *app_id = script_engine_get_current_script_id();
    if (!app_id)
    {
        return sni_api_throw_error("No current script context");
    }

    const char *app_name = script_engine_get_current_script_name();
    if (!app_name)
    {
        app_name = app_id;
    }

    /* Resolve permission name */
    jerry_size_t name_len = jerry_string_size(args_p[0], JERRY_ENCODING_UTF8);
    char *perm_name = eos_malloc(name_len + 1);
    if (!perm_name)
    {
        return sni_api_throw_error("Out of memory");
    }
    jerry_string_to_buffer(args_p[0], JERRY_ENCODING_UTF8, (jerry_char_t *)perm_name, name_len);
    perm_name[name_len] = '\0';

    /* Reject if the permission is not declared in the app's manifest */
    if (!script_engine_has_permission(perm_name))
    {
        eos_free(perm_name);
        return sni_api_throw_error("Permission not declared in manifest");
    }

    eos_perm_category_t cat = _resolve_category(perm_name);
    eos_free(perm_name);

    if (cat >= EOS_PERM_CATEGORY_COUNT)
    {
        return sni_api_throw_error("Unknown permission name");
    }

    /* If already granted "always" or "foreground", skip the panel */
    eos_perm_state_t current = eos_permission_get(app_id, cat);
    if (current == EOS_PERM_STATE_ALLOW_ALWAYS)
    {
        EOS_LOG_D("Permission already granted (always) for app %s, category %d", app_id, cat);
        _schedule_async_granted(args_p[1], app_id, "granted_always");
        return jerry_undefined();
    }

    if (current == EOS_PERM_STATE_ALLOW_FOREGROUND)
    {
        EOS_LOG_D("Permission already granted (foreground) for app %s, category %d", app_id, cat);
        _schedule_async_granted(args_p[1], app_id, "granted_foreground");
        return jerry_undefined();
    }

    /* Dedup: if same category is already active or queued, drop silently */
    if (_queue_has_category(cat))
    {
        EOS_LOG_D("Permission category %d already pending for app %s, dropping duplicate", cat, app_id);
        return jerry_undefined();
    }

    /* If a panel is already shown, enqueue this request */
    if (_perm_req.active)
    {
        if (!_enqueue(cat, app_id, app_name, args_p[1]))
        {
            return sni_api_throw_error("Out of memory");
        }
        return jerry_undefined();
    }

    /* Nothing active: populate slot and show panel */
    _perm_req.active = true;
    _perm_req.category = cat;
    _perm_req.callback = jerry_value_copy(args_p[1]);
    _perm_req.app_id = eos_strdup(app_id);
    _perm_req.app_name = eos_strdup(app_name);

    _show_panel();
    return jerry_undefined();
}

jerry_value_t sni_api_eos_permission_check(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args_p[],
                                           const jerry_length_t args_count)
{
    (void)call_info_p;

    if (args_count < 1 || !jerry_value_is_string(args_p[0]))
    {
        return sni_api_throw_error("Usage: permission.check(permName)");
    }

    /* script_engine_get_current_script_id returns internal pointer - do NOT free */
    const char *app_id = script_engine_get_current_script_id();
    if (!app_id)
    {
        return sni_api_throw_error("No current script context");
    }

    jerry_size_t name_len = jerry_string_size(args_p[0], JERRY_ENCODING_UTF8);
    char *perm_name = eos_malloc(name_len + 1);
    if (!perm_name)
    {
        return sni_api_throw_error("Out of memory");
    }
    jerry_string_to_buffer(args_p[0], JERRY_ENCODING_UTF8, (jerry_char_t *)perm_name, name_len);
    perm_name[name_len] = '\0';

    eos_perm_category_t cat = _resolve_category(perm_name);
    eos_free(perm_name);

    if (cat >= EOS_PERM_CATEGORY_COUNT)
    {
        return jerry_string_sz("unknown");
    }

    eos_perm_state_t state = eos_permission_get(app_id, cat);
    return jerry_string_sz(_state_to_string(state));
}

/* ---- LVGL button callbacks ---- */

/**
 * @brief Common handler for all three panel buttons.
 *
 * 1. Persist the user's choice via eos_permission_set()
 * 2. Invoke the JS callback for THIS request
 * 3. Clean up the active request slot
 * 4. Delete the panel
 * 5. Dequeue and show the next pending request (if any)
 */
static void _perm_button_cb(lv_event_t *e, eos_perm_state_t new_state, const char *result_str)
{
    eos_perm_panel_t *panel = (eos_perm_panel_t *)lv_event_get_user_data(e);

    if (_perm_req.app_id && _perm_req.active)
    {
        if (new_state != EOS_PERM_STATE_DENIED)
        {
            eos_permission_set(_perm_req.app_id, _perm_req.category, new_state);
            EOS_LOG_D("Permission %s: app=%s cat=%d", result_str, _perm_req.app_id, _perm_req.category);
        }
        else
        {
            EOS_LOG_D("Permission denied: app=%s cat=%d", _perm_req.app_id, _perm_req.category);
        }
    }

    _invoke_callback(_perm_req.callback, result_str, _perm_req.app_id);
    _cleanup_request();
    eos_perm_panel_delete(panel);

    /* Show next queued request */
    _dequeue_and_show();
}

static void _perm_allow_once_cb(lv_event_t *e)
{
    _perm_button_cb(e, EOS_PERM_STATE_ALLOW_ONCE, "granted_once");
}

static void _perm_allow_foreground_cb(lv_event_t *e)
{
    _perm_button_cb(e, EOS_PERM_STATE_ALLOW_FOREGROUND, "granted_foreground");
}

static void _perm_deny_cb(lv_event_t *e)
{
    _perm_button_cb(e, EOS_PERM_STATE_DENIED, "denied");
}
