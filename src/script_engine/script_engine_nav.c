/**
 * @file script_engine_nav.c
 * @brief 脚本引擎的导航栈
 * @author Sab1e
 * @date 2025-08-24
 */

#include "script_engine_nav.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "elena_os_log.h"
// Macros and Definitions
#define NAV_STACK_SIZE 32

/**
 * @brief 导航栈结构体
 */
typedef struct
{
    lv_obj_t *stack[NAV_STACK_SIZE]; // [0]: base_scr, [1]: root_scr, [2+]: other screens
    int8_t top;
    bool initialized;
} script_nav_stack_t;

/**
 * @brief 导航正忙，等待响应
 */
#define NAV_BUSY_WAIT()                                                          \
    do                                                                           \
    {                                                                            \
        bool expected = false;                                                   \
        while (!atomic_compare_exchange_weak(&script_nav_busy, &expected, true)) \
        {                                                                        \
            expected = false;                                                    \
            EOS_LOG_D("Waiting for script_nav stack to be available");           \
            lv_timer_handler();                                                  \
        }                                                                        \
    } while (0)

/**
 * @brief 导航正忙，取消操作
 */
#define NAV_BUSY_CHECK()                                                        \
    do                                                                          \
    {                                                                           \
        bool expected = false;                                                  \
        if (!atomic_compare_exchange_strong(&script_nav_busy, &expected, true)) \
        {                                                                       \
            EOS_LOG_D("Nav stack busy, operation rejected");                    \
            return -SE_ERR_BUSY;                                                \
        }                                                                       \
    } while (0)

// Variables
static script_nav_stack_t script_nav = {.top = -1, .initialized = false};
static atomic_bool script_nav_busy = false; // 正在清除

// Function Implementations
static lv_obj_t *_script_nav_peek_prev(void);
bool is_script_nav_stack_initialized(void);
static bool _is_script_nav_stack_full(void);
static bool _is_script_nav_stack_empty(void);
script_engine_result_t script_engine_nav_back_clean(void);
static script_engine_result_t _script_engine_nav_clear_stack(void);

/**
 * @brief 检查导航栈是否已初始化
 */
bool is_script_nav_stack_initialized(void)
{
    return script_nav.initialized;
}

/**
 * @brief 检查导航栈是否已满
 */
static bool _is_script_nav_stack_full(void)
{
    return script_nav.top >= NAV_STACK_SIZE - 1;
}

/**
 * @brief 检查导航栈是否为空
 */
static bool _is_script_nav_stack_empty(void)
{
    return script_nav.top < 0;
}

/**
 * @brief 获取前一个页面对象
 */
static lv_obj_t *_script_nav_peek_prev(void)
{
    EOS_LOG_D("NAV PREV");
    return (script_nav.top > 0) ? script_nav.stack[script_nav.top - 1] : NULL;
}

static void _script_nav_gesture_back_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    EOS_LOG_D("GESTURE: ");
    if (dir == LV_DIR_LEFT)
    {
        EOS_LOG_D("LV_DIR_LEFT");
        script_engine_nav_back_clean();
    }
}

/**
 * @brief 清除整个导航栈
 */
static script_engine_result_t _script_engine_nav_clear_stack(void)
{
    if (script_nav_busy)
    {
        EOS_LOG_E("Nav stack busy");
        return -SE_ERR_BUSY;
    }
    atomic_store(&script_nav_busy, true);

    if (!is_script_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack not initialized");
        atomic_store(&script_nav_busy, false);
        return -SE_ERR_NOT_INITIALIZED;
    }

    // 从栈顶向下清理所有非base_scr的screen
    for (int i = script_nav.top; i >= 1; i--) // 从栈顶开始处理到索引1（跳过base_scr）
    {
        if (script_nav.stack[i] != NULL)
        {
            lv_obj_t *scr = script_nav.stack[i];
            script_nav.stack[i] = NULL; // 清除指针
            lv_obj_del(scr);            // 彻底删除screen
            EOS_LOG_D("Cleared screen at stack position %d", i);
        }
    }

    lv_obj_t *base_scr = script_nav.stack[0];

    // 重置栈状态（只保留base_scr）
    for (int i = 0; i < NAV_STACK_SIZE; i++)
    {
        script_nav.stack[i] = NULL;
    }

    // 加载base_scr
    lv_scr_load(base_scr);
    script_nav.top = -1;
    script_nav.initialized = false;
    atomic_store(&script_nav_busy, false);

    EOS_LOG_D("Nav stack completely cleared. Only base screen remains");
    return SE_OK;
}

void script_engine_nav_clean_up()
{
    _script_engine_nav_clear_stack();
}

/**
 * @brief 初始化导航栈
 * @param base_scr 基础页面（不会被删除）
 */
