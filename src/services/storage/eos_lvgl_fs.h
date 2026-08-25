/**
 * @file eos_lvgl_fs.h
 * @brief File system interface for LVGL, implemented using ElenixOS storage service
 */

#ifndef EOS_LVGL_FS_H
#define EOS_LVGL_FS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Register LVGL file system driver
 * This function initializes and registers the file system driver with LVGL
 * All file operations will go through the ElenixOS storage service
 */
void eos_lvgl_fs_register(void);
#ifdef __cplusplus
}
#endif

#endif /* EOS_LVGL_FS_H */
