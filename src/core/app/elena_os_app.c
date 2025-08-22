/**
 * @file elena_os_app.c
 * @brief 应用系统
 * @author Sab1e
 * @date 2025-08-21
 */

/**
 * Roadmap:
 * 应用程序的打包与解包 √
 *          ↓
 * 从应用程序文件夹获取应用程序列表
 * 应用程序列表使用动态数组，动态数组存储script_pkg_t
 * 应用程序列表加载相关资源时
 * 直接索引：/.sys/app/installed/<App ID>/icon.bin
 *          ↓
 * 显示应用列表及后续工作
 */
#include "elena_os_app.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "elena_os_core.h"
#include "elena_os_sys.h"
#include "script_engine_core.h"
#include "elena_os_pkg_mgr.h"
#include "elena_os_log.h"
// Macros and Definitions

/**
 * @brief 应用结构体
 */
// typedef struct eos_app_t{
//     script_pkg_t pkg;
// }eos_app_t;
typedef script_pkg_t eos_app_t;

typedef struct {
    char **data;
    size_t size;
    size_t capacity;
} eos_app_list_t;
static eos_app_list_t app_list;
// Variables

// Function Implementations

void eos_app_list_init(eos_app_list_t *list, size_t capacity){
    list->data = malloc(capacity * sizeof(char*));
    list->size = 0;
    list->capacity = capacity;
}

void eos_app_list_add(eos_app_list_t *list, const char *id) {
    if (list->size == list->capacity) {
        list->capacity *= 2;
        list->data = realloc(list->data, list->capacity * sizeof(char*));
    }
    list->data[list->size] = strdup(id); // 复制字符串
    list->size++;
}

void eos_app_list_free(eos_app_list_t *list) {
    for (size_t i = 0; i < list->size; i++) {
        free(list->data[i]);
    }
    free(list->data);
}

eos_result_t eos_app_list_get_installed(){
    
}

eos_result_t eos_app_install(const char *eapk_path){
    EOS_CHECK_PTR_RETURN_VAL(eapk_path,EOS_ERR_VAR_NULL);

    

    // 安装应用程序
    eos_pkg_mgr_unpack(eapk_path,EOS_APP_INSTALLED_DIR,SCRIPT_TYPE_APPLICATION);

}

eos_result_t eos_app_uninstall(){
    // 卸载应用程序
}

eos_result_t eos_app_update(){
    // 更新应用程序
}

eos_app_t *eos_app_get_list(){
    // 获取应用列表
}

eos_result_t eos_app_init(){
    // 初始化 从文件系统中读取应用列表
    
}

script_pkg_t eos_app_info_load(const char *app_id){
    // 加载应用包信息

}

void *eos_app_script_load(){

}

void *eos_app_assets_load(const char *file_name){
    // 加载图像等静态资源
}
