/**
 * @file video.xx.c
 * @brief video operations
 *
 * prints string to video buffer and moves cursor
 */
#include <video.h>
#include <memory.h>
#include <faraccess.h>
#include <ports.h>
#include <strings.h>
#include <utils.h>
#include <systeminfo.h>

/*! main video buffer segment */
#define VIDEO_SEG 0xB800
/*! default color for video: white foreground over black background*/
#define WHITE_ON_BLACK  ((0 << 4) | ( 15 & 0x0F))

uint16_t cursor_text_x = 0; ///< cursor postion for column
uint16_t cursor_text_y = 0; ///< cursor porsition for row
uint16_t cursor_graphics_x = 0; ///< cursor postion for column
uint16_t cursor_graphics_y = 0; ///< cursor porsition for row

extern uint8_t _binary_output_font_ps_start;
extern uint8_t _binary_output_font_ps_end;

void put_char(char_t c, int32_t cx, int32_t cy, uint32_t fg, uint32_t bg);

void video_text_print(char_t* string);

/**
 * @brief scrolls video up for one line
 */
void video_text_scroll();

void video_graphics_scroll();
/**
 * @brief moves cursor to new location.
 */
void video_move_cursor();


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

void video_init() {
	VIDEO_BASE_ADDRESS = (uint32_t*)SYSTEM_INFO->frame_buffer->base_address;
	VIDEO_PIXELS_PER_SCANLINE = SYSTEM_INFO->frame_buffer->pixels_per_scanline;
	VIDEO_GRAPHICS_WIDTH = SYSTEM_INFO->frame_buffer->width;
	VIDEO_GRAPHICS_HEIGHT = SYSTEM_INFO->frame_buffer->height;

	video_psf2_font_t* font2 = (video_psf2_font_t*)&_binary_output_font_ps_start;

	if(font2) {
		if(font2->magic == VIDEO_PSF2_FONT_MAGIC) {
			printf("font v2 ok\n");
			FONT_ADDRESS = (uint8_t*)&_binary_output_font_ps_start + font2->header_size;
			FONT_WIDTH = font2->width;
			FONT_HEIGHT = font2->height;
			FONT_BYTES_PER_GLYPH = font2->bytes_per_glyph;

			FONT_CHARS_PER_LINE = VIDEO_GRAPHICS_WIDTH / FONT_WIDTH;
			FONT_LINES_ON_SCREEN = VIDEO_GRAPHICS_HEIGHT / FONT_HEIGHT;

			GRAPHICS_MODE = VIDEO_BASE_ADDRESS != NULL?1:0;
		} else {

			video_psf1_font_t* font1 = (video_psf1_font_t*)&_binary_output_font_ps_start;

			if(font1->magic == VIDEO_PSF1_FONT_MAGIC) {
				printf("font v1 ok\n");
				uint64_t addr = (uint64_t)&_binary_output_font_ps_start;
				addr += sizeof(video_psf1_font_t);
				FONT_ADDRESS = (uint8_t*)addr;
				FONT_WIDTH = 10;
				FONT_HEIGHT = 16;
				FONT_BYTES_PER_GLYPH = font1->bytes_per_glyph;

				FONT_CHARS_PER_LINE = VIDEO_GRAPHICS_WIDTH / FONT_WIDTH;
				FONT_LINES_ON_SCREEN = VIDEO_GRAPHICS_HEIGHT / FONT_HEIGHT;

				GRAPHICS_MODE = VIDEO_BASE_ADDRESS != NULL?1:0;
			} else {
				printf("font magic err\n");
			}

		}
	} else {
		printf("font err\n");
	}
}


void video_graphics_scroll(){
	int64_t j = 0;

	for(int64_t i = FONT_HEIGHT * VIDEO_GRAPHICS_WIDTH; i < VIDEO_GRAPHICS_WIDTH * VIDEO_GRAPHICS_HEIGHT; i++) {
		*((pixel_t*)(VIDEO_BASE_ADDRESS + j)) = *((pixel_t*)(VIDEO_BASE_ADDRESS + i));
		j++;
	}

	for(int64_t i = FONT_HEIGHT * VIDEO_GRAPHICS_WIDTH * (FONT_LINES_ON_SCREEN - 1); i < VIDEO_GRAPHICS_WIDTH * VIDEO_GRAPHICS_HEIGHT; i++) {
		*((pixel_t*)(VIDEO_BASE_ADDRESS + i)) = VIDEO_GRAPHICS_BACKGROUND;
	}
}

