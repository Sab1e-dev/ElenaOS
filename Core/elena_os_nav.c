/**
 * @file elena_os_nav.c
 * @brief 导航栈 用于返回上级
 * @author Sab1e
 * @date 2025-08-16
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
    bool initialized;
} nav_stack_t;
// Variables
static nav_stack_t nav = {.top = -1, .initialized = false};
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
    return nav.initialized;
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

ElenaOSResult_t eos_nav_init(lv_obj_t *root_scr)
{
    if (!root_scr)
    {
        EOS_LOG_E("Root screen is NULL");
        return ELENA_OS_ERR_VAR_NULL;
    }

    if (_is_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack already initialized");
        return ELENA_OS_ERR_ALREADY_INITIALIZED;
    }

    // 设置root screen并初始化栈
    nav.stack[0] = root_scr;
    nav.top = 0;
    nav.initialized = true;

    // 确保root screen是活动屏幕
    lv_scr_load(root_scr);

    EOS_LOG_D("Nav stack initialized with root screen %p", root_scr);
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

    // 确保新屏幕与栈中已有屏幕地址不同
    for (int i = 0; i <= nav.top; i++)
    {
        if (nav.stack[i] == scr)
        {
            EOS_LOG_E("New screen address conflicts with existing screen!");
            lv_obj_del(scr);
            return NULL;
        }
    }

    EOS_LOG_D("NAV PUSH: new screen at %p", scr);
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
        EOS_LOG_D("Nav stack already empty (only root screen remains)");
        return ELENA_OS_OK;
    }

    // 从栈顶向下清理，跳过栈底(root screen)
    for (int i = nav.top; i > 0; i--)
    { // 注意从i>0开始
        if (nav.stack[i] != NULL)
        {
            lv_obj_t *scr = nav.stack[i];
            nav.stack[i] = NULL;
            lv_obj_del(scr);
            EOS_LOG_D("Cleared screen at stack position %d", i);
        }
    }

    // 重置栈顶指针（保留root screen在位置0）
    nav.top = 0;

    // 确保root screen是活动屏幕
    lv_scr_load(nav.stack[0]);

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
        EOS_LOG_E("Nav stack empty (cannot back from root screen)");
        return ELENA_OS_ERR_STACK_EMPTY;
    }

    // 保存要删除的页面（不能是root screen）
    lv_obj_t *scr_to_del = nav.stack[nav.top];

    // 执行回退
    ElenaOSResult_t ret = eos_nav_back();
    if (ret != ELENA_OS_OK)
    {
        return ret;
    }

    // 删除页面（确保不是root screen）
    if (scr_to_del && nav.top >= 0)
    { // nav.top >=0 确保不是root
        lv_obj_del(scr_to_del);
        EOS_LOG_D("Deleted screen at %p", scr_to_del);
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
        EOS_LOG_E("Already at root screen, cannot go back");
        return ELENA_OS_ERR_STACK_EMPTY;
    }

    lv_obj_t *prev_scr = _nav_peek_prev();
    if (!prev_scr)
    {
        // 如果无法获取前一个屏幕，回到root screen
        prev_scr = nav.stack[0];
    }

    // 更新栈指针
    nav.top--;

    // 加载前一个屏幕
    lv_scr_load(prev_scr);

    return ELENA_OS_OK;
}