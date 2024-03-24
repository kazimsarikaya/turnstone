/**
 * @file wndmgr_utils.64.c
 * @brief Window manager utilities implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <utils.h>
#include <strings.h>

void video_text_print(const char_t* text);

MODULE("turnstone.windowmanager");

void windowmanager_print_glyph(const window_t* window, int32_t x, int32_t y, wchar_t wc) {
    uint8_t* glyph = FONT_ADDRESS + (wc * FONT_BYTES_PER_GLYPH);

    int32_t abs_x = window->rect.x + x;
    int32_t abs_y = window->rect.y + y;

    int32_t offs = (abs_y * VIDEO_PIXELS_PER_SCANLINE) + abs_x;

    int32_t tx, ty, line, mask;

    for(ty = 0; ty < FONT_HEIGHT; ty++) {
        line = offs;
        mask = FONT_MASK;

        uint32_t tmp = BYTE_SWAP32(*((uint32_t*)glyph));

        for(tx = 0; tx < FONT_WIDTH; tx++) {

            *((pixel_t*)(VIDEO_BASE_ADDRESS + line)) = tmp & mask ? window->foreground_color.color : window->background_color.color;

            mask >>= 1;
            line++;
        }

        glyph += FONT_BYTES_PERLINE;
        offs  += VIDEO_PIXELS_PER_SCANLINE;
    }
}

void windowmanager_print_text(const window_t* window, int32_t x, int32_t y, const char_t* text) {
    if(window == NULL) {
        return;
    }

    if(text == NULL) {
        return;
    }

    int64_t i = 0;

    while(text[i]) {
        wchar_t wc = video_get_wc(text + i, &i);

        if(wc == '\n') {
            y += FONT_HEIGHT;
            x = 0;
        } else {
            windowmanager_print_glyph(window, x, y, wc);
            x += FONT_WIDTH;
        }

        text++;
    }
}

void windowmanager_clear_screen(window_t* window) {
    video_move_text_cursor(0, 0);
    video_text_cursor_hide();
    for(int32_t i = 0; i < VIDEO_GRAPHICS_HEIGHT; i++) {
        for(int32_t j = 0; j < VIDEO_GRAPHICS_WIDTH; j++) {
            *((pixel_t*)(VIDEO_BASE_ADDRESS + (i * VIDEO_PIXELS_PER_SCANLINE) + j)) = window->background_color.color;
        }
    }
}

rect_t windowmanager_calc_text_rect(const char_t* text, int32_t max_width) {
    if(text == NULL) {
        return (rect_t){0};
    }

    rect_t rect = {0};

    rect.x = 0;
    rect.y = 0;
    rect.width = 0;
    rect.height = 0;

    int32_t max_calc_width = 0;
    size_t len = strlen(text);

    while(*text) {
        if(*text == '\n') {
            rect.height += FONT_HEIGHT;
            max_calc_width = MAX(max_calc_width, rect.width);
            rect.width = 0;
        } else {
            if(rect.width + FONT_WIDTH > max_width) {
                rect.height += FONT_HEIGHT;
                max_calc_width = MAX(max_calc_width, rect.width);
                rect.width = 0;
            } else {
                rect.width += FONT_WIDTH;
            }
        }

        text++;
    }

    rect.width = MAX(rect.width, max_calc_width);

    if(len > 0 && rect.height == 0) {
        rect.height = FONT_HEIGHT;
    }

    return rect;
}

uint32_t windowmanager_append_wchar_to_buffer(wchar_t src, char_t* dst, uint32_t dst_idx) {
    if(dst == NULL) {
        return NULL;
    }

    int64_t j = dst_idx;

    if(src >= 0x800) {
        dst[j++] = ((src >> 12) & 0xF) | 0xE0;
        dst[j++] = ((src >> 6) & 0x3F) | 0x80;
        dst[j++] = (src & 0x3F) | 0x80;
    } else if(src >= 0x80) {
        dst[j++] = ((src >> 6) & 0x1F) | 0xC0;
        dst[j++] = (src & 0x3F) | 0x80;
    } else {
        dst[j++] = src & 0x7F;
    }

    return j;
}
