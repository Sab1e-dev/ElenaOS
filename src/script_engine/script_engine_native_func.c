
/**
 * @file script_engine_native_func.c
 * @brief 原生函数实现及注册
 * @author Sab1e
 * @date 2025-07-26
 */

// Includes
#include "script_engine_native_func.h"
#include "jerryscript.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "script_engine_core.h"
#include "elena_os_port.h"
#include "script_engine_nav.h"
// Macros and Definitions

// Variables

// Function Implementations
/********************************** 原生函数定义 **********************************/
/**
 * @brief 处理 JavaScript 的 print 调用
 * 
 * 将所有参数转换为字符串并打印到标准输出。
 * 每个参数之间以空格分隔，末尾换行。
 * 适用于 JerryScript 引擎的原生函数绑定。
 * 
 * @param call_info_p 指向调用信息的指针，当前未使用，可忽略。类型为 const jerry_call_info_t*。
 * @param args_p 参数数组，包含所有传入的 JavaScript 参数。类型为 const jerry_value_t[]。
 * @param args_count 参数数量，表示 args_p 数组的长度。类型为 jerry_length_t。
 * @return 返回一个未定义值（jerry_undefined），表示没有返回结果。
 */
jerry_value_t js_print_handler(const jerry_call_info_t *call_info_p,
                               const jerry_value_t args_p[],
                               const jerry_length_t args_count)
{
    (void)call_info_p; // 当前未用到 this/func_obj，可忽略

    for (jerry_length_t i = 0; i < args_count; i++)
    {
        jerry_value_t str_val;

        if (jerry_value_is_string(args_p[i]))
        {
            str_val = jerry_value_copy(args_p[i]);
        }
        else
        {
            str_val = jerry_value_to_string(args_p[i]); // 转为字符串
        }

        jerry_size_t size = jerry_string_size(str_val, JERRY_ENCODING_UTF8);
        jerry_char_t *buf = (jerry_char_t *)malloc(size + 1); // Explicitly cast void* to jerry_char_t*
        if (!buf)
        {
            jerry_value_free(str_val);
            continue;
        }

        jerry_string_to_buffer(str_val, JERRY_ENCODING_UTF8, buf, size);
        buf[size] = '\0';

        printf("%s", buf);
        if (i < args_count - 1)
        {
            printf(" ");
        }

        free(buf);
        jerry_value_free(str_val);
    }

    printf("\n");
    return jerry_undefined();
}
/**
 * @brief Native 延时
 */
jerry_value_t js_delay_handler(const jerry_call_info_t *call_info_p,
                               const jerry_value_t args_p[],
                               const jerry_length_t args_count)
{
    eos_delay(args_p[0]);
}
/**
 * @brief 在脚本专用的导航栈上创建新的 screen
 * @param void 无参数
 * @return lv_obj_t* 返回 LVGL 对象指针
 */
static jerry_value_t js_nav_scr_create(const jerry_call_info_t* call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    // 调用底层函数
    lv_obj_t* ret_value = script_engine_nav_scr_create();

    // 处理返回值
    jerry_value_t js_result;
    // 包装为LVGL对象
    js_result = jerry_object();
    jerry_value_t ptr = jerry_number((double)(uintptr_t)ret_value);
    jerry_value_t cls = jerry_string_sz("lv_obj");
    jerry_object_set(js_result, jerry_string_sz("__ptr"), ptr);
    jerry_object_set(js_result, jerry_string_sz("__class"), cls);
    jerry_value_free(ptr);
    jerry_value_free(cls);

    return js_result;
}
/**
 * @brief 在导航栈上返回上一级
 * @param void 无参数
 * @return boolean 是否返回成功
 */
static jerry_value_t js_nav_back(const jerry_call_info_t* call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    // 调用底层函数
    script_engine_result_t ret = script_engine_nav_back_clean();

    jerry_value_t js_result;

    if(ret==SE_OK){
        js_result = jerry_boolean(true);
    }else{
        js_result = jerry_boolean(false);
    }
    
    return js_result;
}


/********************************** 注册原生函数 **********************************/

/**
 * @brief 原生函数列表
 */
const script_engine_func_entry_t script_engine_native_funcs[] = {
    {.name = "print",
     .handler = js_print_handler},
    {.name = "delay",
     .handler = js_delay_handler},
    {.name = "nav_scr_create",
     .handler = js_nav_scr_create},
    {.name = "nav_back",
     .handler = js_nav_back},
};

/**
 * @brief 将原生函数注册到 JerryScript 全局对象中
 */
void script_engine_register_natives()
{
    script_engine_register_functions(script_engine_native_funcs, sizeof(script_engine_native_funcs) / sizeof(script_engine_func_entry_t));
}
