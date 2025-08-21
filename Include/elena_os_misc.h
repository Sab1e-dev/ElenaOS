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
bool eos_is_dir(const char *path);
bool eos_is_file(const char *path);
ElenaOSResult_t eos_mkdir_if_not_exist(const char *path, mode_t mode);
ElenaOSResult_t eos_create_file_if_not_exist(const char *path, const char *default_content);

#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_MISC_H */
