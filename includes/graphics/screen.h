/**
 * @file screen.h
 * @brief screen header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___SCREEN_H
#define ___SCREEN_H 0

#include <types.h>
#include <graphics/color.h>

typedef struct screen_info_t {
    int32_t width;
    int32_t height;
    int32_t pixels_per_scanline;
    int32_t chars_per_line;
    int32_t lines_on_screen;
    color_t fg;
    color_t bg;
} screen_info_t;

void          screen_set_color(color_t fg, color_t bg);
void          screen_get_color(color_t* fg, color_t* bg);
screen_info_t screen_get_info(void);
void          screen_set_dimensions(int32_t width, int32_t height, int32_t pixels_per_scanline);
void          screen_clear(void);

typedef void (*screen_flush_f)(uint32_t scanout, uint64_t offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

extern screen_flush_f SCREEN_FLUSH;

typedef void (*screen_print_glyph_with_stride_f)(wchar_t wc,
                                                 color_t foreground, color_t background,
                                                 pixel_t* destination_base_address,
                                                 uint32_t x, uint32_t y,
                                                 uint32_t stride);

extern screen_print_glyph_with_stride_f SCREEN_PRINT_GLYPH_WITH_STRIDE;

typedef void (*screen_scroll_f)(void);

extern screen_scroll_f SCREEN_SCROLL;

typedef void (*screen_clear_area_f)(uint32_t x, uint32_t y, uint32_t width, uint32_t height, color_t background);

extern screen_clear_area_f SCREEN_CLEAR_AREA;

#endif // ___SCREEN_H
