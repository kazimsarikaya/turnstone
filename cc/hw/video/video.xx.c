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

extern video_psf2_font_t font_data_start;
extern video_psf2_font_t font_data_end;

/**
 * @brief scrolls video up for one line
 */
void    video_graphics_scroll(void);
wchar_t video_get_wc(const char_t* string, int64_t * idx);
void    video_text_print(const char_t* string);
void    video_graphics_print(const char_t* string);
void    video_display_flush_dummy(uint32_t scanout, uint64_t offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height);


uint32_t* VIDEO_BASE_ADDRESS = NULL;
uint32_t VIDEO_PIXELS_PER_SCANLINE = 0;
video_display_flush_f VIDEO_DISPLAY_FLUSH = NULL;
video_move_cursor_f VIDEO_MOVE_CURSOR = NULL;

uint8_t* FONT_ADDRESS = NULL;
int32_t FONT_WIDTH = 0;
int32_t FONT_HEIGHT = 0;
int32_t FONT_BYTES_PER_GLYPH = 0;
int32_t FONT_CHARS_PER_LINE = 0;
int32_t FONT_LINES_ON_SCREEN = 0;
uint32_t FONT_MASK = 0;
uint32_t FONT_BYTES_PERLINE = 0;
boolean_t GRAPHICS_MODE = 0;
int32_t VIDEO_GRAPHICS_WIDTH = 0;
int32_t VIDEO_GRAPHICS_HEIGHT = 0;
uint32_t VIDEO_GRAPHICS_FOREGROUND = 0xFFFFFF;
uint32_t VIDEO_GRAPHICS_BACKGROUND = 0x000000;

lock_t* video_lock = NULL;

wchar_t* video_font_unicode_table = NULL;

void video_set_color(uint32_t foreground, uint32_t background) {
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

    FONT_CHARS_PER_LINE = VIDEO_GRAPHICS_WIDTH / FONT_WIDTH;
    FONT_LINES_ON_SCREEN = VIDEO_GRAPHICS_HEIGHT / FONT_HEIGHT;

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

    VIDEO_DISPLAY_FLUSH = video_display_flush_dummy;

    video_psf2_font_t* font2 = (video_psf2_font_t*)&font_data_start;

    video_lock = lock_create();

    if(font2) {
        if(font2->magic == VIDEO_PSF2_FONT_MAGIC) {
            PRINTLOG(VIDEO, LOG_DEBUG, "font v2 ok");
            FONT_ADDRESS = ((uint8_t*)&font_data_start) + font2->header_size;
            FONT_WIDTH = font2->width;
            FONT_HEIGHT = font2->height;
            FONT_BYTES_PER_GLYPH = font2->bytes_per_glyph;

            if(font2->flags) {
                wchar_t glyph = 0;
                video_font_unicode_table = memory_malloc(sizeof(wchar_t) * ((wchar_t)-1));
                uint8_t* font_unicode_table = FONT_ADDRESS + font2->glyph_count * font2->bytes_per_glyph;

                while(font_unicode_table < (uint8_t*)&font_data_end) {
                    wchar_t wc = *font_unicode_table;

                    if(wc == 0xFF) {
                        glyph++;
                        font_unicode_table++;

                        continue;
                    } else if(wc & 128) {
                        if((wc & 32) == 0 ) {
                            wc = ((font_unicode_table[0] & 0x1F) << 6) + (font_unicode_table[1] & 0x3F);
                            font_unicode_table++;
                        } else if((wc & 16) == 0 ) {
                            wc = ((((font_unicode_table[0] & 0xF) << 6) + (font_unicode_table[1] & 0x3F)) << 6) + (font_unicode_table[2] & 0x3F);
                            font_unicode_table += 2;
                        } else if((wc & 8) == 0 ) {
                            wc = ((((((font_unicode_table[0] & 0x7) << 6) + (font_unicode_table[1] & 0x3F)) << 6) + (font_unicode_table[2] & 0x3F)) << 6) + (font_unicode_table[3] & 0x3F);
                            font_unicode_table += 3;
                        } else {
                            wc = 0;
                        }

                    }

                    video_font_unicode_table[wc] = glyph;
                    font_unicode_table++;
                }

            }

            FONT_CHARS_PER_LINE = VIDEO_GRAPHICS_WIDTH / FONT_WIDTH;
            FONT_LINES_ON_SCREEN = VIDEO_GRAPHICS_HEIGHT / FONT_HEIGHT;

            FONT_BYTES_PERLINE = (FONT_WIDTH + 7) / 8;

            FONT_MASK = 1 << (FONT_BYTES_PERLINE * 8 - 1);


            GRAPHICS_MODE = VIDEO_BASE_ADDRESS != NULL;
        } else {

            video_psf1_font_t* font1 = (video_psf1_font_t*)&font_data_start;

            if(font1->magic == VIDEO_PSF1_FONT_MAGIC) {
                PRINTLOG(VIDEO, LOG_DEBUG, "font v1 ok");
                uint64_t addr = (uint64_t)&font_data_start;
                addr += sizeof(video_psf1_font_t);
                FONT_ADDRESS = (uint8_t*)addr;
                FONT_WIDTH = 10;
                FONT_HEIGHT = 20;
                FONT_BYTES_PER_GLYPH = font1->bytes_per_glyph;

                if(font1->mode) {
                    wchar_t glyph = 0;
                    video_font_unicode_table = memory_malloc(sizeof(wchar_t) * ((wchar_t)-1));
                    uint8_t* font_unicode_table = FONT_ADDRESS + 512 * font1->bytes_per_glyph;

                    while(font_unicode_table < (uint8_t*)&font_data_end) {
                        wchar_t wc = *font_unicode_table;

                        if(wc == 0xFF) {
                            glyph++;
                            font_unicode_table++;

                            continue;
                        } else if(wc & 128) {
                            if((wc & 32) == 0 ) {
                                wc = ((font_unicode_table[0] & 0x1F) << 6) + (font_unicode_table[1] & 0x3F);
                                font_unicode_table++;
                            } else if((wc & 16) == 0 ) {
                                wc = ((((font_unicode_table[0] & 0xF) << 6) + (font_unicode_table[1] & 0x3F)) << 6) + (font_unicode_table[2] & 0x3F);
                                font_unicode_table += 2;
                            } else if((wc & 8) == 0 ) {
                                wc = ((((((font_unicode_table[0] & 0x7) << 6) + (font_unicode_table[1] & 0x3F)) << 6) + (font_unicode_table[2] & 0x3F)) << 6) + (font_unicode_table[3] & 0x3F);
                                font_unicode_table += 3;
                            } else {
                                wc = 0;
                            }

                        }

                        video_font_unicode_table[wc] = glyph;
                        font_unicode_table++;
                    }

                }

                FONT_CHARS_PER_LINE = VIDEO_GRAPHICS_WIDTH / FONT_WIDTH;
                FONT_LINES_ON_SCREEN = VIDEO_GRAPHICS_HEIGHT / FONT_HEIGHT;

                FONT_BYTES_PERLINE = (FONT_WIDTH + 7) / 8;

                FONT_MASK = 1 << (FONT_BYTES_PERLINE * 8 - 1);

                GRAPHICS_MODE = VIDEO_BASE_ADDRESS != NULL;
            } else {
                PRINTLOG(VIDEO, LOG_ERROR, "font magic err");
            }

        }
    } else {
        PRINTLOG(VIDEO, LOG_ERROR, "font err");
    }

    if(!GRAPHICS_MODE) {
        video_text_print("graphics mode not active");
    }
}


