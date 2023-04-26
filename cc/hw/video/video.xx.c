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
wchar_t video_get_wc(char_t* string, int64_t * idx);
void    video_text_print(char_t* string);
void    video_graphics_print(char_t* string);


uint32_t* VIDEO_BASE_ADDRESS = NULL;
uint32_t VIDEO_PIXELS_PER_SCANLINE = 0;
uint8_t* FONT_ADDRESS = NULL;
int32_t FONT_WIDTH = 0;
int32_t FONT_HEIGHT = 0;
int32_t FONT_BYTES_PER_GLYPH = 0;
int32_t FONT_CHARS_PER_LINE = 0;
int32_t FONT_LINES_ON_SCREEN = 0;
boolean_t GRAPHICS_MODE = 0;
int32_t VIDEO_GRAPHICS_WIDTH = 0;
int32_t VIDEO_GRAPHICS_HEIGHT = 0;
uint32_t VIDEO_GRAPHICS_FOREGROUND = 0xFFFFFF;
uint32_t VIDEO_GRAPHICS_BACKGROUND = 0x000000;

wchar_t* video_font_unicode_table = NULL;

void video_refresh_frame_buffer_address(void) {
    VIDEO_BASE_ADDRESS = (uint32_t*)SYSTEM_INFO->frame_buffer->virtual_base_address;
}
void video_init(void) {
    VIDEO_BASE_ADDRESS = (uint32_t*)SYSTEM_INFO->frame_buffer->virtual_base_address;
    VIDEO_PIXELS_PER_SCANLINE = SYSTEM_INFO->frame_buffer->pixels_per_scanline;
    VIDEO_GRAPHICS_WIDTH = SYSTEM_INFO->frame_buffer->width;
    VIDEO_GRAPHICS_HEIGHT = SYSTEM_INFO->frame_buffer->height;

    video_psf2_font_t* font2 = (video_psf2_font_t*)&font_data_start;

    if(font2) {
        if(font2->magic == VIDEO_PSF2_FONT_MAGIC) {
            PRINTLOG(VIDEO, LOG_DEBUG, "font v2 ok");
            FONT_ADDRESS = (uint8_t*)&font_data_start + font2->header_size;
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

            GRAPHICS_MODE = VIDEO_BASE_ADDRESS != NULL?1:0;
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

                GRAPHICS_MODE = VIDEO_BASE_ADDRESS != NULL?1:0;
            } else {
                PRINTLOG(VIDEO, LOG_ERROR, "font magic err");
            }

        }
    } else {
        PRINTLOG(VIDEO, LOG_ERROR, "font err");
    }
}


void video_graphics_scroll(void){
    int64_t j = 0;

    for(int64_t i = FONT_HEIGHT * VIDEO_GRAPHICS_WIDTH; i < VIDEO_GRAPHICS_WIDTH * VIDEO_GRAPHICS_HEIGHT; i++) {
        *((pixel_t*)(VIDEO_BASE_ADDRESS + j)) = *((pixel_t*)(VIDEO_BASE_ADDRESS + i));
        j++;
    }

    for(int64_t i = FONT_HEIGHT * VIDEO_GRAPHICS_WIDTH * (FONT_LINES_ON_SCREEN - 1); i < VIDEO_GRAPHICS_WIDTH * VIDEO_GRAPHICS_HEIGHT; i++) {
        *((pixel_t*)(VIDEO_BASE_ADDRESS + i)) = VIDEO_GRAPHICS_BACKGROUND;
    }
}

