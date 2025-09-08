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
 * 表盘位置：/.sys/wf/faces
 * 表盘数据：/.sys/wf/wf_data
 */
#define EOS_SYS_DIR "/.sys/"
#define EOS_SYS_CONFIG_DIR EOS_SYS_DIR "config/"
#define EOS_SYS_CONFIG_FILE_PATH EOS_SYS_CONFIG_DIR "cfg.json"

#define EOS_SYS_RES_DIR EOS_SYS_DIR "res/"
#define EOS_SYS_RES_IMG_DIR EOS_SYS_RES_DIR "img/"
/************************** 系统配置信息的键 **************************/
#define EOS_SYS_CFG_KEY_VERSION "version"
#define EOS_SYS_CFG_KEY_LANGUAGE "language"
#define EOS_SYS_CFG_KEY_WATCHFACE_ID "wf_id"
#define EOS_SYS_CFG_KEY_BLUETOOTH "bluetooth"

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief 系统第一次运行时初始化
 */
void eos_sys_init(void);
/**
 * @brief 设置布尔类型的配置项
 * @param key 配置项的键
 * @param value 布尔值
 * @return 操作结果
 */
eos_result_t eos_sys_cfg_set_bool(const char *key, bool value);
/**
 * @brief 设置字符串类型的配置项
 * @param key 配置项的键
 * @param value 字符串值
 * @return 操作结果
 */
eos_result_t eos_sys_cfg_set_string(const char *key, const char *value);
/**
 * @brief 设置数字类型的配置项
 * @param key 配置项的键
 * @param value 数字值
 * @return 操作结果
 */
eos_result_t eos_sys_cfg_set_number(const char *key, double value);
/**
 * @brief 获取布尔类型的配置项
 * @param key 配置项的键
 * @param default_value 默认值（当配置项不存在或类型不匹配时返回）
 * @return 获取到的布尔值或默认值
 */
bool eos_sys_cfg_get_bool(const char *key, bool default_value);
/**
 * @brief 获取字符串类型的配置项
 * @param key 配置项的键
 * @param default_value 默认值（当配置项不存在或类型不匹配时返回）
 * @return 获取到的字符串值或默认值（需要调用者释放返回的字符串内存）
 */
char *eos_sys_cfg_get_string(const char *key, const char *default_value);
/**
 * @brief 获取数字类型的配置项
 * @param key 配置项的键
 * @param default_value 默认值（当配置项不存在或类型不匹配时返回）
 * @return 获取到的数字值或默认值
 */
double eos_sys_cfg_get_number(const char *key, double default_value);
/**
 * @brief 添加新的设置项到系统配置文件
 * @param key 要添加的设置项键名（字符串）
 * @param value 要添加的设置项值（字符串）
 * @return 返回结果
 */
eos_result_t eos_sys_add_config_item(const char *key, const char *value);
void eos_sys_settings_create(void);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_SYS_H */
