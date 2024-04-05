/**
 * @file video.xx.c
 * @brief video operations
 *
 * prints string to video buffer and moves cursor
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/video.h>
#include <driver/video_fb.h>
#include <memory.h>
#include <ports.h>
#include <strings.h>
#include <utils.h>
#include <systeminfo.h>
#include <cpu.h>
#include <cpu/sync.h>
#include <list.h>
#include <pci.h>
#include <driver/video_virtio.h>
#include <driver/video_vmwaresvga.h>
#include <logging.h>
#include <apic.h>
#include <graphics/screen.h>
#include <graphics/text_cursor.h>
#include <graphics/font.h>

MODULE("turnstone.kernel.hw.video.fb");

pixel_t* VIDEO_BASE_ADDRESS = NULL;

extern boolean_t GRAPHICS_MODE;
extern lock_t* video_lock;

static void video_fb_display_flush_dummy(uint32_t scanout, uint64_t offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    UNUSED(scanout);
    UNUSED(offset);
    UNUSED(x);
    UNUSED(y);
    UNUSED(width);
    UNUSED(height);
}

void video_fb_refresh_frame_buffer_address(void) {
    VIDEO_BASE_ADDRESS = (uint32_t*)SYSTEM_INFO->frame_buffer->virtual_base_address;

    screen_set_dimensions(SYSTEM_INFO->frame_buffer->width, SYSTEM_INFO->frame_buffer->height, SYSTEM_INFO->frame_buffer->pixels_per_scanline);

    if(VIDEO_BASE_ADDRESS) {
        GRAPHICS_MODE = true;
    }
}

static void video_fb_text_cursor_draw(int32_t x, int32_t y, int32_t width, int32_t height, boolean_t flush) {
    screen_info_t screen_info = screen_get_info();

    int32_t offs = (y * height * screen_info.pixels_per_scanline) + (x * width);
    uint32_t orig_offs = offs;

    int32_t lx, ly, line;

    for(ly = 0; ly < height; ly++) {
        line = offs;

        for(lx = 0; lx < width; lx++) {

            *((pixel_t*)(VIDEO_BASE_ADDRESS + line)) = ~(*((pixel_t*)(VIDEO_BASE_ADDRESS + line)));

            line++;
        }

        offs  += screen_info.pixels_per_scanline;
    }

    if(flush) {
        SCREEN_FLUSH(0, orig_offs * sizeof(pixel_t), x * width, y * height, width, height);
    }
}

static void video_fb_graphics_scroll(void){
    font_table_t* ft = font_get_font_table();
    screen_info_t screen_info = screen_get_info();

    int64_t j = 0;
    int64_t i = (ft->font_height * screen_info.pixels_per_scanline) / 2;
    int64_t x = 0;
    uint64_t* dst = (uint64_t*)VIDEO_BASE_ADDRESS;
    int64_t empty_area_size = (screen_info.pixels_per_scanline - screen_info.width) / 2;
    int64_t half_width = screen_info.width / 2;

    while(i < screen_info.pixels_per_scanline * screen_info.height / 2) {
        dst[j] = dst[i];

        i++;
        j++;
        x++;

        if(x == half_width) {
            x = 0;
            i += empty_area_size;
            j += empty_area_size;
        }
    }

    i = (ft->font_height * screen_info.pixels_per_scanline * (screen_info.lines_on_screen - 1)) / 2;
    x = 0;

    uint64_t bg = screen_info.bg.color;
    bg = (bg << 32) | bg;

    while(i < screen_info.pixels_per_scanline * screen_info.height / 2) {
        dst[i] = bg;

        i++;
        x++;

        if(x == screen_info.width) {
            x = 0;
            i += empty_area_size;
        }
    }
}

static void video_fb_graphics_print(const char_t* string) {
    int64_t i = 0;
    uint64_t flush_offset = 0;
    uint32_t min_x = 0;
    uint32_t max_x = 0;
    uint32_t min_y = 0;
    uint32_t max_y = 0;
    boolean_t full_flush = false;

    int32_t cursor_graphics_x = 0;
    int32_t cursor_graphics_y = 0;

    text_cursor_get(&cursor_graphics_x, &cursor_graphics_y);

    int32_t old_cursor_graphics_x = cursor_graphics_x;
    int32_t old_cursor_graphics_y = cursor_graphics_y;
    int32_t max_cursor_graphics_x = cursor_graphics_x;

    screen_info_t screen_info = screen_get_info();

    font_table_t* ft = font_get_font_table();

    if(string == NULL || strlen(string) == 0) {
        return;
    }

    while(string[i]) {
        wchar_t wc = font_get_wc(string + i, &i);

        if(wc == '\r') {
            cursor_graphics_x = 0;
            i++;

            full_flush = true; // FIXME: calculate the area to flush

            continue;
        }

        if(wc == '\b') {
            if(cursor_graphics_x > 0) {
                cursor_graphics_x--;
            } else {
                if(cursor_graphics_y > 0) {
                    cursor_graphics_y--;
                    cursor_graphics_x = screen_info.chars_per_line - 1;
                }
            }

            i++;

            full_flush = true; // FIXME: calculate the area to flush

            continue;
        }

        if(wc == '\n' || wc == '\t') {

            if(wc == '\n') {
                cursor_graphics_x = 0;
                cursor_graphics_y++;
            } else {
                cursor_graphics_x = ((cursor_graphics_x + FONT_TAB_STOP) / FONT_TAB_STOP) * FONT_TAB_STOP;

                if(cursor_graphics_x >= screen_info.chars_per_line) {
                    cursor_graphics_x = cursor_graphics_x % screen_info.chars_per_line;
                    cursor_graphics_y++;
                }
            }

            i++;

            if(cursor_graphics_y >= screen_info.lines_on_screen) {
                SCREEN_SCROLL();
                cursor_graphics_y = screen_info.lines_on_screen - 1;
                full_flush = true;
            }

            continue;
        }

        if(cursor_graphics_y >= screen_info.lines_on_screen) {
            SCREEN_SCROLL();
            cursor_graphics_y = screen_info.lines_on_screen - 1;
            full_flush = true;
        }

        SCREEN_PRINT_GLYPH_WITH_STRIDE(wc,
                                       screen_info.fg, screen_info.bg,
                                       VIDEO_BASE_ADDRESS,
                                       cursor_graphics_x, cursor_graphics_y,
                                       screen_info.pixels_per_scanline);

        cursor_graphics_x++;

        if(cursor_graphics_x >= screen_info.chars_per_line) {
            cursor_graphics_x = 0;
            cursor_graphics_y++;

            if(cursor_graphics_y >= screen_info.lines_on_screen) {
                SCREEN_SCROLL();
                cursor_graphics_y = screen_info.lines_on_screen - 1;
                full_flush = true;
            }
        }

        if(cursor_graphics_x > max_cursor_graphics_x) {
            max_cursor_graphics_x = cursor_graphics_x;
        }

        i++;
    }

    if(full_flush) {
        SCREEN_FLUSH(0, 0, 0, 0, screen_info.width, screen_info.height);
    } else {
        flush_offset = (old_cursor_graphics_y * ft->font_height * screen_info.pixels_per_scanline) + (old_cursor_graphics_x * ft->font_width);

        if(old_cursor_graphics_y == cursor_graphics_y) {
            min_x = old_cursor_graphics_x * ft->font_width;
            max_x = cursor_graphics_x * ft->font_width;
            min_y = old_cursor_graphics_y * ft->font_height;
            max_y = min_y + ft->font_height;
        } else {
            min_x = old_cursor_graphics_x * ft->font_width;
            max_x = max_cursor_graphics_x * ft->font_width;
            min_y = old_cursor_graphics_y * ft->font_height;
            max_y = min_y + ft->font_height;

            SCREEN_FLUSH(0, flush_offset * sizeof(pixel_t), min_x, min_y, max_x - min_x, max_y - min_y);

            min_x = 0;
            max_x = max_cursor_graphics_x * ft->font_width;
            min_y = (old_cursor_graphics_y + 1) * ft->font_height;
            max_y = (cursor_graphics_y * ft->font_height) + ft->font_height;

            flush_offset = min_y * screen_info.pixels_per_scanline + min_x;
        }

        SCREEN_FLUSH(0, flush_offset * sizeof(pixel_t), min_x, min_y, max_x - min_x, max_y - min_y);
    }

    text_cursor_move(cursor_graphics_x, cursor_graphics_y);
}

static void video_fb_clear_screen_area(uint32_t x, uint32_t y, uint32_t width, uint32_t height, color_t background) {
    if(GRAPHICS_MODE) {
        uint32_t i = 0;
        uint32_t j = 0;
        uint32_t line = 0;
        uint32_t bg = background.color;
        screen_info_t screen_info = screen_get_info();

        for(i = 0; i < height; i++) {
            line = (y + i) * screen_info.pixels_per_scanline + x;

            for(j = 0; j < width; j++) {
                *((pixel_t*)(VIDEO_BASE_ADDRESS + line)) = bg;
                line++;
            }
        }

        SCREEN_FLUSH(0, 0, x, y, width, height);
    }
}



void video_fb_init(void) {
    video_text_print("video init\n");
    GRAPHICS_MODE = false;

    VIDEO_BASE_ADDRESS = (pixel_t*)SYSTEM_INFO->frame_buffer->virtual_base_address;

    char_t buffer[100] = {0};
    utoh_with_buffer(buffer, (uint64_t)VIDEO_BASE_ADDRESS);
    video_text_print("frame buffer address: 0x");
    video_text_print(buffer);
    video_text_print("\n");

    if(font_init() != 0) {
        video_text_print("font init failed\n");
        return;
    }

    font_table_t* font_table = font_get_font_table();

    if(font_table == NULL) {
        video_text_print("font table is NULL\n");
        return;
    }

    screen_set_dimensions(SYSTEM_INFO->frame_buffer->width, SYSTEM_INFO->frame_buffer->height, SYSTEM_INFO->frame_buffer->pixels_per_scanline);


    if(VIDEO_BASE_ADDRESS) {
        GRAPHICS_MODE = true;
        SCREEN_FLUSH = video_fb_display_flush_dummy;
        SCREEN_PRINT_GLYPH_WITH_STRIDE = font_print_glyph_with_stride;
        SCREEN_SCROLL = video_fb_graphics_scroll;
        SCREEN_CLEAR_AREA = video_fb_clear_screen_area;
        TEXT_CURSOR_DRAW = video_fb_text_cursor_draw;
        VIDEO_GRAPHICS_PRINT = video_fb_graphics_print;
    }

    video_lock = lock_create();

    if(!GRAPHICS_MODE) {
        video_text_print("graphics mode not active");
    }

    video_text_print("video init done\n");
}

int8_t video_fb_copy_contents_to_frame_buffer(uint8_t* buffer, uint64_t new_width, uint64_t new_height, uint64_t new_pixels_per_scanline) {
    if(!VIDEO_BASE_ADDRESS) {
        return -1;
    }

    screen_info_t screen_info = screen_get_info();

    UNUSED(new_width);
    UNUSED(new_height);

    int64_t j = 0;
    pixel_t* buf = (pixel_t*)buffer;

    int64_t i = 0;
    int64_t x = 0;

    while(i < screen_info.pixels_per_scanline * screen_info.height) {
        buf[j] = *((pixel_t*)(VIDEO_BASE_ADDRESS + i));

        i++;
        j++;
        x++;

        if(x == screen_info.width) {
            x = 0;
            i += screen_info.pixels_per_scanline - screen_info.width;
            j += (new_pixels_per_scanline - screen_info.pixels_per_scanline);
        }
    }

    return 0;
}
