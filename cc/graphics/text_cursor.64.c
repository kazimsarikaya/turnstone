/**
 * @file text_cursor.64.c
 * @brief Text cursor implementation for 64-bit mode.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <graphics/text_cursor.h>
#include <graphics/screen.h>
#include <graphics/font.h>

MODULE("turnstone.kernel.graphics.text_cursor");


static int32_t text_cursor_x = 0; ///< cursor postion for column
static int32_t text_cursor_y = 0; ///< cursor porsition for row
static boolean_t text_cursor_visible = false; ///< cursor visibility flag

text_cursor_draw_f TEXT_CURSOR_DRAW = 0; ///< function pointer to draw cursor

int8_t text_cursor_move(int32_t x, int32_t y) {
    screen_info_t info = screen_get_info();

    if(x < 0 || x >= info.chars_per_line || y < 0 || y >= info.lines_on_screen) {
        return -1;
    }

    text_cursor_x = x;
    text_cursor_y = y;

    return 0;
}

int8_t text_cursor_move_relative(int32_t x, int32_t y) {
    return text_cursor_move(text_cursor_x + x, text_cursor_y + y);
}

void text_cursor_get(int32_t* x, int32_t* y) {
    if(x) {
        *x = text_cursor_x;
    }

    if(y) {
        *y = text_cursor_y;
    }
}

void text_cursor_toggle(boolean_t flush) {
    if(TEXT_CURSOR_DRAW) {
        font_table_t* ft = font_get_font_table();

        TEXT_CURSOR_DRAW(text_cursor_x, text_cursor_y, ft->font_width, ft->font_height, flush);
        text_cursor_visible = !text_cursor_visible;
    }
}

void text_cursor_hide(void) {
    if (text_cursor_visible) {
        text_cursor_toggle(true);
    }
}

void text_cursor_show(void) {
    if (!text_cursor_visible) {
        text_cursor_toggle(true);
    }
}
