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

/*! main video buffer segment */
#define VIDEO_SEG 0xB800
/*! default color for video: white foreground over black background*/
#define WHITE_ON_BLACK  ((0 << 4) | ( 15 & 0x0F))

uint16_t cursor_x = 0; ///< cursor postion for column
uint16_t cursor_y = 0; ///< cursor porsition for row


/**
 * @brief scrolls video up for one line
 */

void video_scroll();
/**
 * @brief moves cursor to new location.
 */
void video_move_cursor();

void video_clear_screen(){
	size_t i;
	uint16_t blank = ' ' | (WHITE_ON_BLACK << 8);
	for(i = 0; i < VIDEO_BUF_LEN * 2; i += 2) {
		far_write_16(VIDEO_SEG, i, blank);
	}
}

void video_print(char_t* string)
{
	if(string == NULL) {
		return;
	}
	size_t i = 0;
	while(string[i] != '\0') {
		write_serial(COM1, string[i]);
		char_t c = string[i];
		if( c == '\r') {
			cursor_x = 0;
		} else if ( c == '\n') {
			cursor_x = 0;
			cursor_y++;
			if (cursor_y >= 25) {
				cursor_y = 24;
				video_scroll();
			}
		} else if ( c >= ' ') {
			uint16_t location = (cursor_y * 80 + cursor_x) * 2;
			far_write_16(VIDEO_SEG, location, c | (WHITE_ON_BLACK << 8));
			cursor_x++;
			if(cursor_x == 80) {
				cursor_x = 0;
				cursor_y++;
				if (cursor_y >= 25) {
					cursor_y = 24;
					video_scroll();
				}
			}
		}
		i++;
	}
	video_move_cursor();
}

void video_print_at(char_t* string, uint8_t x, uint8_t y) {
	cursor_x = x;
	cursor_y = y;
	video_print(string);
}

void video_move_cursor(){
	uint16_t cursor = cursor_y * 80 + cursor_x;
	outb(0x3D4, 14);
	outb(0x3D5, cursor >> 8);
	outb(0x3D4, 15);
	outb(0x3D5, cursor);
}

void video_scroll() {
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
			int32_t val = 0;
			char_t* str;
			int32_t slen = 0;
			size_t ival = 0;
			int32_t idx = 0;

			while(1) {
				wfmtb = 1;

				switch (*fmt) {
				case '0':
					fmt++;
					val = *fmt - 0x30;
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
					ival = va_arg(args, size_t);
					str = itoa(ival);
					slen = strlen(str);
					for(idx = 0; idx < val - slen; idx++) {
						buf[idx] = '0';
						cnt++;
					}
					video_print(buf);
					memory_memclean(buf, 257);
					video_print(str);
					memory_free(str);
					cnt += slen;
					fmt++;
					break;
				case 'l':
					fmt++;
					wfmtb = 0;
					break;
				case 'p':
				case 'x':
				case 'h':
					ival = va_arg(args, size_t);
					str = itoh(ival);
					slen = strlen(str);
					for(idx = 0; idx < val - slen; idx++) {
						buf[idx] = '0';
						cnt++;
					}
					video_print(buf);
					memory_memclean(buf, 257);
					video_print(str);
					memory_free(str);
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
