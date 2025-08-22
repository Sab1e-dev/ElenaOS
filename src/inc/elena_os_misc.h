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
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_MISC_H */
