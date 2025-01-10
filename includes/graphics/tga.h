/**
 * @file tga.h
 * @brief TGA image header file.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___GRAPHICS_TGA_H
#define ___GRAPHICS_TGA_H 0

#include <graphics/image.h>


typedef struct graphics_tga_image_t {
    uint8_t  magic;
    uint8_t  color_map;
    uint8_t  encoding;
    uint16_t color_map_origin;
    uint16_t color_map_length;
    uint8_t  color_map_entry_size;
    uint16_t x_origin;
    uint16_t y_origin;
    uint16_t width;
    uint16_t height;
    uint8_t  bpp;
    uint8_t  pixel_type;
    uint8_t  data[];
} __attribute__((packed)) graphics_tga_image_t;

graphics_raw_image_t* graphics_load_tga_image(graphics_tga_image_t* tga, uint32_t size);

#endif // ___GRAPHICS_TGA_H
