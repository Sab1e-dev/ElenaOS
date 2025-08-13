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
#include "mem_mgr.h"
// Macros and Definitions
#define LV_IMG_BIN_HEADER_SIZE 12 // Bytes
#define LV_IMG_BIN_HEADER_WIDTH_LB 4
#define LV_IMG_BIN_HEADER_HEIGHT_LB 6
#define LV_IMG_BIN_HEADER_STRIDE_LB 8
// Static Variables

// Function Implementations
/**
 * @brief 指定偏移量的指针地址中读取 uint16_t 数据
 */
static inline uint16_t read_uint16_le(const void *ptr, const uint16_t offset)
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
        if (user_data->img_data)
        {
            mem_mgr_free(user_data->img_data);
        }
        if (user_data->img_dsc)
        {
            lv_mem_free(user_data->img_dsc);
        }
        lv_mem_free(user_data);
    }
}
/**
 * @brief 从 Flash 中打开图片，并加载到 PSRAM ，然后设置 lvgl 图像源
 */
void elena_os_img_set_src(lv_obj_t *img_obj, const char *image_path)
{
    // 打开文件
    FILE *file = fopen(image_path, "rb");
    if (!file)
    {
        LV_LOG_ERROR("Failed to open file: %s\n", image_path);
        return;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0)
    {
        printf("Invalid file size\n");
        fclose(file);
        return;
    }

    // 分配PSRAM内存
    void *img_data = mem_mgr_alloc(file_size);
    if (!img_data)
    {
        printf("Failed to allocate PSRAM memory for image\n");
        fclose(file);
        return;
    }

    // 读取文件内容到PSRAM
    size_t bytes_read = fread(img_data, 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size)
    {
        printf("Failed to read complete file\n");
        mem_mgr_free(img_data);
        return;
    }

    // 创建LVGL图像对象
    const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t *p = (uint8_t *)img_data + LV_IMG_BIN_HEADER_SIZE;

    // 动态分配图像描述符（确保长期有效）
    lv_image_dsc_t *img_dsc = (lv_image_dsc_t *)lv_mem_alloc(sizeof(lv_image_dsc_t));
    if (!img_dsc)
    {
        printf("Failed to allocate image descriptor\n");
        mem_mgr_free(img_data);
        return;
    }

    uint32_t w = (uint32_t)read_uint16_le(img_data, LV_IMG_BIN_HEADER_WIDTH_LB);
    uint32_t h = (uint32_t)read_uint16_le(img_data, LV_IMG_BIN_HEADER_HEIGHT_LB);
    uint32_t stride = (uint32_t)read_uint16_le(img_data, LV_IMG_BIN_HEADER_STRIDE_LB);

    // 初始化图像描述符
    img_dsc->header.magic = LV_IMAGE_HEADER_MAGIC; // 必须设置
    img_dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    img_dsc->header.w = w;
    img_dsc->header.h = h;
    img_dsc->header.stride = stride; // RGB565每个像素2字节
    img_dsc->data_size = file_size - LV_IMG_BIN_HEADER_SIZE;
    img_dsc->data = (const uint8_t *)img_data + LV_IMG_BIN_HEADER_SIZE;

    // 创建用户数据结构
    typedef struct
    {
        void *img_data;
        lv_image_dsc_t *img_dsc;
    } img_user_data_t;

    img_user_data_t *user_data = (img_user_data_t *)lv_mem_alloc(sizeof(img_user_data_t));
    user_data->img_data = img_data;
    user_data->img_dsc = img_dsc;

    lv_img_set_src(img_obj, img_dsc);

    // 设置用户数据和删除回调
    lv_obj_set_user_data(img_obj, user_data);
    lv_obj_add_event_cb(img_obj, _img_delete_event_cb, LV_EVENT_DELETE, NULL);
}