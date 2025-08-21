/**
 * @file elena_os_sys.h
 * @brief 系统
 * @author Sab1e
 * @date 2025-08-21
 */

#ifndef ELENA_OS_SYS_H
#define ELENA_OS_SYS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "elena_os_core.h"

/* Public macros ----------------------------------------------*/

/**
 *
 * 系统设置配置文件：/.sys/config/cfg.json
 * 应用位置：/.sys/app/package
 * 应用数据：/.sys/app/app_data
 *
 */
#define EOS_SYS_DIR "/.sys/"
#define EOS_SYS_CONFIG_DIR EOS_SYS_DIR "config/"
#define EOS_SYS_CONFIG_FILE_PATH EOS_SYS_CONFIG_DIR "cfg.json"

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief 系统第一次运行时初始化
 */
void eos_sys_init();

/**
 * @brief 添加新的设置项到系统配置文件
 * @param key 要添加的设置项键名（字符串）
 * @param value 要添加的设置项值（字符串）
 * @return 返回结果
 */
ElenaOSResult_t eos_sys_add_config_item(const char *key, const char *value);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_SYS_H */
