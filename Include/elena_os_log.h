/**
 * @file elena_os_log.h
 * @brief Debug
 * @author Sab1e
 * @date 2025-08-14
 */

#ifndef ELENA_OS_LOG_H
#define ELENA_OS_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "msh.h"
/* Public macros ----------------------------------------------*/

#define EOS_LOG_D(fmt, ...) \
    printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define EOS_LOG_I(fmt, ...) \
    printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define EOS_LOG_E(fmt, ...) \
    printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define EOS_LOG_W(fmt, ...) \
    printf("[WARN] " fmt "\n", ##__VA_ARGS__)

#define EOS_MEM(tag) printf("[%s] ",tag);\
    msh_exec("list_mem", 8)

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_LOG_H */
