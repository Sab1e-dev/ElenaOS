/**
 * @file elenix_os.h
 * @brief Just include this file externally
 */

#ifndef ELENIX_OS_H
#define ELENIX_OS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include "eos_config.h"
#include "eos_core.h"
#if EOS_USE_VIRTUAL_DISPLAY
#include "eos_virtual_display.h"
#endif /* EOS_USE_VIRTUAL_DISPLAY */
#include "eos_side_button.h"
#include "eos_crown.h"
#include "eos_log.h"
#include "eos_error.h"
#include "eos_device.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* ELENIX_OS_H */
