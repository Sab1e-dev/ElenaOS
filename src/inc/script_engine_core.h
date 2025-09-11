
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

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "jerryscript.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
/**
 * @brief 脚本运行状态
 */
typedef enum{
    SCRIPT_STATE_STOPPED,
    SCRIPT_STATE_READY,
    SCRIPT_STATE_RUNNING
} script_state_t;
/**
 * @brief 脚本包类型
 */
typedef enum{
    SCRIPT_TYPE_UNKNOWN=0,
    SCRIPT_TYPE_APPLICATION=1,
    SCRIPT_TYPE_WATCHFACE=2
}script_pkg_type_t;
/**
 * @brief 函数入口链接结构体
 */
typedef struct {
    const char* name;
    jerry_external_handler_t handler;
} script_engine_func_entry_t;
/**
 * @brief 脚本包描述结构体
 */
typedef struct {
    const char* id;               // 应用唯一ID，例如 "com.mydev.clock"
    const char* name;             // 应用显示名称，例如 "时钟"
    script_pkg_type_t type;       // 脚本类型，例如 SCRIPT_TYPE_APPLICATION
    const char* version;          // 应用版本，例如 "1.0.2"
    const char* author;           // 开发者名称
    const char* description;      // 简要说明
    const char* script_str;       // 主 JS 脚本字符串(UTF-8)
} script_pkg_t;

/**
 * @brief 脚本引擎运行结果
 */
typedef enum {
    SE_OK = 0,                   // 启动成功
    SE_FAILED,
    SE_ERR_NULL_PACKAGE,         // 传入的包指针为空
    SE_ERR_INVALID_JS,           // JS 脚本无效（语法错误、空字符串等）
    SE_ERR_JERRY_EXCEPTION,      // 运行期间抛出 JS 异常
    SE_ERR_ALREADY_RUNNING,      // 当前已有 APP 在运行
    SE_ERR_JERRY_INIT_FAIL,      // JerryScript 初始化失败
    SE_ERR_NOT_INITIALIZED,      // 未初始化
    SE_ERR_SCRIPT_NOT_RUNNING,   // 没有在运行的应用
    SE_ERR_BUSY,                 // 正忙
    SE_ERR_VAR_NULL,             // 值为空
    SE_ERR_ALREADY_INITIALIZED,  // 已经初始化
    SE_ERR_STACK_EMPTY,          // 栈空
    SE_ERR_MALLOC,
    SE_ERR_UNKNOWN               // 未知错误
} script_engine_result_t;

/* Public function prototypes --------------------------------*/

/**
 * @brief 关闭当前运行的 JS 应用
 * @return script_engine_result_t 返回操作结果
 */
script_engine_result_t script_engine_request_stop(void);

/**
 * @brief 获取 manifest.json 并填充 script_pkg_t 结构体
 * @param manifest_path manifest.json 文件路径
 * @param pkg 目标结构体指针
 * @return script_engine_result_t
 */
script_engine_result_t script_engine_get_manifest(const char *manifest_path, script_pkg_t *pkg);

/**
 * @brief 运行指定应用，如果当前已有应用在运行则自动清除
 * @param script_package 脚本包
 * @return script_engine_result_t 返回操作结果
 */
script_engine_result_t script_engine_run(script_pkg_t* script_package);

/**
 * @brief 获取脚本引擎当前状态
 * @return script_state_t 状态
 */
script_state_t script_engine_get_state(void);

/**
 * @brief 请求脚本就绪
 * @return true 请求成功：脚本已就绪
 * @return false 请求失败：脚本运行中或在此之前已经就绪
 */
bool script_engine_request_ready(void);

/**
 * @brief 注册C函数到JS
 * @param entry 函数入口数组
 * @param funcs_count 数组长度
 */
void script_engine_register_functions(const script_engine_func_entry_t* entry, const size_t funcs_count);

#ifdef __cplusplus
}
#endif

#endif // SCRIPT_ENGINE_CORE_H
