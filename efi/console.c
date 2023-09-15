/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <efi.h>
#include <memory.h>
#include <strings.h>
#include <utils.h>

MODULE("turnstone.efi");

extern efi_system_table_t* ST;

void   video_print(char_t* string);
void   video_clear_screen(void);
size_t video_printf(const char_t* fmt, ...);

void video_clear_screen(void) {
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

#define VIDEO_PRINTF_BUFFER_SIZE 2048

size_t video_printf(const char_t* fmt, ...){

    va_list args;
    va_start(args, fmt);


    size_t cnt = 0;

    if(!fmt) {
        return 0;
    }

    char_t video_printf_buffer[VIDEO_PRINTF_BUFFER_SIZE + 128] = {0};
    uint64_t video_printf_buffer_idx = 0;

    while (*fmt) {
        if(video_printf_buffer_idx >= VIDEO_PRINTF_BUFFER_SIZE - 1) {
            video_print(video_printf_buffer);
            video_printf_buffer_idx = 0;
        }

        char_t data = *fmt;

        if(data == '%') {
            fmt++;
            int8_t wfmtb = 1;
            char_t buf[257] = {0};
            char_t ito_buf[64] = {0};
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
                    video_printf_buffer[video_printf_buffer_idx++] = (char_t)val;
                    video_printf_buffer[video_printf_buffer_idx] = '\0';
                    cnt++;
                    fmt++;
                    break;
                case 's':
                    str = va_arg(args, char_t*);
                    slen = strlen(str);

                    strcpy(str, video_printf_buffer + video_printf_buffer_idx);
                    video_printf_buffer_idx += slen;
                    video_printf_buffer[video_printf_buffer_idx] = '\0';

                    cnt += slen;
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
                        buf[idx + 1] = '\0';
                        cnt++;
                    }

                    if(ival < 0) {
                        buf[0] = '-';
                    }

                    strcpy(buf, video_printf_buffer + video_printf_buffer_idx);
                    video_printf_buffer_idx += idx;
                    video_printf_buffer[video_printf_buffer_idx] = '\0';

                    strcpy(ito_buf + sign, video_printf_buffer + video_printf_buffer_idx);
                    video_printf_buffer_idx += slen;
                    video_printf_buffer[video_printf_buffer_idx] = '\0';

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
                        buf[idx + 1] = '\0';
                        cnt++;
                    }

                    strcpy(buf, video_printf_buffer + video_printf_buffer_idx);
                    video_printf_buffer_idx += idx;
                    video_printf_buffer[video_printf_buffer_idx] = '\0';

                    strcpy(ito_buf + sign, video_printf_buffer + video_printf_buffer_idx);
                    video_printf_buffer_idx += slen;
                    video_printf_buffer[video_printf_buffer_idx] = '\0';

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
                        buf[idx + 1] = '\0';
                        cnt++;
                    }

                    strcpy(buf, video_printf_buffer + video_printf_buffer_idx);
                    video_printf_buffer_idx += idx;
                    video_printf_buffer[video_printf_buffer_idx] = '\0';

                    strcpy(ito_buf + sign, video_printf_buffer + video_printf_buffer_idx);
                    video_printf_buffer_idx += slen;
                    video_printf_buffer[video_printf_buffer_idx] = '\0';

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

                    strcpy(fto_buf, video_printf_buffer + video_printf_buffer_idx);
                    video_printf_buffer_idx += slen;
                    video_printf_buffer[video_printf_buffer_idx] = '\0';

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

            while(*fmt) {
                if(video_printf_buffer_idx >= VIDEO_PRINTF_BUFFER_SIZE - 1) {
                    video_print(video_printf_buffer);
                    video_printf_buffer_idx = 0;
                }
                if(*fmt == '%') {
                    break;
                }

                video_printf_buffer[video_printf_buffer_idx++] = *fmt;
                video_printf_buffer[video_printf_buffer_idx] = 0;
                fmt++;
                cnt++;
            }
        }
    }

    if(video_printf_buffer_idx) {
        video_print(video_printf_buffer);
    }

    va_end(args);

    return cnt;
}