wchar_t video_get_wc(char_t* string, int64_t * idx) {
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

void video_graphics_print(char_t* string) {
    int64_t i = 0;

    if(string == NULL || strlen(string) == 0) {
        return;
    }

    while(string[i]) {
        wchar_t wc = video_get_wc(string + i, &i);

        if(wc == '\r') {
            cursor_graphics_x = 0;
            i++;

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

            continue;
        }

        if(cursor_graphics_y >= FONT_LINES_ON_SCREEN) {
            video_graphics_scroll();
            cursor_graphics_y = FONT_LINES_ON_SCREEN - 1;
        }

        if(video_font_unicode_table) {
            wc = video_font_unicode_table[wc];
        }

        uint8_t* glyph = FONT_ADDRESS + (wc * FONT_BYTES_PER_GLYPH);

        int bytesperline = (FONT_WIDTH + 7) / 8;

        int32_t offs = (cursor_graphics_y * FONT_HEIGHT * VIDEO_PIXELS_PER_SCANLINE) + (cursor_graphics_x * FONT_WIDTH);

        int32_t x, y, line, mask;

        for(y = 0; y < FONT_HEIGHT; y++) {
            line = offs;
            mask = 1 << (FONT_WIDTH - 1);

            for(x = 0; x < FONT_WIDTH; x++) {
                *((pixel_t*)(VIDEO_BASE_ADDRESS + line)) = *((uint8_t*)glyph) & mask ? VIDEO_GRAPHICS_FOREGROUND : VIDEO_GRAPHICS_BACKGROUND;

                mask >>= 1;
                line++;
            }

            glyph += bytesperline;
            offs  += VIDEO_PIXELS_PER_SCANLINE;
        }

        cursor_graphics_x++;

        if(cursor_graphics_x >= FONT_CHARS_PER_LINE) {
            cursor_graphics_x = 0;
            cursor_graphics_y++;

            if(cursor_graphics_y >= FONT_LINES_ON_SCREEN) {
                video_graphics_scroll();
                cursor_graphics_y = FONT_LINES_ON_SCREEN - 1;
            }
        }

        i++;
    }
}

void video_clear_screen(){
    if(GRAPHICS_MODE) {
        for(int64_t i = 0; i < VIDEO_GRAPHICS_HEIGHT * VIDEO_GRAPHICS_WIDTH; i++) {
            *((pixel_t*)(VIDEO_BASE_ADDRESS + i)) = VIDEO_GRAPHICS_BACKGROUND;
        }

        cursor_graphics_x = 0;
        cursor_graphics_y = 0;
    }
}

void video_print(char_t* string) {
    video_text_print(string);
    if(GRAPHICS_MODE) {
        video_graphics_print(string);
    }
}

void video_text_print(char_t* string)
{
    if(string == NULL) {
        return;
    }
    size_t i = 0;
    while(string[i] != '\0') {
        write_serial(COM1, string[i]);
        i++;
    }
}

size_t video_printf(const char_t* fmt, ...){

    va_list args;
    va_start(args, fmt);

    size_t cnt = 0;

    while (*fmt) {
        char_t data = *fmt;

        if(data == '%') {
            fmt++;
            int8_t wfmtb = 1;
            char_t buf[257];
            char_t ito_buf[64];
            int32_t val = 0;
            char_t* str = NULL;
            int32_t slen = 0;
            number_t ival = 0;
            unumber_t uval = 0;
            int32_t idx = 0;
            int8_t l_flag = 0;
            int8_t sign = 0;
            char_t fto_buf[128];
            // float128_t fval = 0; // TODO: float128_t ops
            float64_t fval = 0;
            number_t prec = 6;

            while(1) {
                wfmtb = 1;

                switch (*fmt) {
                case '0':
                    fmt++;
                    val = *fmt - 0x30;
                    fmt++;
                    if(*fmt >= '0' && *fmt <= '9') {
                        val = val * 10 + *fmt - 0x30;
                        fmt++;
                    }
                    wfmtb = 0;
                    break;
                case '.':
                    fmt++;
                    prec = *fmt - 0x30;
                    fmt++;
                    wfmtb = 0;
                    break;
                case 'c':
                    val = va_arg(args, int32_t);
                    buf[0] = (char_t)val;
                    video_print(buf);
                    cnt++;
                    fmt++;
                    break;
                case 's':
                    str = va_arg(args, char_t*);
                    video_print(str);
                    cnt += strlen(str);
                    fmt++;
                    break;
                case 'i':
                case 'd':
                    if(l_flag == 2) {
                        ival = va_arg(args, int64_t);
                    } else if(l_flag == 1) {
                        ival = va_arg(args, int64_t);
                    }
                    if(l_flag == 0) {
                        ival = va_arg(args, int32_t);
                    }

                    itoa_with_buffer(ito_buf, ival);
                    slen = strlen(ito_buf);

                    if(ival < 0) {
                        sign = 1;
                        slen -= 2;
                    }

                    for(idx = 0; idx < val - slen; idx++) {
                        buf[idx] = '0';
                        cnt++;
                    }

                    if(ival < 0) {
                        buf[0] = '-';
                    }

                    video_print(buf);
                    memory_memclean(buf, 257);
                    video_print(ito_buf + sign);
                    memory_memclean(ito_buf, 64);

                    cnt += slen;
                    fmt++;
                    l_flag = 0;
                    break;
                case 'u':
                    if(l_flag == 2) {
                        uval = va_arg(args, uint64_t);
                    } else if(l_flag == 1) {
                        uval = va_arg(args, uint64_t);
                    }
                    if(l_flag == 0) {
                        uval = va_arg(args, uint32_t);
                    }

                    utoa_with_buffer(ito_buf, uval);
                    slen = strlen(ito_buf);

                    for(idx = 0; idx < val - slen; idx++) {
                        buf[idx] = '0';
                        cnt++;
                    }

                    video_print(buf);
                    memory_memclean(buf, 257);
                    video_print(ito_buf);
                    memory_memclean(ito_buf, 64);

                    cnt += slen;
                    fmt++;
                    break;
                case 'l':
                    fmt++;
                    wfmtb = 0;
                    l_flag++;
                    break;
                case 'p':
                    l_flag = 1;
                    nobreak;
                case 'x':
                case 'h':
                    if(l_flag == 2) {
                        uval = va_arg(args, uint64_t);
                    } else if(l_flag == 1) {
                        uval = va_arg(args, uint64_t);
                    }
                    if(l_flag == 0) {
                        uval = va_arg(args, uint32_t);
                    }

                    utoh_with_buffer(ito_buf, uval);
                    slen = strlen(ito_buf);

                    for(idx = 0; idx < val - slen; idx++) {
                        buf[idx] = '0';
                        cnt++;
                    }

                    video_print(buf);
                    memory_memclean(buf, 257);
                    video_print(ito_buf);
                    memory_memclean(ito_buf, 64);
                    cnt += slen;
                    fmt++;
                    l_flag = 0;
                    break;
                case '%':
                    buf[0] = '%';
                    video_print(buf);
                    memory_memclean(buf, 257);
                    fmt++;
                    cnt++;
                    break;
                case 'f':
                    if(l_flag == 2) {
                        // fval = va_arg(args, float128_t); // TODO: float128_t ops
                    } else  {
                        fval = va_arg(args, float64_t);
                    }

                    ftoa_with_buffer_and_prec(fto_buf, fval, prec); // TODO: floating point prec format
                    slen = strlen(fto_buf);
                    video_print(fto_buf);
                    memory_memclean(fto_buf, 128);

                    cnt += slen;
                    fmt++;
                    break;
                default:
                    break;
                }

                if(wfmtb) {
                    break;
                }
            }

        } else {
            size_t idx = 0;
            char_t buf[17];
            memory_memclean(buf, 17);

            while(*fmt) {
                if(idx == 16) {
                    video_printf(buf);
                    idx = 0;
                    memory_memclean(buf, 17);
                }
                if(*fmt == '%') {
                    video_print(buf);
                    memory_memclean(buf, 17);

                    break;
                }
                buf[idx++] = *fmt;
                fmt++;
                cnt++;
            }
            video_print(buf);
        }
    }

    va_end(args);

    return cnt;
}
