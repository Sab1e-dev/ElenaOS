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
#include <stdatomic.h>
#include "elena_os_log.h"
#include "elena_os_core.h"
#include "script_engine_core.h"
#include "script_engine_nav.h"
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
/**
 * @brief 导航正忙，等待响应
 */
#define NAV_BUSY_WAIT()                                                   \
    do                                                                    \
    {                                                                     \
        bool expected = false;                                            \
        while (!atomic_compare_exchange_weak(&nav_busy, &expected, true)) \
        {                                                                 \
            expected = false;                                             \
            EOS_LOG_D("Waiting for nav stack to be available");           \
            lv_timer_handler();                                           \
        }                                                                 \
    } while (0)
/**
 * @brief 导航正忙，取消操作
 */
#define NAV_BUSY_CHECK()                                                 \
    do                                                                   \
    {                                                                    \
        bool expected = false;                                           \
        if (!atomic_compare_exchange_strong(&nav_busy, &expected, true)) \
        {                                                                \
            EOS_LOG_D("Nav stack busy, operation rejected");             \
            return -EOS_ERR_BUSY;                                        \
        }                                                                \
    } while (0)
// Variables
static nav_stack_t nav = {.top = -1, .initialized = false};
static atomic_bool nav_busy = false; // 正在清除
// Function Implementations
static lv_obj_t *_nav_peek_prev(void);
static bool _is_nav_stack_initialized(void);
static bool _is_nav_stack_full(void);
static bool _is_nav_stack_empty(void);
eos_result_t eos_nav_back_clean(void);
extern lv_style_t style_screen;
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

eos_result_t eos_nav_init(lv_obj_t *root_scr)
{
    if (!root_scr)
    {
        EOS_LOG_E("Root screen is NULL");
        return -EOS_ERR_VAR_NULL;
    }

    if (_is_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack already initialized");
        return -EOS_ERR_ALREADY_INITIALIZED;
    }

    // 设置root screen并初始化栈
    nav.stack[0] = root_scr;
    nav.top = 0;
    nav.initialized = true;

    // 确保root screen是活动屏幕
    lv_scr_load(root_scr);
    lv_obj_add_style(root_scr, &style_screen, 0);
    EOS_LOG_D("Nav stack initialized with root screen %p", root_scr);
    return EOS_OK;
}

lv_obj_t *eos_nav_scr_create(void)
{
    if (nav_busy)
    {
        EOS_LOG_E("Nav stack busy");
        return NULL;
    }
    atomic_store(&nav_busy, true);

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
    lv_obj_add_style(scr, &style_screen, 0);
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
    EOS_MEM("Create new scr");
    atomic_store(&nav_busy, false);
    return scr;
}

eos_result_t eos_nav_clear_stack(void)
{
    if (nav_busy)
    {
        EOS_LOG_E("Nav stack busy");
        return -EOS_ERR_BUSY;
    }
    atomic_store(&nav_busy, true);

    if (!_is_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack not initialized");
        return -EOS_ERR_NOT_INITIALIZED;
    }

    if (_is_nav_stack_empty())
    {
        EOS_LOG_D("Nav stack already empty (only root screen remains)");
        return EOS_OK;
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
    atomic_store(&nav_busy, false);
    return EOS_OK;
}

eos_result_t eos_nav_back_clean(void)
{
    NAV_BUSY_CHECK();

    if (!_is_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack not initialized");
        atomic_store(&nav_busy, false);
        return -EOS_ERR_NOT_INITIALIZED;
    }

    if (_is_nav_stack_empty())
    {
        EOS_LOG_E("Nav stack empty (cannot back from root screen)");
        atomic_store(&nav_busy, false);
        return -EOS_ERR_STACK_EMPTY;
    }
    if (nav.top == 0)
    {
        EOS_LOG_E("Root screen");
        return -EOS_FAILED;
    }
    // 直接执行回退逻辑
    lv_obj_t *prev_scr = _nav_peek_prev();
    if (!prev_scr)
    {
        prev_scr = nav.stack[0]; // 回退到 root screen
    }

    // 保存要删除的页面
    lv_obj_t *scr_to_del = nav.stack[nav.top];
    nav.top--; // 更新栈指针

    // 删除页面
    if (scr_to_del)
    {
        lv_obj_del(scr_to_del);
        EOS_LOG_D("Deleted screen at %p", scr_to_del);
    }

    // 加载前一个屏幕
    lv_scr_load(prev_scr);

    EOS_MEM("Clear scr");
    atomic_store(&nav_busy, false);
    return EOS_OK;
}

eos_result_t eos_nav_back(void)
{
    NAV_BUSY_CHECK();

    if (!_is_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack not initialized");
        atomic_store(&nav_busy, false);
        return -EOS_ERR_NOT_INITIALIZED;
    }

    if (_is_nav_stack_empty())
    {
        EOS_LOG_E("Already at root screen, cannot go back");
        atomic_store(&nav_busy, false);
        return -EOS_ERR_STACK_EMPTY;
    }

    lv_obj_t *prev_scr = _nav_peek_prev();
    if (!prev_scr)
    {
        prev_scr = nav.stack[0]; // 回退到 root screen
    }

    nav.top--; // 更新栈指针
    lv_scr_load(prev_scr);

    atomic_store(&nav_busy, false);
    return EOS_OK;
}