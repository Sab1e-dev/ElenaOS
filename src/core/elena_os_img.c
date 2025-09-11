/**
 * @file elena_os_img.c
 * @brief 图片显示
 * @author Sab1e
 * @date 2025-08-12
 */

#include "elena_os_img.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include "lvgl.h"
#include "elena_os_log.h"
#include "elena_os_port.h"
// Macros and Definitions
#define LV_IMG_BIN_HEADER_SIZE 12 // Bytes
#define LV_IMG_BIN_HEADER_WIDTH_LB 4
#define LV_IMG_BIN_HEADER_HEIGHT_LB 6
#define LV_IMG_BIN_HEADER_STRIDE_LB 8
// Variables

// Function Implementations
/**
 * @brief 删除事件回调函数
 */
static void _img_delete_event_cb(lv_event_t *e)
{
    EOS_LOG_D("Try delete image");
    lv_obj_t *img_obj = lv_event_get_target(e);
    EOS_CHECK_PTR_RETURN(img_obj);

    // 从事件中获取用户数据
    img_user_data_t *user_data = (img_user_data_t *)lv_event_get_user_data(e);
    EOS_CHECK_PTR_RETURN(user_data);

    if (user_data->bin_data)
    {
        eos_free_large(user_data->bin_data);
        user_data->bin_data = NULL;
    }
    if (user_data->img_dsc)
    {
        lv_free(user_data->img_dsc);
        user_data->img_dsc = NULL;
    }
    lv_free(user_data);
    EOS_LOG_D("Image deleted.");
}

void eos_img_set_size(lv_obj_t *img_obj, const uint32_t w, const uint32_t h)
{
    const void *src = lv_img_get_src(img_obj);

    if (src == NULL)
    {
        EOS_LOG_E("Image src is NULL");
        return;
    }

    // 检查 src 类型
    lv_image_src_t src_type = lv_img_src_get_type(src);
    if (src_type != LV_IMAGE_SRC_VARIABLE)
    {
        EOS_LOG_E("Image src not a variable");
        return;
    }
    // 内存里的图片描述符
    const lv_image_dsc_t *dsc = (const lv_image_dsc_t *)src;
    if (dsc->header.w == 0 || dsc->header.h == 0)
    {
        EOS_LOG_E("Image width or height is 0");
        return;
    }
    lv_obj_set_size(img_obj, w, h);
    lv_image_set_scale_x(img_obj, (uint32_t)((w * 256) / dsc->header.w));
    lv_image_set_scale_y(img_obj, (uint32_t)((h * 256) / dsc->header.h));
}

void eos_img_set_src(lv_obj_t *img_obj, const char *bin_path)
{
    EOS_CHECK_PTR_RETURN(img_obj);

    // 清除回调
    lv_obj_remove_event_cb_with_user_data(img_obj, _img_delete_event_cb, NULL);

    // 避免数据泄漏
    lv_image_set_src(img_obj, NULL);

    // 打开新图像文件
    int fd = open(bin_path, O_RDONLY);
    if (fd == -1)
    {
        EOS_LOG_E("Failed to open file: %s\n", bin_path);
        return;
    }

    // 获取文件大小
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1)
    {
        EOS_LOG_E("Failed to get file size\n");
        close(fd);
        return;
    }
    off_t file_size = file_stat.st_size;

    if (file_size <= 0)
    {
        EOS_LOG_E("Invalid file size\n");
        close(fd);
        return;
    }

    // 分配内存
    void *bin_data = eos_malloc_large(file_size);
    if (!bin_data)
    {
        EOS_LOG_E("Failed to allocate memory for image\n");
        close(fd);
        return;
    }

    // 读取文件内容到内存
    ssize_t bytes_read = read(fd, bin_data, file_size);
    close(fd); // 读取完成后立即关闭文件描述符

    if (bytes_read != file_size)
    {
        EOS_LOG_E("Failed to read complete file (read %zd of %ld bytes)\n", bytes_read, file_size);
        eos_free_large(bin_data);
        return;
    }

    // 动态分配图像描述符
    lv_image_dsc_t *img_dsc = (lv_image_dsc_t *)lv_malloc(sizeof(lv_image_dsc_t));
    if (!img_dsc)
    {
        EOS_LOG_E("Failed to allocate image descriptor\n");
        eos_free_large(bin_data);
        return;
    }
    memset(img_dsc, 0, sizeof(lv_image_dsc_t));

    memcpy(&img_dsc->header, bin_data, sizeof(lv_image_header_t));

    if (img_dsc->header.magic != LV_IMAGE_HEADER_MAGIC)
    {
        EOS_LOG_E("Invalid image magic\n");
        lv_free(img_dsc);
        eos_free_large(bin_data);
        return;
    }

    // 直接赋值图像描述符头
    img_dsc->data_size = file_size - sizeof(lv_image_header_t);
    img_dsc->data = (const uint8_t *)bin_data + sizeof(lv_image_header_t);

    // 创建用户数据结构
    img_user_data_t *user_data = (img_user_data_t *)lv_malloc(sizeof(img_user_data_t));
    if (!user_data)
    {
        EOS_LOG_E("Failed to allocate user data\n");
        eos_free_large(bin_data);
        lv_free(img_dsc);
        return;
    }
    user_data->bin_data = bin_data;
    user_data->img_dsc = img_dsc;

    // 设置图像源
    lv_image_set_src(img_obj, img_dsc);
    // 添加删除事件回调，并将用户数据附加到回调
    lv_obj_add_event_cb(img_obj, _img_delete_event_cb, LV_EVENT_DELETE, user_data);
    EOS_LOG_D("Image Set OK");
}