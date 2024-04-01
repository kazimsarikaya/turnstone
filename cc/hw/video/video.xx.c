/**
 * @file video.xx.c
 * @brief video operations
 *
 * prints string to video buffer and moves cursor
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <video.h>
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

MODULE("turnstone.kernel.hw.video");

#define VIDEO_TAB_STOP 8

uint16_t cursor_graphics_x = 0; ///< cursor postion for column
uint16_t cursor_graphics_y = 0; ///< cursor porsition for row

/**
 * @brief scrolls video up for one line
 */
void video_graphics_scroll(void);
void video_text_print(const char_t* string);
void video_graphics_print(const char_t* string);
void video_display_flush_dummy(uint32_t scanout, uint64_t offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void video_clear_screen_area(uint32_t x, uint32_t y, uint32_t width, uint32_t height, color_t background);


uint32_t* VIDEO_BASE_ADDRESS = NULL;
uint32_t VIDEO_PIXELS_PER_SCANLINE = 0;
video_display_flush_f VIDEO_DISPLAY_FLUSH = NULL;
video_move_cursor_f VIDEO_MOVE_CURSOR = NULL;
video_print_glyph_with_stride_f VIDEO_PRINT_GLYPH_WITH_STRIDE = NULL;
video_scroll_screen_f VIDEO_SCROLL_SCREEN = NULL;
video_clear_screen_area_f VIDEO_CLEAR_SCREEN_AREA = NULL;

boolean_t GRAPHICS_MODE = 0;
int32_t VIDEO_GRAPHICS_WIDTH = 0;
int32_t VIDEO_GRAPHICS_HEIGHT = 0;
color_t VIDEO_GRAPHICS_FOREGROUND = {.color = 0x00FFFFFF};
color_t VIDEO_GRAPHICS_BACKGROUND = {.color = 0x00000000};

static int32_t VIDEO_FONT_CHARS_PER_LINE = 0;
static int32_t VIDEO_FONT_LINES_ON_SCREEN = 0;

static uint32_t VIDEO_FONT_WIDTH = 0;
static uint32_t VIDEO_FONT_HEIGHT = 0;

lock_t* video_lock = NULL;


void video_set_color(color_t foreground, color_t background) {
    VIDEO_GRAPHICS_FOREGROUND = foreground;
    VIDEO_GRAPHICS_BACKGROUND = background;
}

void video_display_flush_dummy(uint32_t scanout, uint64_t offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    UNUSED(scanout);
    UNUSED(offset);
    UNUSED(x);
    UNUSED(y);
    UNUSED(width);
    UNUSED(height);
}

void video_refresh_frame_buffer_address(void) {
    VIDEO_BASE_ADDRESS = (uint32_t*)SYSTEM_INFO->frame_buffer->virtual_base_address;
    VIDEO_PIXELS_PER_SCANLINE = SYSTEM_INFO->frame_buffer->pixels_per_scanline;
    VIDEO_GRAPHICS_WIDTH = SYSTEM_INFO->frame_buffer->width;
    VIDEO_GRAPHICS_HEIGHT = SYSTEM_INFO->frame_buffer->height;

    VIDEO_FONT_CHARS_PER_LINE = VIDEO_GRAPHICS_WIDTH / VIDEO_FONT_WIDTH;
    VIDEO_FONT_LINES_ON_SCREEN = VIDEO_GRAPHICS_HEIGHT / VIDEO_FONT_HEIGHT;

    if(VIDEO_BASE_ADDRESS) {
        GRAPHICS_MODE = true;
    }
}

void video_init(void) {
    video_text_print("video init\n");
    GRAPHICS_MODE = false;

    VIDEO_BASE_ADDRESS = (uint32_t*)SYSTEM_INFO->frame_buffer->virtual_base_address;
    VIDEO_PIXELS_PER_SCANLINE = SYSTEM_INFO->frame_buffer->pixels_per_scanline;
    VIDEO_GRAPHICS_WIDTH = SYSTEM_INFO->frame_buffer->width;
    VIDEO_GRAPHICS_HEIGHT = SYSTEM_INFO->frame_buffer->height;

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

    utoh_with_buffer(buffer, (uint64_t)font_table->bitmap);
    video_text_print("font bitmap address: 0x");
    video_text_print(buffer);
    video_text_print("\n");

    VIDEO_FONT_WIDTH = font_table->font_width;

    utoa_with_buffer(buffer, VIDEO_FONT_WIDTH);
    video_text_print("font width: ");
    video_text_print(buffer);
    video_text_print("\n");

    VIDEO_FONT_HEIGHT = font_table->font_height;

    utoa_with_buffer(buffer, VIDEO_FONT_HEIGHT);
    video_text_print("font height: ");
    video_text_print(buffer);
    video_text_print("\n");

    utoh_with_buffer(buffer, font_table->glyph_count);
    video_text_print("glyph count: 0x");
    video_text_print(buffer);
    video_text_print("\n");

    VIDEO_FONT_CHARS_PER_LINE = VIDEO_GRAPHICS_WIDTH / VIDEO_FONT_WIDTH;
    VIDEO_FONT_LINES_ON_SCREEN = VIDEO_GRAPHICS_HEIGHT / VIDEO_FONT_HEIGHT;

    VIDEO_DISPLAY_FLUSH = video_display_flush_dummy;

    if(VIDEO_BASE_ADDRESS) {
        GRAPHICS_MODE = true;
        VIDEO_PRINT_GLYPH_WITH_STRIDE = font_print_glyph_with_stride;
        VIDEO_SCROLL_SCREEN = video_graphics_scroll;
        VIDEO_CLEAR_SCREEN_AREA = video_clear_screen_area;
    }

    video_lock = lock_create();

    if(!GRAPHICS_MODE) {
        video_text_print("graphics mode not active");
    }



    video_text_print("video init done\n");
}


void video_graphics_scroll(void){
    int64_t j = 0;
    int64_t i = (VIDEO_FONT_HEIGHT * VIDEO_PIXELS_PER_SCANLINE) / 2;
    int64_t x = 0;
    uint64_t* dst = (uint64_t*)VIDEO_BASE_ADDRESS;
    int64_t empty_area_size = (VIDEO_PIXELS_PER_SCANLINE - VIDEO_GRAPHICS_WIDTH) / 2;
    int64_t half_width = VIDEO_GRAPHICS_WIDTH / 2;

    while(i < VIDEO_PIXELS_PER_SCANLINE * VIDEO_GRAPHICS_HEIGHT / 2) {
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

    i = (VIDEO_FONT_HEIGHT * VIDEO_PIXELS_PER_SCANLINE * (VIDEO_FONT_LINES_ON_SCREEN - 1)) / 2;
    x = 0;

    uint64_t bg = VIDEO_GRAPHICS_BACKGROUND.color;
    bg = (bg << 32) | bg;

    while(i < VIDEO_PIXELS_PER_SCANLINE * VIDEO_GRAPHICS_HEIGHT / 2) {
        dst[i] = bg;

        i++;
        x++;

        if(x == VIDEO_GRAPHICS_WIDTH) {
            x = 0;
            i += empty_area_size;
        }
    }
}

boolean_t video_text_cursor_visible = true;

void video_text_cursor_toggle(boolean_t flush) {
    int32_t offs = (cursor_graphics_y * VIDEO_FONT_HEIGHT * VIDEO_PIXELS_PER_SCANLINE) + (cursor_graphics_x * VIDEO_FONT_WIDTH);
    uint32_t orig_offs = offs;

    uint32_t x, y, line;

    for(y = 0; y < VIDEO_FONT_HEIGHT; y++) {
        line = offs;

        for(x = 0; x < VIDEO_FONT_WIDTH; x++) {

            *((pixel_t*)(VIDEO_BASE_ADDRESS + line)) = ~(*((pixel_t*)(VIDEO_BASE_ADDRESS + line)));

            line++;
        }

        offs  += VIDEO_PIXELS_PER_SCANLINE;
    }

    video_text_cursor_visible = !video_text_cursor_visible;

    if(flush) {
        VIDEO_DISPLAY_FLUSH(0, orig_offs * sizeof(pixel_t), cursor_graphics_x * VIDEO_FONT_WIDTH, cursor_graphics_y * VIDEO_FONT_HEIGHT, VIDEO_FONT_WIDTH, VIDEO_FONT_HEIGHT);
    }
}

void video_text_cursor_hide(void) {
    if(video_text_cursor_visible) {
        video_text_cursor_toggle(true);
    }
}

void video_text_cursor_show(void) {
    if(!video_text_cursor_visible) {
        video_text_cursor_toggle(true);
    }
}

void video_graphics_print(const char_t* string) {
    int64_t i = 0;
    uint64_t flush_offset = 0;
    uint32_t min_x = 0;
    uint32_t max_x = 0;
    uint32_t min_y = 0;
    uint32_t max_y = 0;
    boolean_t full_flush = false;
    uint16_t old_cursor_graphics_x = cursor_graphics_x;
    uint16_t old_cursor_graphics_y = cursor_graphics_y;
    uint16_t max_cursor_graphics_x = cursor_graphics_x;

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
                    cursor_graphics_x = VIDEO_FONT_CHARS_PER_LINE - 1;
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
                cursor_graphics_x = ((cursor_graphics_x + VIDEO_TAB_STOP) / VIDEO_TAB_STOP) * VIDEO_TAB_STOP;

                if(cursor_graphics_x >= VIDEO_FONT_CHARS_PER_LINE) {
                    cursor_graphics_x = cursor_graphics_x % VIDEO_FONT_CHARS_PER_LINE;
                    cursor_graphics_y++;
                }
            }

            i++;

            if(cursor_graphics_y >= VIDEO_FONT_LINES_ON_SCREEN) {
                VIDEO_SCROLL_SCREEN();
                cursor_graphics_y = VIDEO_FONT_LINES_ON_SCREEN - 1;
                full_flush = true;
            }

            continue;
        }

        if(cursor_graphics_y >= VIDEO_FONT_LINES_ON_SCREEN) {
            VIDEO_SCROLL_SCREEN();
            cursor_graphics_y = VIDEO_FONT_LINES_ON_SCREEN - 1;
            full_flush = true;
        }

        VIDEO_PRINT_GLYPH_WITH_STRIDE(wc,
                                      VIDEO_GRAPHICS_FOREGROUND, VIDEO_GRAPHICS_BACKGROUND,
                                      VIDEO_BASE_ADDRESS,
                                      cursor_graphics_x, cursor_graphics_y,
                                      VIDEO_PIXELS_PER_SCANLINE);

        cursor_graphics_x++;

        if(cursor_graphics_x >= VIDEO_FONT_CHARS_PER_LINE) {
            cursor_graphics_x = 0;
            cursor_graphics_y++;

            if(cursor_graphics_y >= VIDEO_FONT_LINES_ON_SCREEN) {
                VIDEO_SCROLL_SCREEN();
                cursor_graphics_y = VIDEO_FONT_LINES_ON_SCREEN - 1;
                full_flush = true;
            }
        }

        if(cursor_graphics_x > max_cursor_graphics_x) {
            max_cursor_graphics_x = cursor_graphics_x;
        }

        i++;
    }

    if(full_flush) {
        VIDEO_DISPLAY_FLUSH(0, 0, 0, 0, VIDEO_GRAPHICS_WIDTH, VIDEO_GRAPHICS_HEIGHT);
    } else {
        flush_offset = (old_cursor_graphics_y * VIDEO_FONT_HEIGHT * VIDEO_PIXELS_PER_SCANLINE) + (old_cursor_graphics_x * VIDEO_FONT_WIDTH);

        if(old_cursor_graphics_y == cursor_graphics_y) {
            min_x = old_cursor_graphics_x * VIDEO_FONT_WIDTH;
            max_x = cursor_graphics_x * VIDEO_FONT_WIDTH;
            min_y = old_cursor_graphics_y * VIDEO_FONT_HEIGHT;
            max_y = min_y + VIDEO_FONT_HEIGHT;
        } else {
            min_x = old_cursor_graphics_x * VIDEO_FONT_WIDTH;
            max_x = max_cursor_graphics_x * VIDEO_FONT_WIDTH;
            min_y = old_cursor_graphics_y * VIDEO_FONT_HEIGHT;
            max_y = min_y + VIDEO_FONT_HEIGHT;

            VIDEO_DISPLAY_FLUSH(0, flush_offset * sizeof(pixel_t), min_x, min_y, max_x - min_x, max_y - min_y);

            min_x = 0;
            max_x = max_cursor_graphics_x * VIDEO_FONT_WIDTH;
            min_y = (old_cursor_graphics_y + 1) * VIDEO_FONT_HEIGHT;
            max_y = (cursor_graphics_y * VIDEO_FONT_HEIGHT) + VIDEO_FONT_HEIGHT;

            flush_offset = min_y * VIDEO_PIXELS_PER_SCANLINE;
        }

        VIDEO_DISPLAY_FLUSH(0, flush_offset * sizeof(pixel_t), min_x, min_y, max_x - min_x, max_y - min_y);
    }
}

