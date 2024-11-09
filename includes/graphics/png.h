/**
 * @file png.h
 * @brief PNG image header file.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___GRAPHICS_PNG_H
#define ___GRAPHICS_PNG_H 0

#include <graphics/image.h>


const char_t* graphics_png_error_string(int32_t error);

graphics_raw_image_t* graphics_load_png_image(const uint8_t* data, uint32_t size);


#endif // ___GRAPHICS_PNG_H
