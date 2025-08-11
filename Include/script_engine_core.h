
/**
 * @file script_engine_core.h
 * @brief 应用程序系统外部接口定义
 * @author Sab1e
 * @date 2025-07-26
 */
#ifndef SCRIPT_ENGINE_CORE_H
#define SCRIPT_ENGINE_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "jerryscript.h"
// 类型声明
typedef enum{
    SCRIPT_TYPE_UNKNOWN=0,
    SCRIPT_TYPE_APPLICATION=1,
    SCRIPT_TYPE_WATCH_FACE=2
}ScriptType_t;
/**
 * @brief 函数入口链接结构体
 */
typedef struct {
    const char* name;
    jerry_external_handler_t handler;
} ScriptEngineFuncEntry;
// 应用包描述结构体
typedef struct {
    const char* id;           // 应用唯一ID，例如 "com.mydev.clock"
    const char* name;             // 应用显示名称，例如 "时钟"
    const ScriptType_t type;    // 脚本类型，例如 SCRIPT_TYPE_APPLICATION
    const char* version;          // 应用版本，例如 "1.0.2"
    const char* author;           // 开发者名称
    const char* description;      // 简要说明
    const char* script_str;       // 主 JS 脚本字符串(UTF-8)
} ScriptPackage_t;

// 应用运行结果枚举
typedef enum {
    SE_OK = 0,                    // 启动成功
    SE_ERR_NULL_PACKAGE = -1,         // 传入的包指针为空
    SE_ERR_INVALID_JS = -2,           // JS 脚本无效（语法错误、空字符串等）
    SE_ERR_JERRY_EXCEPTION = -3,      // 运行期间抛出 JS 异常
    SE_ERR_ALREADY_RUNNING = -4,      // 当前已有 APP 在运行
    SE_ERR_JERRY_INIT_FAIL = -5,      // JerryScript 初始化失败
    SE_ERR_LVGL_NOT_INITIALIZED = -6, // LVGL 未初始化
    SE_ERR_SCRIPT_NOT_RUNNING = -7,           // 没有在运行的应用
    SE_ERR_UNKNOWN = -99               // 未知错误
} ScriptEngineResult_t;

// 函数声明
ScriptEngineResult_t script_engine_request_stop();
ScriptEngineResult_t script_engine_run(const ScriptPackage_t* app);
void script_engine_register_functions(const ScriptEngineFuncEntry* entry, const size_t funcs_count);

#ifdef __cplusplus
}
#endif

#endif // SCRIPT_ENGINE_CORE_H