void video_graphics_scroll(void){
    int64_t j = 0;
    int64_t i = (FONT_HEIGHT * VIDEO_PIXELS_PER_SCANLINE) / 2;
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

    i = (FONT_HEIGHT * VIDEO_PIXELS_PER_SCANLINE * (FONT_LINES_ON_SCREEN - 1)) / 2;
    x = 0;

    while(i < VIDEO_PIXELS_PER_SCANLINE * VIDEO_GRAPHICS_HEIGHT / 2) {
        dst[i] = VIDEO_GRAPHICS_BACKGROUND;

        i++;
        x++;

        if(x == VIDEO_GRAPHICS_WIDTH) {
            x = 0;
            i += empty_area_size;
        }
    }
}

wchar_t video_get_wc(const char_t* string, int64_t * idx) {
    if(string == NULL || idx == NULL) {
        return NULL;
    }

    wchar_t wc = string[0];

    if(wc & 128) {
        if((wc & 32) == 0) {
            wc = ((string[0] & 0x1F) << 6) + (string[1] & 0x3F);
            (*idx)++;
        } else if((wc & 16) == 0 ) {
            wc = ((((string[0] & 0xF) << 6) + (string[1] & 0x3F)) << 6) + (string[2] & 0x3F);
            (*idx) += 2;
        } else if((wc & 8) == 0 ) {
            wc = ((((((string[0] & 0x7) << 6) + (string[1] & 0x3F)) << 6) + (string[2] & 0x3F)) << 6) + (string[3] & 0x3F);
            (*idx) += 3;
        } else {
            wc = 0;
        }
    }

    return wc;
}

static void video_print_glyph(wchar_t wc) {
    uint8_t* glyph = FONT_ADDRESS + (wc * FONT_BYTES_PER_GLYPH);

    int32_t offs = (cursor_graphics_y * FONT_HEIGHT * VIDEO_PIXELS_PER_SCANLINE) + (cursor_graphics_x * FONT_WIDTH);

    int32_t x, y, line, mask;

    for(y = 0; y < FONT_HEIGHT; y++) {
        line = offs;
        mask = FONT_MASK;

        uint32_t tmp = BYTE_SWAP32(*((uint32_t*)glyph));

        for(x = 0; x < FONT_WIDTH; x++) {

            *((pixel_t*)(VIDEO_BASE_ADDRESS + line)) = tmp & mask ? VIDEO_GRAPHICS_FOREGROUND : VIDEO_GRAPHICS_BACKGROUND;

            mask >>= 1;
            line++;
        }

        glyph += FONT_BYTES_PERLINE;
        offs  += VIDEO_PIXELS_PER_SCANLINE;
    }
}

