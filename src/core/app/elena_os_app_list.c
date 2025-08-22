/**
 * @file elena_os_app_list.c
 * @brief 应用列表页面
 * @author Sab1e
 * @date 2025-08-21
 */

#include "elena_os_app_list.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "elena_os_nav.h"
// Macros and Definitions

// Variables

// Function Implementations

void eos_app_list_create(){
    // 创建新的页面用于绘制应用列表
    lv_obj_t *scr = eos_nav_scr_create();
    lv_screen_load(scr);

    
}