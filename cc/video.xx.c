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