void video_clear_screen_area(uint32_t x, uint32_t y, uint32_t width, uint32_t height, color_t background) {
    if(GRAPHICS_MODE) {
        uint32_t i = 0;
        uint32_t j = 0;
        uint32_t line = 0;
        uint32_t bg = background.color;

        for(i = 0; i < height; i++) {
            line = (y + i) * VIDEO_PIXELS_PER_SCANLINE + x;

            for(j = 0; j < width; j++) {
                *((pixel_t*)(VIDEO_BASE_ADDRESS + line)) = bg;
                line++;
            }
        }

        VIDEO_DISPLAY_FLUSH(0, 0, x, y, width, height);
    }
}

void video_clear_screen(void){
    video_clear_screen_area(0, 0, VIDEO_GRAPHICS_WIDTH, VIDEO_GRAPHICS_HEIGHT, VIDEO_GRAPHICS_BACKGROUND);
}

void video_print(const char_t* string) {
    lock_acquire(video_lock);

    video_text_print(string);

    if(GRAPHICS_MODE) {
        video_text_cursor_toggle(false);
        video_graphics_print(string);
        video_text_cursor_toggle(true);
    }

    lock_release(video_lock);
}

static const int32_t serial_ports[] = {0x3F8, 0x2F8, 0x3E8, 0x2E8};

