/**
 * @file elena_os_clock.h
 * @brief 时钟服务
 * @author Sab1e
 * @date 2025-08-28
 */

#ifndef ELENA_OS_CLOCK_H
#define ELENA_OS_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/
char *eos_colck_get_time_str(bool show_sec);
#ifdef __cplusplus
}
#endif

#endif /* ELENA_OS_CLOCK_H */
