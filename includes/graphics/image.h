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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ___PIXEL_T
#define ___PIXEL_T
typedef uint32_t pixel_t;
#endif

typedef struct graphics_raw_image_t {
    uint32_t width;
    uint32_t height;
    pixel_t* data;
} graphics_raw_image_t;

#ifdef __cplusplus
}
#endif

#endif // ___GRAPHICS_IMAGE_H
