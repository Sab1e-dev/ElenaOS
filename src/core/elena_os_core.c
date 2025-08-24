/**
 * @file elena_os_core.c
 * @brief Elena OS 核心代码实现
 * @author Sab1e
 * @date 2025-08-10
 */

#include "elena_os_core.h"

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "lvgl.h"
#include "cJSON.h"
#include "elena_os_img.h"
#include "elena_os_msg_list.h"
#include "elena_os_lang.h"
#include "elena_os_log.h"
#include "elena_os_nav.h"
#include "elena_os_base_item.h"
#include "elena_os_event.h"
#include "elena_os_test.h"
#include "elena_os_version.h"
#include "elena_os_port.h"
#include "elena_os_swipe_panel.h"
#include "elena_os_sys.h"
#include "elena_os_app.h"
#include "script_engine_core.h"
// Macros and Definitions

// Variables
script_pkg_t *script_pkg_ptr;      // 脚本包指针
// Function Implementations

eos_result_t eos_run()
{
    eos_app_init();
    eos_sys_init();
    eos_test_start();
}
