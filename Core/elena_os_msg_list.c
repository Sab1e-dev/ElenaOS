/**
 * @file elena_os_msg_list.c
 * @brief 下拉消息列表
 * @author Sab1e
 * @date 2025-08-13
 */

/**
 * TODO:
 * 滑动删除
 */

#include "elena_os_msg_list.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elena_os_lang.h"
#include "elena_os_anim.h"
#include "elena_os_log.h"
#include "mem_mgr.h"
// Macros and Definitions
#define MSG_LIST_ITEM_MARGIN_BOTTOM 25
#define SCREEN_W lv_disp_get_hor_res(NULL)
#define SCREEN_H lv_disp_get_ver_res(NULL)
typedef struct
{
    msg_list_item_t *item;
    msg_list_t *list;
    lv_obj_t *detail_container;
} btn_data_t;
// Variables

// Function Implementations
static void _del_item_cb(msg_list_t *list)
{
    uint8_t child_count = lv_obj_get_child_cnt(list->list);
    if(child_count<=1){
        // 显示无消息标签
        if (list->no_msg_label)
        {
            lv_obj_clear_flag(list->no_msg_label, LV_OBJ_FLAG_HIDDEN);
        }
        // 隐藏清除按钮
        if (list->clear_all_btn)
        {
            lv_obj_add_flag(list->clear_all_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
}

static void _anim_timeline_end_cb(eos_anim_t *a) {
    btn_data_t *data = (btn_data_t *)eos_anim_get_user_data(a);
    if (!data) return;

    // 1. 删除容器（自动删除所有子对象）
    if (data->detail_container) {
        lv_obj_del(data->detail_container);
    }

    // 释放数据内存
    lv_mem_free(data);
}

static void _mark_as_read_btn_click_cb(lv_event_t *e)
{
    // 获取事件的目标对象（mark_as_read_btn）
    lv_obj_t *btn = lv_event_get_target(e);

    // 获取按钮的用户数据（包含item和list）
    btn_data_t *data = (btn_data_t *)lv_event_get_user_data(e);
    if (!data)
        return;

    // 获取按钮的父容器（即详情页面container）
    lv_obj_t *container = lv_obj_get_parent(btn);

    eos_anim_t *anim = eos_anim_scale_create(container,SCREEN_W,0,SCREEN_H,0,500);
    eos_anim_set_cb(anim, _anim_timeline_end_cb, data);
    eos_anim_start(anim);

    // 删除消息项并更新UI状态
    if (data->item)
    {
        eos_msg_list_item_delete(data->item);
        if (data->list)
        {
            _del_item_cb(data->list);
        }
    }
}

static void _msg_list_item_pressed_cb(lv_event_t *e)
{
    // 获取被点击的消息项容器
    lv_obj_t *item_container = lv_event_get_target(e);
    msg_list_item_t *item = (msg_list_item_t *)lv_obj_get_user_data(item_container);
    msg_list_t *list = (msg_list_t *)lv_event_get_user_data(e);
    // 获取当前屏幕
    lv_obj_t *scr = lv_scr_act();

    // 创建详情页面容器
    lv_obj_t *detail_container = lv_obj_create(scr);
    lv_obj_center(detail_container);
    lv_obj_set_style_bg_color(detail_container, lv_color_hex(0x111111), 0);
    lv_obj_set_flex_flow(detail_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(detail_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(detail_container, 20, 0);
    lv_obj_set_style_border_width(detail_container, 0, 0);
    lv_obj_set_flex_align(
        detail_container,
        LV_FLEX_ALIGN_START,  // 主轴(竖直方向)居中
        LV_FLEX_ALIGN_CENTER, // 交叉轴(水平方向)
        LV_FLEX_ALIGN_CENTER  // 每行内子项的对齐方式
    );

    // 添加图标区域（复制原消息项的图标）
    lv_obj_t *icon_area = lv_obj_create(detail_container);
    lv_obj_set_size(icon_area, 80, 80);
    lv_obj_set_style_bg_opa(icon_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon_area, 0, 0);

    // 复制原消息项的图标
    lv_obj_t *original_icon = lv_obj_get_child(item->icon_area, 0);
    if (original_icon)
    {
        lv_obj_t *new_icon = lv_img_create(icon_area);
        lv_img_set_src(new_icon, lv_img_get_src(original_icon));
        lv_obj_center(new_icon);
        lv_obj_clear_flag(new_icon, LV_OBJ_FLAG_SCROLLABLE);
    }

    // 添加标题（复制原消息项的标题）
    lv_obj_t *title_label = lv_label_create(detail_container);
    lv_label_set_text(title_label, lv_label_get_text(item->title_label));
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_margin_top(title_label, 5, 0);

    // 添加消息内容（复制原消息项的内容）
    lv_obj_t *msg_label = lv_label_create(detail_container);
    lv_label_set_text(msg_label, item->msg_str);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(msg_label, lv_color_white(), 0);
    lv_obj_set_width(msg_label, lv_pct(90));
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_margin_top(msg_label, 10, 0);

    // 添加"标记为已读"按钮
    lv_obj_t *mark_as_read_btn = lv_btn_create(detail_container);
    lv_obj_set_size(mark_as_read_btn, lv_pct(80), 80);
    lv_obj_clear_flag(mark_as_read_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(mark_as_read_btn, lv_color_hex(0x212121), 0);
    lv_obj_set_style_border_width(mark_as_read_btn, 0, 0);
    lv_obj_set_style_margin_bottom(mark_as_read_btn, MSG_LIST_ITEM_MARGIN_BOTTOM, 0);
    lv_obj_set_style_pad_all(mark_as_read_btn, 0, 0);
    lv_obj_set_style_align(mark_as_read_btn, LV_ALIGN_CENTER, 0);
    lv_obj_set_style_radius(mark_as_read_btn, 50, 0);
    lv_obj_set_style_shadow_width(mark_as_read_btn, 0, 0);
    lv_obj_set_style_margin_top(msg_label, 10, 0);

    // 添加按钮标签
    lv_obj_t *btn_label = eos_lang_label_create(mark_as_read_btn, STR_ID_MSG_LIST_ITEM_MARK_AS_READ);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_center(btn_label);

    btn_data_t *data = lv_mem_alloc(sizeof(btn_data_t));
    data->item = item;
    data->list = list;
    data->detail_container = detail_container;

    // 为按钮添加点击事件，并将 item 作为用户数据传递
    lv_obj_add_event_cb(mark_as_read_btn, _mark_as_read_btn_click_cb, LV_EVENT_CLICKED, data);
    
    // 添加动画
    eos_anim_scale_start(detail_container,
                                0, SCREEN_W,
                                0, SCREEN_H,
                                150);
    
}

msg_list_item_t *eos_msg_list_item_create(msg_list_t *list)
{
    
    if (!list)
        return NULL;

    msg_list_item_t *item = lv_mem_alloc(sizeof(msg_list_item_t));
    if (!item)
        return NULL;
    memset(item, 0, sizeof(msg_list_item_t));

    // 创建容器
    item->container = lv_obj_create(list->list);
    lv_obj_set_size(item->container, lv_pct(100), LV_SIZE_CONTENT);
    // 垂直排布
    lv_obj_set_flex_flow(item->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(item->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(item->container, lv_color_hex(0x212121), 0);
    lv_obj_set_style_border_width(item->container, 0, 0);
    lv_obj_set_style_margin_bottom(item->container, MSG_LIST_ITEM_MARGIN_BOTTOM, 0);
    lv_obj_set_style_pad_all(item->container, 20, 0);
    lv_obj_set_style_align(item->container, LV_ALIGN_CENTER, 0);
    lv_obj_set_style_radius(item->container, 30, 0);
    lv_obj_set_user_data(item->container, item);
    lv_obj_set_style_translate_x(item->container, 0, 0);
    lv_obj_add_flag(item->container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item->container, _msg_list_item_pressed_cb, LV_EVENT_CLICKED, list);

    // 第一行
    lv_obj_t *row1 = lv_obj_create(item->container);
    lv_obj_set_size(row1, lv_pct(100), 64);
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
    lv_obj_set_style_text_font(item->title_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(item->title_label, lv_color_white(), 0);
    lv_label_set_long_mode(item->title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align_to(item->title_label, item->icon_area, LV_ALIGN_OUT_RIGHT_MID, 12, 0);

    // 时间
    item->time_label = lv_label_create(row1);
    lv_obj_set_style_text_font(item->time_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(item->time_label, lv_color_white(), 0);
    lv_obj_align(item->time_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // 消息内容
    item->msg_label = lv_label_create(item->container);
    lv_obj_set_style_text_font(item->msg_label, &lv_font_montserrat_28, 0);
    lv_obj_set_width(item->msg_label, lv_pct(100));
    lv_label_set_long_mode(item->msg_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(item->msg_label, lv_color_white(), 0);
    lv_obj_set_style_margin_all(item->msg_label, 5, 0);
    lv_obj_align_to(item->msg_label, row1, LV_ALIGN_BOTTOM_MID, 0, 0);

    // 设置忽略事件，避免影响 container 的 Clicked 事件
    lv_obj_clear_flag(item->icon_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(item->title_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(item->time_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(item->msg_label, LV_OBJ_FLAG_CLICKABLE);

    // 当添加新消息时处理UI状态
    if (list->no_msg_label)
    {
        lv_obj_add_flag(list->no_msg_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (list->clear_all_btn)
    {
        lv_obj_clear_flag(list->clear_all_btn, LV_OBJ_FLAG_HIDDEN);
    }
    
    return item;
}

void eos_msg_list_item_delete(msg_list_item_t *item)
{
    if (!item) return;

    // 释放消息字符串
    if (item->msg_str) {
        lv_mem_free(item->msg_str);
        item->msg_str = NULL;
    }

    // 直接删除容器（LVGL会自动删除子对象）
    if (item->container) {
        lv_obj_del(item->container);
    }

    // 释放结构体
    lv_mem_free(item);
}

void eos_msg_list_item_set_msg(msg_list_item_t *item, const char *msg)
{
    if (!item) return;

    // 释放旧消息
    if (item->msg_str) {
        lv_mem_free(item->msg_str);
        item->msg_str = NULL;
    }

    if (!msg || strlen(msg) == 0) {
        lv_label_set_text(item->msg_label, "");
    } else {
        // 分配新内存并复制
        item->msg_str = lv_mem_alloc(strlen(msg) + 1);
        if (item->msg_str) {
            strcpy(item->msg_str, msg);
            lv_label_set_text(item->msg_label, item->msg_str);
        } else {
            lv_label_set_text(item->msg_label, "");
        }
    }
}

void eos_msg_list_item_set_title(msg_list_item_t *item, const char *title)
{
    if (!item)
        return;
    lv_label_set_text(item->title_label, title ? title : "");
}

void eos_msg_list_item_set_time(msg_list_item_t *item, const char *time)
{
    if (!item)
        return;
    lv_label_set_text(item->time_label, time ? time : "");
}

void eos_msg_list_item_set_icon_obj(msg_list_item_t *item, lv_obj_t *icon_obj)
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

void eos_msg_list_item_set_icon_dsc(msg_list_item_t *item, const lv_img_dsc_t *icon_dsc)
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

void eos_msg_list_clear_all(msg_list_t *list)
{
    if (!list || !list->list)
        return;

    // 获取所有子对象
    uint32_t child_count = lv_obj_get_child_cnt(list->list);
    lv_obj_t **children = lv_mem_alloc(sizeof(lv_obj_t *) * child_count);

    // 保存子对象指针（避免动态删除影响遍历）
    for (uint32_t i = 0; i < child_count; i++)
    {
        children[i] = lv_obj_get_child(list->list, i);
    }

    // 删除子对象
    for (uint32_t i = 0; i < child_count; i++)
    {
        lv_obj_t *child = children[i];
        if (child == list->clear_all_btn)
        {
            continue;
        }
        msg_list_item_t *item = (msg_list_item_t *)lv_obj_get_user_data(child);
        if (item)
        {
            eos_msg_list_item_delete(item);
        }
        else
        {
            lv_obj_del(child);
        }
    }

    lv_mem_free(children);
}

static void _msg_list_item_anim_completed_cb(lv_anim_t *a)
{
    msg_list_t *list = (msg_list_t *)lv_anim_get_user_data(a);

    // 减少动画计数
    if (list->animating_count > 0)
    {
        list->animating_count--;
    }

    // 检查是否所有动画都完成了
    if (list->animating_count == 0)
    {
        // 直接清除所有内容
        eos_msg_list_clear_all(list);
        eos_drag_item_pull_back(list->drag_item);
        _del_item_cb(list);
    }
}

static void _anime_exec_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0);
}

static void _trigger_msg_anims(msg_list_t *list)
{
    static uint8_t anim_index = 0; // 静态变量记录当前动画序号
    lv_obj_t *parent = list->list;
    uint8_t child_count = lv_obj_get_child_cnt(parent);

    // 每次只处理一个消息项的动画
    for (uint8_t i = anim_index; i < child_count && list->animating_count < 2; i++)
    {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        msg_list_item_t *item = (msg_list_item_t *)lv_obj_get_user_data(child);

        // 跳过清除按钮本身
        if (child == list->clear_all_btn)
        {
            anim_index++;
            continue;
        }

        if (item)
        {
            // 创建动画
            lv_anim_t anim;
            lv_anim_init(&anim);
            lv_anim_set_var(&anim, item->container);
            lv_anim_set_values(&anim, lv_obj_get_x(item->container), SCREEN_W);
            lv_anim_set_time(&anim, 120);
            lv_anim_set_exec_cb(&anim, _anime_exec_cb);
            lv_anim_set_user_data(&anim, list);
            lv_anim_set_ready_cb(&anim, _msg_list_item_anim_completed_cb);
            lv_anim_start(&anim);

            list->animating_count++;
            anim_index++;
        }
    }

    anim_index = 0;
}

static void _msg_list_clear_all_btn_cb(lv_event_t *e)
{
    msg_list_t *list = (msg_list_t *)lv_event_get_user_data(e);

    // 隐藏无消息标签
    if (list->no_msg_label)
    {
        lv_obj_add_flag(list->no_msg_label, LV_OBJ_FLAG_HIDDEN);
    }

    // 重置动画计数
    list->animating_count = 0;

    // 触发动画
    _trigger_msg_anims(list);
}

msg_list_t *eos_msg_list_create(lv_obj_t *parent)
{
    msg_list_t *list = lv_mem_alloc(sizeof(msg_list_t));
    if (!list)
        return NULL;

    memset(list, 0, sizeof(msg_list_t));

    list->drag_item = eos_drag_item_create(parent);
    eos_drag_item_set_dir(list->drag_item, DRAG_DIR_DOWN);

    list->list = lv_list_create(list->drag_item->drag_obj);
    lv_obj_set_size(list->list, LV_PCT(100), LV_PCT(88));
    lv_obj_center(list->list);
    lv_obj_set_style_bg_opa(list->list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list->list, 0, 0);
    lv_obj_set_style_pad_all(list->list, 30, 0);
    lv_obj_align(list->list, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_user_data(list->list, list);
    lv_obj_set_scroll_dir(list->list, LV_DIR_VER);

    // 创建清除所有按钮
    list->clear_all_btn = lv_btn_create(list->list);
    lv_obj_set_size(list->clear_all_btn, lv_pct(100), 80);
    lv_obj_clear_flag(list->clear_all_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(list->clear_all_btn, lv_color_hex(0x212121), 0);
    lv_obj_set_style_border_width(list->clear_all_btn, 0, 0);
    lv_obj_set_style_margin_bottom(list->clear_all_btn, MSG_LIST_ITEM_MARGIN_BOTTOM, 0);
    lv_obj_set_style_pad_all(list->clear_all_btn, 0, 0);
    lv_obj_set_style_align(list->clear_all_btn, LV_ALIGN_CENTER, 0);
    lv_obj_set_style_radius(list->clear_all_btn, 50, 0);
    lv_obj_set_style_shadow_width(list->clear_all_btn, 0, 0);

    // 初始时隐藏清除按钮
    lv_obj_add_flag(list->clear_all_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *clear_all_label = eos_lang_label_create(list->clear_all_btn, STR_ID_MSG_LIST_CLEAR_ALL);
    lv_obj_set_style_text_font(clear_all_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(clear_all_label, lv_color_white(), 0);
    lv_obj_center(clear_all_label);

    lv_obj_add_event_cb(list->clear_all_btn, _msg_list_clear_all_btn_cb, LV_EVENT_CLICKED, list);

    // 创建无消息标签
    list->no_msg_label = eos_lang_label_create(list->drag_item->drag_obj, STR_ID_MSG_LIST_NO_MSG);
    lv_obj_set_style_text_font(list->no_msg_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(list->no_msg_label, lv_color_hex(0xA6A6A6), 0);
    lv_obj_center(list->no_msg_label);

    return list;
}