
/**
 * @file appsys_core.c
 * @brief 应用程序系统核心功能实现
 * @author Sab1e
 * @date 2025-07-26
 */

#include "appsys_core.h"
#include "appsys_native_func.h"
#include "lv_bindings.h"
#include "lv_bindings_misc.h"
#include "appsys_port.h"
#include <string.h>
#include <stdio.h>
// 全局状态记录是否已初始化 VM
static bool js_vm_initialized = false;
/**
 * @brief 注册C函数到JS
 * @param entry 函数入口数组
 * @param funcs_count 数组长度
 */
void appsys_register_functions(const AppSysFuncEntry* entry,const size_t funcs_count) {
    jerry_value_t global = jerry_current_realm();
    for (size_t i = 0; i < funcs_count; ++i) {
        jerry_value_t fn = jerry_function_external(entry[i].handler);
        jerry_value_t name = jerry_string_sz(entry[i].name);
        jerry_object_set(global, name, fn);
        jerry_value_free(name);
        jerry_value_free(fn);
    }
    jerry_value_free(global);
}
/**
 * @brief appsys_clear_current_app 清除当前运行的 JS 应用
 */
static void appsys_clear_current_app() {
    if (js_vm_initialized) {
        jerry_cleanup();
        js_vm_initialized = false;
    }
}
/**
 * @brief appsys_create_app_info 把 ApplicationPackage_t 转换成 JS 对象（供 JS 访问 app_info）
 * @param ApplicationPackage_t 结构体
 * @return jerry_value_t 返回值说明
 */
jerry_value_t appsys_create_app_info(const ApplicationPackage_t* app) {
    jerry_value_t obj = jerry_object();

    jerry_value_t key, val;

#define SET_PROP(field) \
        key = jerry_string_sz((const jerry_char_t*)#field); \
        val = jerry_string_sz((const jerry_char_t*)app->field); \
        jerry_object_set(obj, key, val); \
        jerry_value_free(key); \
        jerry_value_free(val);

    SET_PROP(app_id);
    SET_PROP(name);
    SET_PROP(version);
    SET_PROP(author);
    SET_PROP(description);

    return obj;
}

/**
 * @brief appsys_run_app 运行指定应用，如果当前已有应用在运行则自动清除
 * @param ApplicationPackage_t 应用包结构体
 * @return AppRunResult_t 返回运行结果枚举
 */
AppRunResult_t appsys_run_app(const ApplicationPackage_t* app) {
    if (app == NULL || app->mainjs_str == NULL) {
        return APP_ERR_NULL_PACKAGE;
    }
    // 清除前一个 JS 应用
    appsys_clear_current_app();

    // 初始化 JerryScript VM
    jerry_init(JERRY_INIT_EMPTY);
    js_vm_initialized = true;

    // 注册原生函数
    appsys_register_natives();

    // 初始化 LVGL 绑定
    lv_binding_init();

    // 设置全局 app_info 变量
    jerry_value_t global = jerry_current_realm();
    jerry_value_t app_info = appsys_create_app_info(app);

    jerry_value_t key = jerry_string_sz((const jerry_char_t*)"app_info");
    jerry_object_set(global, key, app_info);

    jerry_value_free(key);
    jerry_value_free(app_info);
    jerry_value_free(global);

    // 执行主 JS 脚本
    jerry_value_t result = jerry_eval(
        (const jerry_char_t*)app->mainjs_str,
        strlen(app->mainjs_str),
        JERRY_PARSE_NO_OPTS
    );

    // 检查是否执行成功
    if (jerry_value_is_exception(result)) {
        printf("JS Error: ");
        jerry_value_t value = jerry_exception_value(result, false);
        jerry_char_t str_buf_p[256];

        /* Determining required buffer size */
        jerry_size_t req_sz = jerry_string_size(value, JERRY_ENCODING_CESU8);

        if (req_sz <= 255)
        {
            jerry_string_to_buffer(value, JERRY_ENCODING_CESU8, str_buf_p, req_sz);
            str_buf_p[req_sz] = '\0';
            printf("%s", (const char*)str_buf_p);
        }
        else
        {
            printf("error: buffer isn't big enough");
        }
        jerry_value_free(value);
        jerry_value_free(result);
        return APP_ERR_JERRY_EXCEPTION;
    }

    jerry_value_free(result);
    jerry_cleanup();
    return APP_SUCCESS;
}

