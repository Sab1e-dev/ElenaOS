/**
 * @file elena_os_app.h
 * @brief 应用系统
 * @author Sab1e
 * @date 2025-08-21
 */

#ifndef ELENA_OS_APP_H
#define ELENA_OS_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "elena_os_core.h"
#include "elena_os_sys.h"

/* Public macros ----------------------------------------------*/
#define EOS_APP_DIR EOS_SYS_DIR "app/"
#define EOS_APP_INSTALLED_DIR EOS_APP_DIR "apps/"
#define EOS_APP_DATA_DIR EOS_APP_DIR "app_data/"
#define EOS_APP_ICON_FILE_NAME  "icon.bin"
#define EOS_APP_MANIFEST_FILE_NAME "manifest.json"
/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

size_t eos_app_list_size(void);

const char* eos_app_list_get_id(size_t index);

bool eos_app_list_contains(const char* app_id);

eos_result_t eos_app_install(const char *eapk_path);
eos_result_t eos_app_uninstall(const char *app_id);
eos_result_t eos_app_init();
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_APP_H */
