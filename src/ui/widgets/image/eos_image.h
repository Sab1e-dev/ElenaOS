/**
 * @file eos_image.h
 * @brief Image display
 */

#ifndef EOS_IMAGE_H
#define EOS_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "eos_image_resuorces.h"
/* Public macros ----------------------------------------------*/

/* Public typedefs --------------------------------------------*/

/* Public function prototypes ---------------------------------*/

/**
 * @brief Scale image to specified resolution
 * @param img_obj Target image object
 * @param w Target width (px)
 * @param h Target height (px)
 */
void eos_img_set_size(lv_obj_t *img_obj, const uint32_t w, const uint32_t h);
#ifdef __cplusplus
}
#endif

#endif /* EOS_IMAGE_H */