void video_text_print(const char_t* string) {
    if(string == NULL) {
        return;
    }
    size_t i = 0;
    uint32_t apic_id = apic_get_local_apic_id();

    while(string[i] != '\0') {
        write_serial(serial_ports[apic_id], string[i]);
        i++;
    }
}

int8_t video_copy_contents_to_frame_buffer(uint8_t* buffer, uint64_t new_width, uint64_t new_height, uint64_t new_pixels_per_scanline) {
    if(!VIDEO_BASE_ADDRESS) {
        return -1;
    }

    UNUSED(new_width);
    UNUSED(new_height);

    int64_t j = 0;
    pixel_t* buf = (pixel_t*)buffer;

    int64_t i = 0;
    int64_t x = 0;

    while(i < VIDEO_PIXELS_PER_SCANLINE * VIDEO_GRAPHICS_HEIGHT) {
        buf[j] = *((pixel_t*)(VIDEO_BASE_ADDRESS + i));

        i++;
        j++;
        x++;

        if(x == VIDEO_GRAPHICS_WIDTH) {
            x = 0;
            i += VIDEO_PIXELS_PER_SCANLINE - VIDEO_GRAPHICS_WIDTH;
            j += (new_pixels_per_scanline - VIDEO_PIXELS_PER_SCANLINE);
        }
    }

    return 0;
}

int8_t video_move_text_cursor(int32_t x, int32_t y) {
    if(x >= VIDEO_GRAPHICS_WIDTH || y >= VIDEO_GRAPHICS_HEIGHT || x < 0 || y < 0) {
        return -1;
    }

    cursor_graphics_x = x;
    cursor_graphics_y = y;

    return 0;
}

int8_t video_move_text_cursor_relative(int32_t x, int32_t y) {
    if((cursor_graphics_x + x) >= VIDEO_GRAPHICS_WIDTH ||
       (cursor_graphics_y + y) >= VIDEO_GRAPHICS_HEIGHT ||
       (cursor_graphics_x + x) < 0 ||
       (cursor_graphics_y + y) < 0) {
        return -1;
    }

    cursor_graphics_x += x;
    cursor_graphics_y += y;

    return 0;
}

void video_text_cursor_get(int32_t* x, int32_t* y) {
    if(x) {
        *x = cursor_graphics_x;
    }

    if(y) {
        *y = cursor_graphics_y;
    }
}
