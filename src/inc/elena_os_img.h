/**
 * @file elena_os_img.h
 * @brief 图片显示
 * @author Sab1e
 * @date 2025-08-12
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
#include "elena_os_sys.h"
/* Public macros ----------------------------------------------*/
#define EOS_IMG_APP EOS_SYS_RES_IMG_DIR "app.bin"

/* Public typedefs --------------------------------------------*/

/**
 * @brief 用户数据结构体
 * 
 * 用于清理内存和动态分配的 img
 */
typedef struct {
    void *bin_data;             // 指向存储 bin 文件数据的指针
    lv_image_dsc_t *img_dsc;    // 指向图片描述符的指针
} img_user_data_t;
/* Public function prototypes --------------------------------*/

/**
 * @brief 从 Flash 中打开图片，并加载到内存，然后设置 lvgl 图像源。
 * @param img_obj 要设置图像源的 Image 对象
 * @param bin_path bin 文件的路径
 * @warning 只支持 LVGL 的 bin 文件
 * @note 当 lv_img_t 的对象删除时，自动释放内存
 */
void eos_img_set_src(lv_obj_t *img_obj, const char *bin_path);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_IMG_H */
