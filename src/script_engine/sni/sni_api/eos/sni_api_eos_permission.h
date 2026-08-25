/**
 * @file sni_api_eos_permission.h
 * @brief SNI permission API - eos.permission.request() and eos.permission.check()
 */
#ifndef SNI_API_EOS_PERMISSION_H
#define SNI_API_EOS_PERMISSION_H

/* Includes ---------------------------------------------------*/
#include "jerryscript.h"
#include "sni_api_export.h"

/* Public function prototypes ---------------------------------*/

jerry_value_t sni_api_eos_permission_request(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args_p[],
                                             const jerry_length_t args_count);

jerry_value_t sni_api_eos_permission_check(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args_p[],
                                           const jerry_length_t args_count);

#endif /* SNI_API_EOS_PERMISSION_H */
