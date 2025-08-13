/*
 * @file       elena_os_img.h
 * @brief      图片显示
 * @author     Sab1e
 * @date       2025-08-12
 */

#ifndef ELENA_OS_IMG_H
#define ELENA_OS_IMG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
typedef struct {
    void *img_data;
    lv_image_dsc_t *img_dsc;
} img_user_data_t;
/* Public function prototypes --------------------------------*/
void elena_os_img_set_src(lv_obj_t *img_obj, const char *image_path);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_IMG_H */