void video_graphics_print(char_t* string) {
	int64_t i = 0;

	while(string[i]) {
		if(string[i] == '\r') {
			cursor_graphics_x = 0;
			i++;

			continue;
		}
		if(string[i] == '\n') {
			cursor_graphics_x = 0;
			cursor_graphics_y++;
			i++;

			if(cursor_graphics_y >= FONT_LINES_ON_SCREEN) {
				video_graphics_scroll();
				cursor_graphics_y = FONT_LINES_ON_SCREEN - 1;
			}

			continue;
		}


		uint8_t* glyph = FONT_ADDRESS + (string[i] * FONT_BYTES_PER_GLYPH);

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
	size_t i;
	uint16_t blank = ' ' | (WHITE_ON_BLACK << 8);
	for(i = 0; i < VIDEO_BUF_LEN * 2; i += 2) {
		far_write_16(VIDEO_SEG, i, blank);
	}

	cursor_text_x = 0;
	cursor_text_y = 0;

	video_move_cursor();

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
		char_t c = string[i];
		if( c == '\r') {
			cursor_text_x = 0;
		} else if ( c == '\n') {
			cursor_text_x = 0;
			cursor_text_y++;
			if (cursor_text_y >= 25) {
				cursor_text_y = 24;
				video_text_scroll();
			}
		} else if ( c >= ' ') {
			uint16_t location = (cursor_text_y * 80 + cursor_text_x) * 2;
			far_write_16(VIDEO_SEG, location, c | (WHITE_ON_BLACK << 8));
			cursor_text_x++;
			if(cursor_text_x == 80) {
				cursor_text_x = 0;
				cursor_text_y++;
				if (cursor_text_y >= 25) {
					cursor_text_y = 24;
					video_text_scroll();
				}
			}
		}
		i++;
	}
	video_move_cursor();
}

void video_print_at(char_t* string, uint8_t x, uint8_t y) {
	cursor_text_x = x;
	cursor_text_y = y;
	video_print(string);
}

void video_move_cursor(){
	uint16_t cursor = cursor_text_y * 80 + cursor_text_x;
	outb(0x3D4, 14);
	outb(0x3D5, cursor >> 8);
	outb(0x3D4, 15);
	outb(0x3D5, cursor);
}

void video_text_scroll() {
	for(size_t i = 0; i < 80 * 24; i++) {
		uint16_t data = far_read_16(VIDEO_SEG, (i * 2) + 160);
		far_write_16(VIDEO_SEG, i * 2, data);
	}
	uint16_t blank = ' ' | (WHITE_ON_BLACK << 8);
	for(size_t i = 80 * 24; i < 80 * 25; i++) {
		far_write_16(VIDEO_SEG, i * 2, blank);
	}
}

size_t video_printf(char_t* fmt, ...){

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
			char_t* str;
			int32_t slen = 0;
			number_t ival = 0;
			unumber_t uval = 0;
			int32_t idx = 0;
			int8_t l_flag = 0;
			int8_t sign = 0;

#if ___BITS == 64
			char_t fto_buf[128];
			// float128_t fval = 0; // TODO: float128_t ops
			float64_t fval = 0;
			number_t prec = 6;
#endif

			while(1) {
				wfmtb = 1;

				switch (*fmt) {
				case '0':
					fmt++;
					val = *fmt - 0x30;
					fmt++;
					wfmtb = 0;
					break;
#if ___BITS == 64
				case '.':
					fmt++;
					prec = *fmt - 0x30;
					fmt++;
					wfmtb = 0;
					break;
#endif
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
#if ___BITS == 64
					if(l_flag == 2) {
						ival = va_arg(args, int128_t);
					} else if(l_flag == 1) {
						ival = va_arg(args, int64_t);
					}
#endif
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
#if ___BITS == 64
					if(l_flag == 2) {
						uval = va_arg(args, uint128_t);
					} else if(l_flag == 1) {
						uval = va_arg(args, uint64_t);
					}
#endif
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
				case 'x':
				case 'h':
#if ___BITS == 64
					if(l_flag == 2) {
						uval = va_arg(args, uint128_t);
					} else if(l_flag == 1) {
						uval = va_arg(args, uint64_t);
					}
#endif
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
					break;
				case '%':
					buf[0] = '%';
					video_print(buf);
					memory_memclean(buf, 257);
					fmt++;
					cnt++;
					break;
#if ___BITS == 64
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
#endif
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
