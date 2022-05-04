/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <efi.h>
#include <memory.h>
#include <strings.h>
#include <utils.h>

extern efi_system_table_t* ST;

void video_clear_screen() {
	ST->console_output->clear_screen(ST->console_output);
}

void video_print(char_t* string)
{
	if(string == NULL) {
		return;
	}

	wchar_t data[] = {0, 0};

	size_t i = 0;

	while(string[i] != '\0') {
		char_t c = string[i];

		if(c == '\n') {
			data[0] = '\r';
			ST->console_output->output_string(ST->console_output, data);
		}

		data[0] = c;

		ST->console_output->output_string(ST->console_output, data);

		data[0] = 0;

		i++;
	}
}

size_t video_printf(char_t* fmt, ...) {

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