static void video_text_cursor_toggle(boolean_t flush) {
    int32_t offs = (cursor_graphics_y * FONT_HEIGHT * VIDEO_PIXELS_PER_SCANLINE) + (cursor_graphics_x * FONT_WIDTH);
    uint32_t orig_offs = offs;

    int32_t x, y, line;

    for(y = 0; y < FONT_HEIGHT; y++) {
        line = offs;

        for(x = 0; x < FONT_WIDTH; x++) {

            *((pixel_t*)(VIDEO_BASE_ADDRESS + line)) = ~(*((pixel_t*)(VIDEO_BASE_ADDRESS + line)));

            line++;
        }

        offs  += VIDEO_PIXELS_PER_SCANLINE;
    }

    if(flush) {
        VIDEO_DISPLAY_FLUSH(0, orig_offs * sizeof(pixel_t), cursor_graphics_x * FONT_WIDTH, cursor_graphics_y * FONT_HEIGHT, FONT_WIDTH, FONT_HEIGHT);
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
        wchar_t wc = video_get_wc(string + i, &i);

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
                    cursor_graphics_x = FONT_CHARS_PER_LINE - 1;
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

                if(cursor_graphics_x >= FONT_CHARS_PER_LINE) {
                    cursor_graphics_x = cursor_graphics_x % FONT_CHARS_PER_LINE;
                    cursor_graphics_y++;
                }
            }

            i++;

            if(cursor_graphics_y >= FONT_LINES_ON_SCREEN) {
                video_graphics_scroll();
                cursor_graphics_y = FONT_LINES_ON_SCREEN - 1;
                full_flush = true;
            }

            continue;
        }

        if(cursor_graphics_y >= FONT_LINES_ON_SCREEN) {
            video_graphics_scroll();
            cursor_graphics_y = FONT_LINES_ON_SCREEN - 1;
            full_flush = true;
        }

        if(video_font_unicode_table) {
            wc = video_font_unicode_table[wc];
        }

        video_print_glyph(wc);

        cursor_graphics_x++;

        if(cursor_graphics_x >= FONT_CHARS_PER_LINE) {
            cursor_graphics_x = 0;
            cursor_graphics_y++;

            if(cursor_graphics_y >= FONT_LINES_ON_SCREEN) {
                video_graphics_scroll();
                cursor_graphics_y = FONT_LINES_ON_SCREEN - 1;
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
        flush_offset = (old_cursor_graphics_y * FONT_HEIGHT * VIDEO_PIXELS_PER_SCANLINE) + (old_cursor_graphics_x * FONT_WIDTH);

        if(old_cursor_graphics_y == cursor_graphics_y) {
            min_x = old_cursor_graphics_x * FONT_WIDTH;
            max_x = cursor_graphics_x * FONT_WIDTH;
            min_y = old_cursor_graphics_y * FONT_HEIGHT;
            max_y = min_y + FONT_HEIGHT;
        } else {
            min_x = old_cursor_graphics_x * FONT_WIDTH;
            max_x = max_cursor_graphics_x * FONT_WIDTH;
            min_y = old_cursor_graphics_y * FONT_HEIGHT;
            max_y = min_y + FONT_HEIGHT;

            VIDEO_DISPLAY_FLUSH(0, flush_offset * sizeof(pixel_t), min_x, min_y, max_x - min_x, max_y - min_y);

            min_x = 0;
            max_x = max_cursor_graphics_x * FONT_WIDTH;
            min_y = (old_cursor_graphics_y + 1) * FONT_HEIGHT;
            max_y = (cursor_graphics_y * FONT_HEIGHT) + FONT_HEIGHT;

            flush_offset = min_y * VIDEO_PIXELS_PER_SCANLINE;
        }

        VIDEO_DISPLAY_FLUSH(0, flush_offset * sizeof(pixel_t), min_x, min_y, max_x - min_x, max_y - min_y);
    }
}

void video_clear_screen(void){
    if(GRAPHICS_MODE) {
        for(int64_t i = 0; i < VIDEO_GRAPHICS_HEIGHT * VIDEO_GRAPHICS_WIDTH; i++) {
            *((pixel_t*)(VIDEO_BASE_ADDRESS + i)) = VIDEO_GRAPHICS_BACKGROUND;
        }

        cursor_graphics_x = 0;
        cursor_graphics_y = 0;

        VIDEO_DISPLAY_FLUSH(0, 0, 0, 0, VIDEO_GRAPHICS_WIDTH, VIDEO_GRAPHICS_HEIGHT);
    }
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
    if(GRAPHICS_MODE) {
        return -1;
    }

    if(x >= VIDEO_GRAPHICS_WIDTH || y >= VIDEO_GRAPHICS_HEIGHT || x < 0 || y < 0) {
        return -1;
    }

    cursor_graphics_x = x;
    cursor_graphics_y = y;

    return 0;
}
