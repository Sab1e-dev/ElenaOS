/**
 * @file elena_os_misc.h
 * @brief 各种工具函数
 * @author Sab1e
 * @date 2025-08-22
 */

#ifndef ELENA_OS_MISC_H
#define ELENA_OS_MISC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include "elena_os_core.h"
#include "script_engine_core.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief 判断目标路径是否为目录
 */
bool eos_is_dir(const char *path);
/**
 * @brief 判断目标路径是否为文件
 */
bool eos_is_file(const char *path);
/**
 * @brief 如果文件夹不存在则创建
 * @param path 目标文件夹路径
 * @param mode 权限
 * @return eos_result_t 创建结果
 */
eos_result_t eos_mkdir_if_not_exist(const char *path, mode_t mode);
/**
 * @brief 如果文件不存在则创建
 * @param path 目标文件路径
 * @param default_content 默认文件内容，可为空""
 * @return eos_result_t 创建结果
 */
eos_result_t eos_create_file_if_not_exist(const char *path, const char *default_content);
/**
 * @brief 递归创建目录
 * @param path 目标目录
 * @return eos_result_t 创建结果
 */
eos_result_t eos_create_dir_recursive(const char *path);

/**
 * @brief 递归删除目录
 * @param path 目标目录
 * @return eos_result_t 删除结果 
 */
eos_result_t eos_rm_recursive(const char *path);
/**
 * @brief 是否为有效的文件名
 * @param name 文件名称
 * @return eos_result_t 判断结果
 */
bool eos_is_valid_filename(const char *name);
/**
 * @brief 从指定文件中读取文本字符串
 * @param filename 目标文件路径 
 * @return char* 获取成功则返回文本字符串
 * 
 * 获取失败则返回 NULL
 */
char *eos_read_file(const char *filename);
/**
 * @brief 用于创建给定字符串 s 的副本。
 * @param s 目标字符串
 * @return const char* 创建成功则返回新分配的字符串指针
 * 
 * 内存分配失败则返回 NULL
 */
const char *eos_strdup(const char *s);
/**
 * @brief 释放并清空脚本包内的数据
 * @param pkg 目标脚本包
 */
void eos_pkg_free(script_pkg_t *pkg);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_MISC_H */