script_engine_result_t script_engine_nav_init(lv_obj_t *base_scr)
{
    if (!base_scr)
    {
        EOS_LOG_E("Base screen is NULL");
        return -SE_ERR_VAR_NULL;
    }

    if (is_script_nav_stack_initialized())
    {
        script_engine_nav_clean_up();
    }

    // 创建root_scr（脚本的根页面）
    lv_obj_t *root_scr = lv_obj_create(NULL);
    if (!root_scr)
    {
        EOS_LOG_E("Create root screen failed.");
        return -SE_ERR_MALLOC;
    }

    // 初始化栈：stack[0] = base_scr, stack[1] = root_scr
    script_nav.stack[0] = base_scr;
    script_nav.stack[1] = root_scr;
    script_nav.top = 1; // 栈顶索引为1
    script_nav.initialized = true;

    // 加载root_scr（脚本的根页面）
    lv_scr_load(root_scr);
    lv_obj_add_event_cb(root_scr, _script_nav_gesture_back_cb, LV_EVENT_GESTURE, NULL);
    EOS_LOG_D("Nav stack initialized: base_scr=%p, root_scr=%p", base_scr, root_scr);
    return SE_OK;
}

/**
 * @brief 创建新的导航页面
 */
lv_obj_t *script_engine_nav_scr_create(void)
{
    if (script_nav_busy)
    {
        EOS_LOG_E("Nav stack busy");
        return NULL;
    }
    atomic_store(&script_nav_busy, true);

    if (!is_script_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack not initialized");
        return NULL;
    }

    if (_is_script_nav_stack_full())
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
    for (int i = 0; i <= script_nav.top; i++)
    {
        if (script_nav.stack[i] == scr)
        {
            EOS_LOG_E("New screen address conflicts with existing screen!");
            lv_obj_del(scr);
            return NULL;
        }
    }

    EOS_LOG_D("NAV PUSH: new screen at %p", scr);
    script_nav.stack[++script_nav.top] = scr;
    lv_obj_add_event_cb(scr, _script_nav_gesture_back_cb, LV_EVENT_GESTURE, NULL);
    EOS_MEM("Create new scr");
    atomic_store(&script_nav_busy, false);
    return scr;
}

/**
 * @brief 返回并清理当前页面
 */
script_engine_result_t script_engine_nav_back_clean(void)
{
    NAV_BUSY_CHECK();

    if (!is_script_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack not initialized");
        atomic_store(&script_nav_busy, false);
        return -SE_ERR_NOT_INITIALIZED;
    }

    if (_is_script_nav_stack_empty())
    {
        EOS_LOG_E("Nav stack empty (cannot back from root screen)");
        atomic_store(&script_nav_busy, false);
        return -SE_ERR_STACK_EMPTY;
    }

    // 如果当前在root_scr（top==1），则清理整个栈
    if (script_nav.top == 1)
    {
        // 停止脚本引擎
        if (script_engine_get_state() == SCRIPT_STATE_RUNNING)
        {
            script_engine_request_stop();
        }
        EOS_LOG_D("Back to base_scr and cleared root_scr");
        EOS_MEM("Clear root_scr and back to base");
        atomic_store(&script_nav_busy, false);
        return SE_OK;
    }

    // 其他页面正常返回逻辑
    lv_obj_t *prev_scr = _script_nav_peek_prev();
    if (!prev_scr)
    {
        prev_scr = script_nav.stack[0]; // 回退到 base screen
    }

    // 保存要删除的页面
    lv_obj_t *scr_to_del = script_nav.stack[script_nav.top];
    script_nav.top--; // 更新栈指针

    // 加载前一个屏幕
    lv_scr_load(prev_scr);

    // 删除页面
    if (scr_to_del)
    {
        lv_obj_del(scr_to_del);
        EOS_LOG_D("Deleted screen at %p", scr_to_del);
    }

    EOS_MEM("Clear scr");
    atomic_store(&script_nav_busy, false);
    return SE_OK;
}

/**
 * @brief 返回但不删除当前页面
 */
script_engine_result_t script_engine_nav_back(void)
{
    NAV_BUSY_CHECK();

    if (!is_script_nav_stack_initialized())
    {
        EOS_LOG_E("Nav stack not initialized");
        atomic_store(&script_nav_busy, false);
        return -SE_ERR_NOT_INITIALIZED;
    }

    if (_is_script_nav_stack_empty())
    {
        EOS_LOG_E("Already at root screen, cannot go back");
        atomic_store(&script_nav_busy, false);
        return -SE_ERR_STACK_EMPTY;
    }

    lv_obj_t *prev_scr = _script_nav_peek_prev();
    if (!prev_scr)
    {
        prev_scr = script_nav.stack[0]; // 回退到 base screen
    }

    script_nav.top--; // 更新栈指针
    lv_scr_load(prev_scr);

    atomic_store(&script_nav_busy, false);
    return SE_OK;
}