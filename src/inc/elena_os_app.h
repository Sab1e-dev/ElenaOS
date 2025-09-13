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
#include "lvgl.h"

/* Public macros ----------------------------------------------*/
/************************** 路径定义 **************************/
#define EOS_APP_DIR EOS_SYS_DIR "app/"
#define EOS_APP_INSTALLED_DIR EOS_APP_DIR "apps/"
#define EOS_APP_DATA_DIR EOS_APP_DIR "app_data/"
/************************** 文件名定义 **************************/
#define EOS_APP_ICON_FILE_NAME  "icon.bin"
#define EOS_APP_MANIFEST_FILE_NAME "manifest.json"
#define EOS_APP_SCRIPT_ENTRY_FILE_NAME "main.js"
/************************** 配置文件 **************************/
#define EOS_APP_LIST_APP_ORDER_PATH EOS_SYS_CONFIG_DIR "app_order.json"
/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/
/**
 * @brief 将目标 ID 的应用移动到指定位置，以便 app_list 排序
 * @param app_id 目标 ID
 * @param new_index 新的索引值
 * @return eos_result_t 
 */
eos_result_t eos_app_order_move(const char *app_id, size_t new_index);
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
 * @brief 从应用列表中获取与输入字符串匹配的 ID（避免重复分配内存）
 * @param id 要查找的原始 ID（如 header.pkg_id）
 * @return 列表中已存在的字符串指针（生命周期由列表管理），若未找到则返回 NULL
 */
const char *eos_app_list_get_existing_id(const char *id);
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
 * @brief 当应用被删除时，自动删除指定对象
 * @param obj 目标对象
 * @param app_id 目标应用 ID
 */
void eos_app_obj_auto_delete(lv_obj_t *obj, const char *app_id);
/**
 * @brief 初始化应用系统
 * @return eos_result_t 初始化结果
 */
eos_result_t eos_app_init(void);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_APP_H */
