/**
 * @file elena_os_core.c
 * @brief Elena OS 核心代码实现
 * @author Sab1e
 * @date 2025-08-10
 */

#include "elena_os_core.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "elena_os_drag_item.h"
#include "lvgl.h"
#include "elena_os_blur.h"
#include "elena_os_img.h"
// Macros and Definitions
// 图片缓冲区大小
// #define LV_FS_CACHE_SIZE  32*1024

// Static Variables

// Function Implementations
ElenaOSResult_t elena_os_run(){
    lv_obj_t* scr = lv_scr_act();

    // 2. 初始化显示驱动
    lv_display_t*  disp = lv_disp_get_default();

    // // 3. 设置绘图缓冲区
    lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);


    lv_obj_t* btn = lv_btn_create(scr);
    lv_obj_align(btn,LV_ALIGN_CENTER,0,0);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label,"09:15");
    lv_obj_center(label);

    lv_obj_t* img = lv_img_create(lv_scr_act());
    elena_os_img_set_src(img, "b.bin");
    lv_obj_center(img);
    lv_obj_move_background(img);


    // uint32_t w = lv_disp_get_hor_res(NULL);
    // uint32_t h = lv_disp_get_ver_res(NULL);
    // lv_obj_t* blur_obj = create_rgb888_glass(scr,w,h);
    // if(blur_obj==NULL){
    //     printf("BLUR INIT ERROR!\n");
    // }

    drag_item_t* drag_item = elena_os_drag_item_create(scr);
    elena_os_drag_item_set_dir(drag_item,DRAG_DIR_DOWN);

    drag_item_t* drag_item1 = elena_os_drag_item_create(scr);
    elena_os_drag_item_set_dir(drag_item1,DRAG_DIR_UP);
    
    
    while(1){
        uint32_t d = lv_timer_handler();
        rt_thread_mdelay(d);
    }
}
