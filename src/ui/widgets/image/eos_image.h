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
/**
 * @brief Create a circular-clipped image widget.
 *
 * Wraps the image in a container with `LV_RADIUS_CIRCLE` and `clip_corner`
 * so the image is cropped to a circle. The image is center-aligned and
 * scaled to fill the circular area (cover mode).
 *
 * @param parent Parent object
 * @param src Image source (path string, symbol, or lv_image_dsc_t pointer)
 * @param size Diameter of the circle in pixels
 * @return lv_obj_t* The circular container (not the inner image)
 */
lv_obj_t *eos_circle_image_create(lv_obj_t *parent, const void *src, lv_coord_t size);
#ifdef __cplusplus
}
#endif

#endif /* EOS_IMAGE_H */
