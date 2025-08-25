/**
 * @file elena_os_watchface.h
 * @brief 表盘
 * @author Sab1e
 * @date 2025-08-22
 */

#ifndef ELENA_OS_WATCHFACE_H
#define ELENA_OS_WATCHFACE_H

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
#define EOS_WATCHFACE_DIR EOS_SYS_DIR "wf/"
#define EOS_WATCHFACE_INSTALLED_DIR EOS_WATCHFACE_DIR "faces/"
#define EOS_WATCHFACE_DATA_DIR EOS_WATCHFACE_DIR "wf_data/"
#define EOS_WATCHFACE_MANIFEST_FILE_NAME "manifest.json"
#define EOS_WATCHFACE_SNAPSHOT_FILE_NAME "snapshot.bin"
#define EOS_WATCHFACE_SCRIPT_ENTRY_FILE_NAME "main.js"
/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/
/**
 * @brief 获取当前已安装的表盘数量，即列表大小
 * @return size_t 列表大小
 */
size_t eos_watchface_list_size(void);
/**
 * @brief 根据索引值获取表盘的 id
 * @param index 索引值（0基）
 * @return const char* id 字符串
 */
const char* eos_watchface_list_get_id(size_t index);
/**
 * @brief 判断指定 id 字符串的代码是否存在于表盘列表中
 * @param watchface_id id 字符串
 * @return true 
 * @return false 
 */
bool eos_watchface_list_contains(const char* watchface_id);
/**
 * @brief 安装表盘
 * @param eapk_path eapk 安装包路径
 * @return eos_result_t 安装结果
 */
eos_result_t eos_watchface_install(const char *eapk_path);
/**
 * @brief 卸载表盘
 * @param watchface_id 表盘 id
 * @return eos_result_t 卸载结果
 */
eos_result_t eos_watchface_uninstall(const char *watchface_id);
/**
 * @brief 初始化表盘系统
 * @return eos_result_t 初始化结果
 */
eos_result_t eos_watchface_init(void);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_WATCHFACE_H */
