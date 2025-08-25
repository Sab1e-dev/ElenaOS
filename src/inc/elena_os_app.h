/**
 * @file elena_os_app.h
 * @brief 应用系统
 * @author Sab1e
 * @date 2025-08-21
 */

#ifndef ELENA_OS_APP_H
#define ELENA_OS_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "elena_os_core.h"
#include "elena_os_sys.h"

/* Public macros ----------------------------------------------*/
#define EOS_APP_DIR EOS_SYS_DIR "app/"
#define EOS_APP_INSTALLED_DIR EOS_APP_DIR "apps/"
#define EOS_APP_DATA_DIR EOS_APP_DIR "app_data/"
#define EOS_APP_ICON_FILE_NAME  "icon.bin"
#define EOS_APP_MANIFEST_FILE_NAME "manifest.json"
#define EOS_APP_SCRIPT_ENTRY_FILE_NAME "main.js"
/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/
/**
 * @brief 获取当前已安装的应用数量，即列表大小
 * @return size_t 列表大小
 */
size_t eos_app_list_size(void);
/**
 * @brief 根据索引值获取应用的 id
 * @param index 索引值（0基）
 * @return const char* id 字符串
 */
const char* eos_app_list_get_id(size_t index);
/**
 * @brief 判断指定 id 字符串的代码是否存在于应用列表中
 * @param app_id id 字符串
 * @return true 
 * @return false 
 */
bool eos_app_list_contains(const char* app_id);
/**
 * @brief 安装应用
 * @param eapk_path eapk 安装包路径
 * @return eos_result_t 安装结果
 */
eos_result_t eos_app_install(const char *eapk_path);
/**
 * @brief 卸载应用
 * @param app_id 应用 id
 * @return eos_result_t 卸载结果
 */
eos_result_t eos_app_uninstall(const char *app_id);
/**
 * @brief 初始化应用系统
 * @return eos_result_t 初始化结果
 */
eos_result_t eos_app_init(void);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_APP_H */
