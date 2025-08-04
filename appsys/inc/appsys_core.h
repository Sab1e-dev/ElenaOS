
/**
 * @file appsys_core.h
 * @brief 应用程序系统外部接口定义
 * @author Sab1e
 * @date 2025-07-26
 */
#ifndef APPSYS_CORE_H
#define APPSYS_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "lvgl/lvgl.h"
#include "jerryscript.h"
// 类型声明
/**
 * @brief 函数入口链接结构体
 */
typedef struct {
    const char* name;
    jerry_external_handler_t handler;
} AppSysFuncEntry;
// 应用包描述结构体
typedef struct {
    const char* app_id;           // 应用唯一ID，例如 "com.mydev.clock"
    const char* name;             // 应用显示名称，例如 "时钟"
    const char* version;          // 应用版本，例如 "1.0.2"
    const char* author;           // 开发者名称
    const char* description;      // 简要说明
    const char* mainjs_str;       // 主 JS 脚本字符串
} ApplicationPackage_t;

// 应用运行结果枚举
typedef enum {
    APP_SUCCESS = 0,                    // 启动成功
    APP_ERR_NULL_PACKAGE = -1,         // 传入的包指针为空
    APP_ERR_INVALID_JS = -2,           // JS 脚本无效（语法错误、空字符串等）
    APP_ERR_JERRY_EXCEPTION = -3,      // 运行期间抛出 JS 异常
    APP_ERR_ALREADY_RUNNING = -4,      // 当前已有 APP 在运行
    APP_ERR_JERRY_INIT_FAIL = -5,      // JerryScript 初始化失败
} AppRunResult_t;


// 函数声明
AppRunResult_t appsys_run_app(const ApplicationPackage_t* app);
void appsys_register_functions(const AppSysFuncEntry* entry, const size_t funcs_count);

#ifdef __cplusplus
}
#endif

#endif // APPSYS_CORE_H
