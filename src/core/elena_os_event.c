/**
 * @file elena_os_event.c
 * @brief 事件系统
 * @author Sab1e
 * @date 2025-08-16
 */

#include "elena_os_event.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "elena_os_log.h"
// Macros and Definitions
/**
 * @brief 事件回调节点结构
 */
typedef struct _event_node_t
{
    lv_obj_t *obj;
    lv_event_code_t event;
    lv_event_cb_t cb;
    void *user_data;
    struct _event_node_t *next;
} event_node_t;
// Variables
static event_node_t *event_list_head = NULL; // 事件链表头
/************************** 事件定义 **************************/
static uint32_t event_list[EOS_EVENT_MAX_NUMBER] = {0};
// Function Implementations
/**
 * @brief 对象删除回调
 */
static void _obj_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);

    // 移除该对象的所有回调
    event_node_t **curr = &event_list_head;
    while (*curr)
    {
        if ((*curr)->obj == obj)
        {
            event_node_t *tmp = *curr;
            *curr = (*curr)->next;
            lv_free(tmp);
        }
        else
        {
            curr = &(*curr)->next;
        }
    }
}

void eos_event_init(void)
{
    for (uint32_t i = 0; i < EOS_EVENT_MAX_NUMBER; i++)
    {
        event_list[i] = lv_event_register_id();
    }
}

uint32_t eos_event_get_code(eos_event_t e)
{
    return event_list[e];
}

void eos_event_add_cb(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t event, void *user_data)
{
    if (!obj || !cb)
    {
        EOS_LOG_E("Invalid arguments for eos_event_add_cb");
        return;
    }

    // 创建新节点
    event_node_t *new_node = lv_malloc(sizeof(event_node_t));
    if (!new_node)
    {
        EOS_LOG_E("Failed to allocate event node");
        return;
    }

    new_node->obj = obj;
    new_node->event = event;
    new_node->cb = cb;
    new_node->user_data = user_data;
    new_node->next = NULL;

    // 添加到链表头部
    new_node->next = event_list_head;
    event_list_head = new_node;

    // 向LVGL注册事件回调
    lv_obj_add_event_cb(obj, cb, event, user_data);

    // 确保有删除回调
    lv_obj_add_event_cb(obj, _obj_delete_cb, LV_EVENT_DELETE, NULL);
}

void eos_event_remove_cb(lv_obj_t *obj, lv_event_code_t event, lv_event_cb_t cb)
{
    event_node_t **curr = &event_list_head;

    while (*curr)
    {
        if ((*curr)->obj == obj && (*curr)->event == event && (*curr)->cb == cb)
        {
            event_node_t *tmp = *curr;
            *curr = (*curr)->next;

            // 从LVGL中移除回调
            lv_obj_remove_event_cb(obj, cb);

            lv_free(tmp);
            return;
        }
        curr = &(*curr)->next;
    }
}

void eos_event_broadcast(lv_event_code_t event, void *param)
{
    event_node_t *curr = event_list_head;

    while (curr)
    {
        if (curr->event == event && lv_obj_is_valid(curr->obj))
        {
            // 使用lv_obj_send_event发送事件
            lv_result_t res = lv_obj_send_event(curr->obj, event, param);
            if (res != LV_RESULT_OK)
            {
                EOS_LOG_W("Event %d send failed for obj %p", event, curr->obj);
            }
        }
        curr = curr->next;
    }
}