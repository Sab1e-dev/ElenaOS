/**
 * @file elena_os_msg_list.c
 * @brief 下拉消息列表
 * @author Sab1e
 * @date 2025-08-13
 */

/**
 * TODO:
 * 未读标记
 * 点击放大
 * 文字较多时，用"..."替代
 * icon=64*64
 */

#include "elena_os_msg_list.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Macros and Definitions

// Static Variables

// Function Implementations
static lv_obj_t *msg_list;

// 事件处理函数
static void event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED)
    {
        LV_UNUSED(obj);
        LV_LOG_USER("Clicked: %s", lv_list_get_button_text(msg_list, obj));
    }
}

// 创建消息列表项
msg_list_item_t *elena_os_msg_list_item_create(lv_obj_t *parent)
{
    msg_list_item_t *item = lv_mem_alloc(sizeof(msg_list_item_t));
    if (!item)
        return NULL;
    memset(item, 0, sizeof(msg_list_item_t));

    /**
     * container
     * {
     *      (ROW1) row1 { icon  title     time }
     *      (ROW2) text (long warp)
     * }
     */

    // 创建容器
    item->container = lv_obj_create(parent);
    lv_obj_set_size(item->container, lv_pct(100), LV_SIZE_CONTENT);
    // 垂直排布
    lv_obj_set_flex_flow(item->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(item->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(item->container,lv_color_hex(0x212121),0);
    lv_obj_set_style_border_width(item->container, 0, 0);
    lv_obj_set_style_margin_bottom(item->container, 10, 0);
    lv_obj_set_style_pad_all(item->container, 20, 0);
    lv_obj_set_style_align(item->container, LV_ALIGN_CENTER, 0);
    lv_obj_set_style_radius(item->container, 30, 0);

    // 第一行
    lv_obj_t* row1 = lv_obj_create(item->container);\
    lv_obj_set_size(row1,lv_pct(100),64);
    // lv_obj_set_style_bg_color(row1,lv_color_hex(0xFF0000),0);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 5, 0);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);

    // 图标区域
    item->icon_area = lv_obj_create(row1);
    lv_obj_set_size(item->icon_area, 60, 60);
    lv_obj_set_style_bg_opa(item->icon_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item->icon_area, 0, 0);
    lv_obj_set_flex_align(item->icon_area, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(item->icon_area, LV_OBJ_FLAG_SCROLLABLE);

    // 标题
    item->title_label = lv_label_create(row1);
    lv_label_set_text(item->title_label, "Title");
    lv_obj_set_style_text_font(item->title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(item->title_label, lv_color_white(), 0);
    lv_label_set_long_mode(item->title_label,LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align_to(item->title_label,item->icon_area,LV_ALIGN_OUT_RIGHT_MID,12,0);
    
    // 时间
    item->time_label = lv_label_create(row1);
    lv_label_set_text(item->time_label, "10:30 AM");
    lv_obj_set_style_text_font(item->time_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(item->time_label, lv_color_white(), 0);
    lv_obj_align(item->time_label, LV_ALIGN_RIGHT_MID, 0,0);

    // 消息内容
    item->msg_label = lv_label_create(item->container);
    lv_label_set_text(item->msg_label, "Message content");
    lv_obj_set_style_text_font(item->msg_label, &lv_font_montserrat_24, 0);
    lv_obj_set_width(item->msg_label,lv_pct(100));
    lv_label_set_long_mode(item->msg_label,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(item->msg_label, lv_color_white(), 0);
    lv_obj_set_style_margin_all(item->msg_label, 5, 0);
    lv_obj_align_to(item->msg_label,row1, LV_ALIGN_BOTTOM_MID,0,0);

    // 未读标记（默认隐藏）
    item->unread_mark = lv_obj_create(item->container);
    lv_obj_set_size(item->unread_mark, 8, 8);
    lv_obj_set_style_bg_color(item->unread_mark, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_radius(item->unread_mark, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(item->unread_mark, LV_OBJ_FLAG_HIDDEN);

    return item;
}

// 删除消息列表项
void elena_os_msg_list_item_delete(msg_list_item_t *item)
{
    if (!item)
        return;
    lv_obj_del(item->container);
    lv_mem_free(item);
}

// 设置消息内容
void elena_os_msg_list_item_set_msg(msg_list_item_t *item, const char *msg)
{
    if (!item)
        return;
    lv_label_set_text(item->msg_label, msg ? msg : "");
}

// 设置标题
void elena_os_msg_list_item_set_title(msg_list_item_t *item, const char *title)
{
    if (!item)
        return;
    lv_label_set_text(item->title_label, title ? title : "");
}

// 设置时间
void elena_os_msg_list_item_set_time(msg_list_item_t *item, const char *time)
{
    if (!item)
        return;
    lv_label_set_text(item->time_label, time ? time : "");
}

// 设置图标
void elena_os_msg_list_item_set_icon_obj(msg_list_item_t *item, lv_obj_t *icon_obj)
{
    if (!item || !icon_obj)
        return;

    // 移除原有图标对象
    lv_obj_t *old_icon = lv_obj_get_child(item->icon_area, 0);
    if (old_icon)
    {
        lv_obj_del(old_icon);
    }

    // 将新图标添加到容器中
    lv_obj_set_parent(icon_obj, item->icon_area);
    lv_obj_center(icon_obj);
}
void elena_os_msg_list_item_set_icon_dsc(msg_list_item_t *item, const lv_img_dsc_t *icon_dsc)
{
    if (!item)
        return;
    if (!icon_dsc)
        return;
    lv_obj_t *img = lv_img_create(item->icon_area);
    lv_img_set_src(item->icon_area, icon_dsc);
    lv_obj_center(img);
    /**
     * TODO: 设置默认图标
     */
    // lv_img_set_src(item->icon_img, icon_dsc ? icon_dsc : &lv_img_dsc_default);
}

// 设置未读状态
void elena_os_msg_list_item_set_unread(msg_list_item_t *item, bool unread)
{
    if (!item)
        return;
    if (unread)
    {
        lv_obj_clear_flag(item->unread_mark, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(item->unread_mark, LV_OBJ_FLAG_HIDDEN);
    }
}

// 创建消息列表
msg_list_t *elena_os_msg_list_create(lv_obj_t *parent)
{
    msg_list_t *list = lv_mem_alloc(sizeof(msg_list_t));
    if (!list)
        return NULL;

    list->drag_item = elena_os_drag_item_create(parent);
    elena_os_drag_item_set_dir(list->drag_item, DRAG_DIR_DOWN);

    list->list = lv_list_create(list->drag_item->drag_obj);
    lv_obj_set_size(list->list, LV_PCT(100), LV_PCT(100));
    lv_obj_center(list->list);
    lv_obj_set_style_bg_opa(list->list,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(list->list, 0, 0);
    lv_obj_set_style_pad_all(list->list, 30, 0);

    return list;
}