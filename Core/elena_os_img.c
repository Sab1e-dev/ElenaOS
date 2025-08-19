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
#include "mem_mgr.h"
#include "elena_os_log.h"
// Macros and Definitions
#define LV_IMG_BIN_HEADER_SIZE 12 // Bytes
#define LV_IMG_BIN_HEADER_WIDTH_LB 4
#define LV_IMG_BIN_HEADER_HEIGHT_LB 6
#define LV_IMG_BIN_HEADER_STRIDE_LB 8
// Variables

// Function Implementations
/**
 * @brief 指定偏移量的指针地址中读取 uint16_t 数据
 */
static inline uint16_t _read_uint16_le(const void *ptr, const uint16_t offset)
{
    const uint8_t *p = (const uint8_t *)ptr;
    return ((uint16_t)p[offset + 1] << 8) | (uint16_t)p[offset];
}
/**
 * @brief 删除事件回调函数
 */
static void _img_delete_event_cb(lv_event_t *e)
{
    lv_obj_t *img_obj = lv_event_get_target(e);
    img_user_data_t *user_data = (img_user_data_t *)lv_obj_get_user_data(img_obj);

    if (user_data)
    {
        if (user_data->bin_data)
        {
            mem_mgr_free(user_data->bin_data);
        }
        if (user_data->img_dsc)
        {
            lv_mem_free(user_data->img_dsc);
        }
        lv_mem_free(user_data);
    }
    EOS_LOG_D("Image deleted.");
}

void eos_img_set_src(lv_obj_t *img_obj, const char *bin_path)
{
    EOS_CHECK_PTR_RETURN(img_obj);
    // 使用 POSIX open 打开文件（只读模式）
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

    // 分配 PSRAM 内存
    void *bin_data = mem_mgr_alloc(file_size);
    if (!bin_data)
    {
        EOS_LOG_E("Failed to allocate PSRAM memory for image\n");
        close(fd);
        return;
    }

    // 读取文件内容到 PSRAM
    ssize_t bytes_read = read(fd, bin_data, file_size);
    close(fd); // 读取完成后立即关闭文件描述符

    if (bytes_read != file_size)
    {
        EOS_LOG_E("Failed to read complete file (read %zd of %ld bytes)\n", bytes_read, file_size);
        mem_mgr_free(bin_data);
        return;
    }

    // 创建 LVGL 图像对象
    const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t *p = (uint8_t *)bin_data + LV_IMG_BIN_HEADER_SIZE;

    // 动态分配图像描述符
    lv_image_dsc_t *img_dsc = (lv_image_dsc_t *)lv_mem_alloc(sizeof(lv_image_dsc_t));
    if (!img_dsc)
    {
        EOS_LOG_E("Failed to allocate image descriptor\n");
        mem_mgr_free(bin_data);
        return;
    }
    memset(img_dsc, 0, sizeof(lv_image_dsc_t)); // 必须清空

    memcpy(&img_dsc->header, bin_data, sizeof(lv_image_header_t));

    if (img_dsc->header.magic != LV_IMAGE_HEADER_MAGIC)
    {
        EOS_LOG_E("Invalid image magic\n");
        lv_mem_free(img_dsc);
        mem_mgr_free(bin_data);
        return;
    }

    // 直接赋值图像描述符头
    img_dsc->data_size = file_size - sizeof(lv_image_header_t);
    img_dsc->data = (const uint8_t *)bin_data + sizeof(lv_image_header_t);

    // 创建用户数据结构
    img_user_data_t *user_data = (img_user_data_t *)lv_mem_alloc(sizeof(img_user_data_t));
    if (!user_data)
    {
        EOS_LOG_E("Failed to allocate user data\n");
        mem_mgr_free(bin_data);
        lv_mem_free(img_dsc);
        return;
    }
    user_data->bin_data = bin_data;
    user_data->img_dsc = img_dsc;

    // 设置图像源
    lv_image_set_src(img_obj, img_dsc);

    EOS_LOG_D("Image Set OK");
    // 设置用户数据和删除回调
    lv_obj_set_user_data(img_obj, user_data);
    lv_obj_add_event_cb(img_obj, _img_delete_event_cb, LV_EVENT_DELETE, NULL);
}