/**
 * @file image.h
 * @brief Image header file.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */



#ifndef ___GRAPHICS_IMAGE_H
#define ___GRAPHICS_IMAGE_H

#include <types.h>
#include <graphics/color.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct graphics_raw_image_t {
    uint32_t width;
    uint32_t height;
    color_t* data;
} graphics_raw_image_t;

#ifdef __cplusplus
}
#endif

#endif // ___GRAPHICS_IMAGE_H
