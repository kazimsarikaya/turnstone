/**
 * @file screen.64.c
 * @brief Screen implementation for 64-bit mode.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <graphics/screen.h>
#include <graphics/font.h>

MODULE("turnstone.kernel.graphics.screen");

static color_t screen_fg_color = {.color = 0xFFFFFFFF};
static color_t screen_bg_color = {.color = 0x00000000};

static int32_t screen_width = 0;
static int32_t screen_height = 0;
static int32_t screen_pixels_per_scanline = 0;
static int32_t screen_chars_per_line = 0;
static int32_t screen_lines_on_screen = 0;

screen_flush_f SCREEN_FLUSH = NULL;
screen_print_glyph_with_stride_f SCREEN_PRINT_GLYPH_WITH_STRIDE = NULL;
screen_scroll_f SCREEN_SCROLL = NULL;
screen_clear_area_f SCREEN_CLEAR_AREA = NULL;

void screen_set_color(color_t fg, color_t bg) {
    screen_fg_color = fg;
    screen_bg_color = bg;
}

void screen_get_color(color_t* fg, color_t* bg) {
    if(fg) {
        *fg = screen_fg_color;
    }

    if(bg) {
        *bg = screen_bg_color;
    }
}

screen_info_t screen_get_info(void) {
    screen_info_t info = {
        .width = screen_width,
        .height = screen_height,
        .pixels_per_scanline = screen_pixels_per_scanline,
        .chars_per_line = screen_chars_per_line,
        .lines_on_screen = screen_lines_on_screen,
        .fg = screen_fg_color,
        .bg = screen_bg_color,
    };

    return info;
}

void screen_set_dimensions(int32_t width, int32_t height, int32_t pixels_per_scanline) {
    screen_width = width;
    screen_height = height;
    screen_pixels_per_scanline = pixels_per_scanline;

    font_table_t* ft = font_get_font_table();

    if(!ft) {
        screen_chars_per_line = 80;
        screen_lines_on_screen = 25;
        return;
    }

    screen_chars_per_line = screen_width / ft->font_width;
    screen_lines_on_screen = screen_height / ft->font_height;
}

void screen_clear(void) {
    screen_info_t screen_info = screen_get_info();
    SCREEN_CLEAR_AREA(0, 0, screen_info.width, screen_info.height, screen_info.bg);
}
