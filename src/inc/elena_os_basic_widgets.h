/**
 * @file elena_os_basic_widgets.h
 * @brief 基本控件
 * @author Sab1e
 * @date 2025-08-17
 */

#ifndef ELENA_OS_BASIC_WIDGETS_H
#define ELENA_OS_BASIC_WIDGETS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/
/**
 * @brief 应用内上方的头部结构体定义
 */
typedef struct{
    lv_obj_t *container;
    lv_obj_t *clock_label;
    lv_obj_t *title_label;
    lv_obj_t *back_btn;
    lv_timer_t *clock_timer;
}eos_app_header_t;
/**
 * @brief 列表内的滑块定义
 */
typedef struct{
    lv_obj_t *slider;
    lv_obj_t *plus_btn;
    lv_obj_t *minus_btn;
}eos_list_slider_t;
/* Public function prototypes --------------------------------*/

/**
 * @brief 创建一个返回按钮
 * @param parent 父对象
 * @param show_text 是否显示返回文字
 * @return lv_obj_t* 创建成功则返回 btn 对象，否则返回 NULL
 */
lv_obj_t *eos_back_btn_create(lv_obj_t *parent, bool show_text);
/**
 * @brief 向列表中添加一个带有图标的按钮
 * @param list 目标列表
 * @param icon 左侧图标 支持图片路径
 * @param txt 按钮中的文字
 * @return lv_obj_t* 创建成功则返回按钮对象指针
 * 
 * 创建失败则返回 NULL
 */
lv_obj_t * eos_list_add_button(lv_obj_t * list, const void * icon, const char * txt);
/**
 * @brief 应用头设置标题名称
 * @param title 标题字符串
 */
void eos_app_header_set_title(const char *title);
/**
 * @brief 隐藏应用头
 */
void eos_app_header_hide(void);
/**
 * @brief 显示应用头
 */
void eos_app_header_show(void);
/**
 * @brief 初始化应用头
 * 
 * 应用头将被放置在 lv_layer_top() 层中
 * 
 * 隐藏应用头请使用`eos_app_header_hide`
 * 
 * 显示应用头请使用`eos_app_header_show`
 * 
 * @warning 应用头只能初始化一次
 */
void eos_app_header_init(void);
/**
 * @brief 将目标 screen 与应用头绑定，以便 screen 加载时显示应用头，screen 删除时隐藏应用头
 * @param scr 目标应用头
 * @param title 主题（一般是应用名称），可以通过`eos_app_header_set_title`进行修改
 */
void eos_screen_bind_header(lv_obj_t *scr, const char *title);
/**
 * @brief 向列表中添加指定像素高度的占位符
 * @param list 目标列表
 * @param height 占位符的高度(px)
 * @return lv_obj_t* 创建成功则返回创建的占位符对象指针
 * 
 * 创建失败则返回 NULL
 */
lv_obj_t *eos_list_add_placeholder(lv_obj_t *list, uint32_t height);
/**
 * @brief 向列表中添加一个开关
 * @param list 目标列表
 * @param txt 开关功能的描述
 * @return lv_obj_t* 创建成功则返回开关对象（标准 lv_switch 对象）
 * 
 * 创建失败则返回 NULL
 */
lv_obj_t *eos_list_add_switch(lv_obj_t *list, const char *txt);
/**
 * @brief 向列表中添加圆形图标的按钮
 * 
 * btn{  [icon]  [txt]  }
 * 
 * @param list 目标列表
 * @param circle_color 圆形图标的背景色
 * @param icon 图标
 * @param txt 按钮描述文字
 * @return lv_obj_t* 创建成功则返回按钮对象（标准 lv_button 对象）
 * 
 * 创建失败则返回 NULL
 */
lv_obj_t *eos_list_add_circle_icon_button(lv_obj_t *list, lv_color_t circle_color, const void *icon, const char *txt);
eos_list_slider_t *eos_list_add_slider(lv_obj_t *list, const char *txt);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_BASIC_WIDGETS_H */
