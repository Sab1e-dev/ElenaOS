/**
 * @file elena_os_nav.c
 * @brief 导航栈 用于返回上级
 * @author Sab1e
 * @date 2025-08-16
 */

/**
 * TODO:
 * 第一次可以正常进入scr 并且可以正常退出，第二次会卡死
 * 
 */

#include "elena_os_nav.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "elena_os_log.h"
// Macros and Definitions
#define NAV_STACK_SIZE 32
/**
 * @brief 导航栈结构体
 */
typedef struct
{
    lv_obj_t *stack[NAV_STACK_SIZE];
    int8_t top;
} nav_stack_t;
// Variables
static nav_stack_t nav = {.top = -1};
// Function Implementations
static lv_obj_t *_nav_peek_prev(void);
static bool _is_nav_stack_initialized(void);
static bool _is_nav_stack_full(void);
static bool _is_nav_stack_empty(void);

/**
 * @brief 检查导航栈是否已初始化
 */
static bool _is_nav_stack_initialized(void)
{
    return nav.top != -1;
}

/**
 * @brief 检查导航栈是否已满
 */
static bool _is_nav_stack_full(void)
{
    return nav.top >= NAV_STACK_SIZE - 1;
}

/**
 * @brief 检查导航栈是否为空
 */
static bool _is_nav_stack_empty(void)
{
    return nav.top < 0;
}

/**
 * @brief 获取前一个页面对象
 */
static lv_obj_t *_nav_peek_prev(void)
{
    EOS_LOG_D("NAV PREV");
    return (nav.top > 0) ? nav.stack[nav.top - 1] : NULL;
}

ElenaOSResult_t eos_nav_init(lv_obj_t *scr)
{
    if (!scr)
    {
        EOS_LOG_E("Init screen is NULL");
        return ELENA_OS_ERR_VAR_NULL;
    }

    if (_is_nav_stack_initialized())
    {
        // 清空栈
        for (int i = NAV_STACK_SIZE; i >= 0; i--)
        {
            if (nav.stack[i] != NULL)
            {
                lv_obj_t *scr = nav.stack[i];
                nav.stack[i] = NULL; // 先置NULL再删除，避免悬垂指针
                lv_obj_del(scr);
                EOS_LOG_D("Cleared screen at stack position %d", i);
            }
        }
    }

    // 重置栈
    nav.top = -1;

    // 保证栈底有 scr 可用
    nav.stack[0] = scr;
    nav.top++; // top = 0
    return ELENA_OS_OK;
}

lv_obj_t *eos_nav_scr_create(void)
{
    if (!_is_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack not initialized");
        return NULL;
    }

    if (_is_nav_stack_full())
    {
        EOS_LOG_E("Nav stack full");
        return NULL;
    }

    lv_obj_t *scr = lv_obj_create(NULL);
    if (!scr)
    {
        EOS_LOG_E("Create screen failed.");
        return NULL;
    }

    EOS_LOG_D("NAV PUSH");
    nav.stack[++nav.top] = scr;
    return scr;
}

ElenaOSResult_t eos_nav_clear_stack(void)
{
    if (!_is_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack not initialized");
        return ELENA_OS_ERR_NOT_INITIALIZED;
    }

    if (_is_nav_stack_empty())
    {
        EOS_LOG_D("Nav stack already empty");
        return ELENA_OS_OK;
    }

    // 从栈顶向下清理
    for (int i = nav.top; i >= 0; i--)
    {
        if (nav.stack[i] != NULL)
        {
            lv_obj_t *scr = nav.stack[i];
            nav.stack[i] = NULL; // 先置NULL再删除，避免悬垂指针
            lv_obj_del(scr);
            EOS_LOG_D("Cleared screen at stack position %d", i);
        }
    }

    nav.top = -1; // 重置栈
    return ELENA_OS_OK;
}

ElenaOSResult_t eos_nav_back_clear(void)
{
    if (!_is_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack not initialized");
        return ELENA_OS_ERR_NOT_INITIALIZED;
    }

    if (_is_nav_stack_empty())
    {
        EOS_LOG_E("Nav stack empty");
        return ELENA_OS_ERR_STACK_EMPTY;
    }

    // 保存要删除的页面
    lv_obj_t *scr_to_del = nav.stack[nav.top];

    ElenaOSResult_t ret = eos_nav_back();
    if (ret != ELENA_OS_OK)
    {
        EOS_LOG_E("Nav can't back.");
        return ret;
    }

    // 删除页面
    if (scr_to_del)
    {
        lv_obj_del(scr_to_del);
    }

    return ELENA_OS_OK;
}

ElenaOSResult_t eos_nav_back(void)
{
    if (!_is_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack not initialized");
        return ELENA_OS_ERR_NOT_INITIALIZED;
    }

    if (_is_nav_stack_empty())
    {
        EOS_LOG_E("Nav stack empty!");
        return ELENA_OS_ERR_STACK_EMPTY;
    }

    lv_obj_t *prev_scr = _nav_peek_prev();
    if (!prev_scr)
    {
        EOS_LOG_E("No previous screen");
        return ELENA_OS_ERR_VAR_NULL;
    }

    nav.top--;
    lv_scr_load(prev_scr);
    return ELENA_OS_OK;
}