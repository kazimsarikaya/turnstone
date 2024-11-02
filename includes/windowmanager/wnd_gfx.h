/**
 * @file wnd_gfx.h
 * @brief window manager graphics header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___WND_GFX_H
#define ___WND_GFX_H

#include <windowmanager/wnd_types.h>

boolean_t windowmanager_draw_window(window_t* window);
void      windowmanager_print_glyph(const window_t* window, uint32_t x, uint32_t y, wchar_t wc);
void      windowmanager_print_text(const window_t* window, uint32_t x, uint32_t y, const char_t* text);
void      windowmanager_clear_screen(window_t* window);
color_t   wnd_gfx_blend_colors(color_t color1, color_t color2);
int8_t    wnd_gfx_draw_rectangle(pixel_t* buffer, uint32_t area_width, rect_t rect, color_t color);

#endif // ___WND_GFX_H
