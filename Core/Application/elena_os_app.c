/**
 * @file elena_os_app.c
 * @brief 应用系统
 * @author Sab1e
 * @date 2025-08-21
 */

/**
 * Roadmap:
 * 应用程序的打包与解包
 *          ↓
 * 从应用程序文件夹获取应用程序列表
 * 应用程序列表使用动态数组，动态数组存储ScriptPackage_t
 * 应用程序列表加载相关资源时
 * 直接索引：/.sys/app/installed/<App ID>/icon.bin
 *          ↓
 * 显示应用列表及后续工作
 */
#include "elena_os_app.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "elena_os_core.h"
#include "elena_os_sys.h"
#include "script_engine_core.h"
// Macros and Definitions
#define EOS_APP_DIR EOS_SYS_DIR "app/"
#define EOS_APP_INSTALLED_DIR EOS_APP_DIR "installed/"
#define EOS_APP_DATA_DIR EOS_APP_DIR "app_data/"
#define EOS_APP_PACKAGE_LIST_FILE_PATH EOS_APP_DIR "app_list.json"

/**
 * @brief 应用结构体
 */
// typedef struct ElenaOSApp_t{
//     ScriptPackage_t pkg;
// }ElenaOSApp_t;
typedef ScriptPackage_t ElenaOSApp_t;
// Variables

// Function Implementations



ElenaOSResult_t eos_app_install(){
    // 安装应用程序
}

ElenaOSResult_t eos_app_uninstall(){
    // 卸载应用程序
}

ElenaOSResult_t eos_app_update(){
    // 更新应用程序
}

ElenaOSApp_t *eos_app_get_list(){
    // 获取应用列表
}

ElenaOSResult_t eos_app_init(){
    // 初始化 从文件系统中读取应用列表
    
}

ScriptPackage_t eos_app_info_load(const char *app_id){
    // 加载应用包信息

}

void *eos_app_script_load(){

}

void *eos_app_assets_load(const char *file_name){
    // 加载图像等静态资源
}
