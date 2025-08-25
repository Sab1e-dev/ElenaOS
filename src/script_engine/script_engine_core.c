
/**
 * @file script_engine_core.c
 * @brief 脚本引擎核心功能实现
 * @author Sab1e
 * @date 2025-07-26
 */

#include "script_engine_core.h"

// Includes
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include "lvgl.h"
#include "lv_bindings.h"
#include "lv_bindings_misc.h"
#include "elena_os_port.h"
#include "script_engine_nav.h"
#include "script_engine_native_func.h"
#include "elena_os_log.h"
// Macros and Definitions

// Variables
static atomic_bool should_terminate = ATOMIC_VAR_INIT(false); // 请求终止脚本标志位
static script_state_t script_state = SCRIPT_STATE_STOPPED;
static bool is_terminated_by_req = false;
// Function Implementations
/**
 * @brief VM 终止运行回调
 */
static jerry_value_t _script_engine_vm_exec_stop_callback(void *user_p)
{
    (void)user_p; // 不使用参数
    if (should_terminate)
    {
        atomic_store(&should_terminate, false);
        is_terminated_by_req = true;
        EOS_LOG_D("Script execution stopped by request.\n");
        return jerry_string_sz("Script terminated by request");
    }

    return jerry_undefined();
}
/**
 * @brief 请求停止当前脚本运行
 */
void _request_script_termination(void)
{
    atomic_store(&should_terminate, true);
}

script_state_t script_engine_get_state(void)
{
    return script_state;
}

bool script_engine_request_ready(void)
{
    if (script_state != SCRIPT_STATE_STOPPED)
    {
        return false;
    }
    script_state = SCRIPT_STATE_READY;
    return true;
}

script_engine_result_t script_engine_request_stop(void)
{
    if (script_state == SCRIPT_STATE_RUNNING)
    {
        _request_script_termination();
        return SE_OK;
    }
    return -SE_ERR_SCRIPT_NOT_RUNNING;
}
/**
 * @brief 解析js错误变量并打印错误原因
 */
static void _script_engine_exception_handler(char *tag, jerry_value_t result)
{
    printf("[%s] Error: ", tag);
    jerry_value_t value = jerry_exception_value(result, false);
    jerry_char_t str_buf_p[256];

    /* Determining required buffer size */
    jerry_size_t req_sz = jerry_string_size(value, JERRY_ENCODING_CESU8);

    if (req_sz <= 255)
    {
        if (req_sz <= 1)
        {
            printf("unknown error");
        }
        else
        {
            jerry_string_to_buffer(value, JERRY_ENCODING_CESU8, str_buf_p, req_sz);
            str_buf_p[req_sz] = '\0';
            printf("%s", (const char *)str_buf_p);
        }
    }
    else
    {
        printf("error: buffer isn't big enough");
    }
    printf("\r\n");
    jerry_value_free(value);
}
/**
 * @brief 把 script_pkg_t 转换成 JS 对象（供 JS 访问 script_info）
 */
jerry_value_t _script_engine_create_info(const script_pkg_t *script_package)
{
    jerry_value_t obj = jerry_object();

    jerry_value_t key, val;

#define SET_PROP(field)                                                 \
    key = jerry_string_sz((const jerry_char_t *)#field);                \
    val = jerry_string_sz((const jerry_char_t *)script_package->field); \
    jerry_object_set(obj, key, val);                                    \
    jerry_value_free(key);                                              \
    jerry_value_free(val);

    SET_PROP(id);
    SET_PROP(name);
    SET_PROP(version);
    SET_PROP(author);
    SET_PROP(description);

    return obj;
}

script_engine_result_t script_engine_run(script_pkg_t *script_package)
{
    if (script_package == NULL || script_package->script_str == NULL)
    {
        return -SE_ERR_NULL_PACKAGE;
    }
    if (!jerry_feature_enabled(JERRY_FEATURE_VM_EXEC_STOP))
    {
        EOS_LOG_E("JerryScript VM does not support execution stop feature.\n");
        return -SE_ERR_JERRY_INIT_FAIL;
    }
    // 检查 LVGL 是否已初始化
    if (!lv_is_initialized())
    {
        EOS_LOG_E("LVGL not initialized, please initialize it first.\n");
        return -SE_ERR_NOT_INITIALIZED;
    }

    if (script_state == SCRIPT_STATE_RUNNING)
    {
        return -SE_ERR_ALREADY_RUNNING;
    }
    script_state = SCRIPT_STATE_RUNNING;
    atomic_store(&should_terminate, false);
    is_terminated_by_req = false;
    // 初始化 JerryScript VM
    jerry_init(JERRY_INIT_EMPTY);

    // 初始化停止回调
    jerry_halt_handler(16, _script_engine_vm_exec_stop_callback, NULL);
    jerry_log_set_level(JERRY_LOG_LEVEL_DEBUG);
    // 注册原生函数
    script_engine_register_natives();

    // 初始化 LVGL 绑定
    lv_binding_init();

    // 设置全局 script_info 变量
    jerry_value_t global = jerry_current_realm();
    jerry_value_t script_info = _script_engine_create_info(script_package);

    jerry_value_t key = jerry_string_sz((const jerry_char_t *)"script_info");
    jerry_object_set(global, key, script_info);

    jerry_value_free(key);
    jerry_value_free(script_info);
    jerry_value_free(global);

    // 执行主 JS 脚本
    jerry_value_t parsed_code = jerry_parse(
        (const jerry_char_t *)script_package->script_str,
        strlen(script_package->script_str),
        JERRY_PARSE_NO_OPTS);
    // 清理脚本字符串
    eos_mem_free((void *)script_package->script_str);
    script_package->script_str = NULL;
    if (!jerry_value_is_exception(parsed_code))
    {
        jerry_value_t result = jerry_run(parsed_code);
        // 检查是否执行成功
        if (jerry_value_is_exception(result) && !is_terminated_by_req)
        {
            // 执行出错
            _script_engine_exception_handler("Script Runtime", result);
            jerry_value_free(parsed_code);
            jerry_value_free(result);
            jerry_cleanup();
            script_state = SCRIPT_STATE_STOPPED;
            return -SE_ERR_JERRY_EXCEPTION;
        }
        else
        {
            // 执行成功
            jerry_value_free(parsed_code);
            jerry_value_free(result);
            jerry_cleanup();
            script_state = SCRIPT_STATE_STOPPED;
            return SE_OK;
        }
    }
    else
    {
        // 代码解析出错
        _script_engine_exception_handler("Script Parse", parsed_code);
        jerry_value_free(parsed_code);
        jerry_cleanup();
        script_state = SCRIPT_STATE_STOPPED;
        return -SE_ERR_INVALID_JS;
    }
}

void script_engine_register_functions(const script_engine_func_entry_t *entry, const size_t funcs_count)
{
    jerry_value_t global = jerry_current_realm();
    for (size_t i = 0; i < funcs_count; ++i)
    {
        jerry_value_t fn = jerry_function_external(entry[i].handler);
        jerry_value_t name = jerry_string_sz(entry[i].name);
        jerry_object_set(global, name, fn);
        jerry_value_free(name);
        jerry_value_free(fn);
    }
    jerry_value_free(global);
}
