/**
 * @file buffer.64.c
 * @brief buffer implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <buffer.h>
#include <cpu/sync.h>
#include <utils.h>
#include <strings.h>

MODULE("turnstone.lib");

#define BUFFER_PRINTF_BUFFER_SIZE 2048

uint8_t buffer_tmp_buffer_area_for_printf[BUFFER_PRINTF_BUFFER_SIZE];

typedef struct buffer_t {
    memory_heap_t* heap;
    lock_t         lock;
    uint64_t       capacity;
    uint64_t       length;
    uint64_t       position;
    boolean_t      readonly;
    uint8_t*       data;
}buffer_t;

buffer_t buffer_tmp_buffer_for_printf = {
    .heap = NULL,
    .lock = NULL,
    .capacity = BUFFER_PRINTF_BUFFER_SIZE,
    .length = 0,
    .position = 0,
    .readonly = false,
    .data = buffer_tmp_buffer_area_for_printf
};

buffer_t* buffer_get_tmp_buffer_for_printf(void) {
    return &buffer_tmp_buffer_for_printf;
}

void buffer_reset_tmp_buffer_for_printf(void) {
    buffer_tmp_buffer_for_printf.length = 0;
    buffer_tmp_buffer_for_printf.position = 0;
    memory_memclean(buffer_tmp_buffer_for_printf.data, buffer_tmp_buffer_for_printf.capacity);
}

buffer_t* buffer_new_with_capacity(memory_heap_t* heap, uint64_t capacity) {
    heap = memory_get_heap(heap);

    buffer_t* buffer = memory_malloc_ext(heap, sizeof(buffer_t), 0);

    if(buffer == NULL) {
        return NULL;
    }

    if(capacity == 0) {
        capacity = 1;
    }

    buffer->heap = heap;
    buffer->lock = lock_create_with_heap(buffer->heap);
    buffer->capacity = capacity;
    buffer->data = memory_malloc_ext(buffer->heap, buffer->capacity, 0);

    if(buffer->data == NULL) {
        memory_free_ext(heap, buffer);

        return NULL;
    }

    return buffer;
}

memory_heap_t* buffer_get_heap(buffer_t* buffer) {
    if(!buffer) {
        return NULL;
    }

    return buffer->heap;
}

buffer_t* buffer_encapsulate(uint8_t* data, uint64_t length) {
    buffer_t* buffer = memory_malloc_ext(NULL, sizeof(buffer_t), 0);

    if(buffer == NULL) {
        return NULL;
    }

    buffer->lock = lock_create_with_heap(buffer->heap);
    buffer->capacity = length;
    buffer->length = length;
    buffer->readonly = true;
    buffer->data = data;

    return buffer;
}

boolean_t buffer_set_readonly(buffer_t* buffer, boolean_t ro) {
    if(!buffer) {
        return false;
    }

    buffer->readonly = ro;

    return true;
}

uint64_t buffer_get_length(buffer_t* buffer) {
    if(!buffer) {
        return 0;
    }

    return buffer->length;
};

boolean_t buffer_reset(buffer_t* buffer) {
    if(!buffer) {
        return false;
    }

    lock_acquire(buffer->lock);

    memory_memclean(buffer->data, buffer->length);

    buffer->length = 0;
    buffer->position = 0;

    lock_release(buffer->lock);

    return true;
};

uint64_t buffer_get_capacity(buffer_t* buffer) {
    if(!buffer) {
        return 0;
    }

    return buffer->capacity;
};

uint64_t buffer_get_position(buffer_t* buffer) {
    if(!buffer) {
        return 0;
    }

    return buffer->position;
};

static boolean_t buffer_resize_if_need(buffer_t* buffer, uint64_t length) {
    uint64_t new_cap = buffer->capacity;

    while(buffer->position + length > new_cap) {
        if(new_cap > (1 << 20)) {
            new_cap += 1 << 20;
        } else {
            new_cap *= 2;
        }
    }

    if(buffer->capacity != new_cap) {
        uint8_t* tmp_data = memory_malloc_ext(buffer->heap, new_cap, 0);

        if(tmp_data == NULL) {
            return false;
        }

        buffer->capacity = new_cap;

        memory_memcopy(buffer->data, tmp_data, buffer->length);
        memory_free_ext(buffer->heap, buffer->data);
        buffer->data = tmp_data;
    }

    return true;
}

buffer_t* buffer_append_byte(buffer_t* buffer, uint8_t data) {
    if(!buffer) {
        return NULL;
    }

    if(buffer->readonly) {
        return NULL;
    }

    lock_acquire(buffer->lock);

    if(!buffer_resize_if_need(buffer, 1)) {
        lock_release(buffer->lock);

        return NULL;
    }

    buffer->data[buffer->position] = data;
    buffer->position++;

    if(buffer->length < buffer->position) {
        buffer->length = buffer->position;
    }

    lock_release(buffer->lock);

    return buffer;
}

buffer_t* buffer_append_bytes(buffer_t* buffer, uint8_t* data, uint64_t length) {
    if(!buffer) {
        return NULL;
    }

    if(buffer->readonly) {
        return NULL;
    }

    lock_acquire(buffer->lock);

    if(!buffer_resize_if_need(buffer, length)) {
        lock_release(buffer->lock);

        return NULL;
    }

    memory_memcopy(data, buffer->data + buffer->position, length);

    buffer->position += length;

    if(buffer->length < buffer->position) {
        buffer->length = buffer->position;
    }

    lock_release(buffer->lock);

    return buffer;
}

buffer_t* buffer_append_buffer(buffer_t* buffer, buffer_t* appenden) {
    return buffer_append_bytes(buffer, appenden->data, appenden->length);
}

uint8_t* buffer_get_bytes(buffer_t* buffer, uint64_t length) {
    if(length == 0) {
        return NULL;
    }

    lock_acquire(buffer->lock);

    if(buffer->position >= buffer->length) {
        lock_release(buffer->lock);

        return NULL;
    }

    uint8_t* res = memory_malloc_ext(buffer->heap, length, 0);
    memory_memcopy(buffer->data + buffer->position, res, length);

    buffer->position += length;

    lock_release(buffer->lock);

    return res;
}

uint8_t* buffer_get_all_bytes(buffer_t* buffer, uint64_t* length) {
    uint8_t* res = memory_malloc_ext(buffer->heap, buffer->length, 0);

    if(!res) {
        if(length) {
            *length = 0;
        }

        return NULL;
    }

    memory_memcopy(buffer->data, res, buffer->length);

    if(length) {
        *length = buffer->length;
    }

    return res;
}

uint8_t* buffer_get_all_bytes_and_reset(buffer_t* buffer, uint64_t* length) {
    lock_acquire(buffer->lock);

    uint8_t* res = buffer->data;

    if(length) {
        *length = buffer->length;
    }

    buffer->data = memory_malloc_ext(buffer->heap, buffer->capacity, 0);
    buffer->length = 0;
    buffer->position = 0;

    lock_release(buffer->lock);

    return res;
}

uint8_t* buffer_get_all_bytes_and_destroy(buffer_t* buffer, uint64_t* length) {
    if(!buffer) {
        return NULL;
    }

    uint8_t* res = buffer->data;

    if(length) {
        *length = buffer->length;
    }

    lock_destroy(buffer->lock);
    memory_free_ext(buffer->heap, buffer);

    return res;
}

uint8_t  buffer_get_byte(buffer_t* buffer) {
    lock_acquire(buffer->lock);

    if(buffer->position >= buffer->length) {
        lock_release(buffer->lock);

        return NULL;
    }
    uint8_t res = buffer->data[buffer->position];
    buffer->position++;

    lock_release(buffer->lock);

    return res;
}

uint8_t buffer_peek_byte(buffer_t* buffer) {
    if(!buffer) {
        return 0;
    }

    if(buffer->position + 1  > buffer->length) {
        return 0;
    }

    return buffer->data[buffer->position];
}

uint8_t buffer_peek_byte_at_position(buffer_t* buffer, uint64_t position) {
    if(!buffer) {
        return 0;
    }

    if(position + 1  > buffer->length) {
        return 0;
    }

    return buffer->data[position];
}

uint64_t buffer_peek_ints_at_position(buffer_t* buffer, uint64_t position, uint8_t bc) {
    if(!buffer) {
        return 0;
    }

    if(bc <= 0 || bc > 8) {
        return 0;
    }

    if(position + bc  > buffer->length) {
        return 0;
    }

    uint64_t* res_p = (uint64_t*)&buffer->data[position];
    uint64_t res = *res_p;

    switch(bc) {
    case 1:
        res &= 0xFF;
        break;
    case 2:
        res &= 0xFFFF;
        break;
    case 3:
        res &= 0xFFFFFF;
        break;
    case 4:
        res &= 0xFFFFFFFF;
        break;
    case 5:
        res &= 0xFFFFFFFFFF;
        break;
    case 6:
        res &= 0xFFFFFFFFFFFF;
        break;
    case 7:
        res &= 0xFFFFFFFFFFFFFF;
        break;
    case 8:
        break;
    }

    return res;
}

uint64_t buffer_peek_ints(buffer_t* buffer, uint8_t bc) {
    if(!buffer) {
        return 0;
    }

    return buffer_peek_ints_at_position(buffer, buffer->position, bc);
}

uint64_t buffer_remaining(buffer_t* buffer) {
    lock_acquire(buffer->lock);

    uint64_t res = buffer->length - buffer->position;

    lock_release(buffer->lock);

    return res;
}


boolean_t   buffer_seek(buffer_t* buffer, int64_t position, buffer_seek_direction_t direction) {
    lock_acquire(buffer->lock);

    if(direction == BUFFER_SEEK_DIRECTION_START) {
        if(position < 0 || (uint64_t)position >= buffer->capacity) {
            lock_release(buffer->lock);

            return false;
        }

        buffer->position = 0;
    }

    if(direction == BUFFER_SEEK_DIRECTION_END) {
        if (position > 0 || position + ((int64_t)buffer->capacity) < 0) {
            lock_release(buffer->lock);

            return false;
        }

        buffer->position = buffer->capacity;
    }

    if(direction == BUFFER_SEEK_DIRECTION_CURRENT && (buffer->position + position > buffer->capacity || ((int64_t)buffer->position) + position < 0)) {
        lock_release(buffer->lock);

        return false;
    }

    buffer->position += position;

    if(buffer->position > buffer->length) {
        buffer->length = buffer->position;
    }

    lock_release(buffer->lock);

    return true;
}

int8_t buffer_destroy(buffer_t* buffer) {
    if(!buffer) {
        return 0;
    }

    lock_destroy(buffer->lock);

    if(!buffer->readonly) {
        memory_free_ext(buffer->heap, buffer->data);
    }

    memory_free_ext(buffer->heap, buffer);

    return 0;
}

boolean_t buffer_write_slice_into(buffer_t* buffer, uint64_t pos, uint64_t len, uint8_t* dest) {
    if(!buffer || !dest) {
        return false;
    }

    if(pos > buffer->length) {
        return false;
    }

    if(len > buffer->length - pos) {
        len = buffer->length - pos;
    }

    if(!len) {
        return false;
    }

    memory_memcopy(buffer->data + pos, dest, len);

    return true;
}

uint8_t* buffer_get_view_at_position(buffer_t* buffer, uint64_t position, uint64_t length) {
    if(!buffer) {
        return NULL;
    }

    if(position + length > buffer->length) {
        return NULL;
    }

    return buffer->data + position;
}


int64_t buffer_printf(buffer_t* buffer, const char_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int64_t res = buffer_vprintf(buffer, fmt, args);

    va_end(args);

    return res;
}

int64_t buffer_vprintf(buffer_t* buffer, const char_t* fmt, va_list args) {
    if(!buffer) {
        return 0;
    }

    size_t cnt = 0;

    if(!fmt) {
        return 0;
    }

    char_t video_printf_buffer[BUFFER_PRINTF_BUFFER_SIZE + 128] = {0};
    uint64_t video_printf_buffer_idx = 0;

    while (*fmt) {
        if(video_printf_buffer_idx >= BUFFER_PRINTF_BUFFER_SIZE - 1) {
            buffer_append_bytes(buffer, (uint8_t*)video_printf_buffer, video_printf_buffer_idx);
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
                    video_printf_buffer[video_printf_buffer_idx++] = (char_t)'%';
                    video_printf_buffer[video_printf_buffer_idx] = '\0';
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
                if(video_printf_buffer_idx >= BUFFER_PRINTF_BUFFER_SIZE - 1) {
                    buffer_append_bytes(buffer, (uint8_t*)video_printf_buffer, video_printf_buffer_idx);
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
        buffer_append_bytes(buffer, (uint8_t*)video_printf_buffer, video_printf_buffer_idx);
    }

    return cnt;
}

char_t* buffer_read_line_ext(buffer_t* buffer, char_t line_continuation_char, char_t delimiter_char, uint64_t* length) {
    if(!buffer) {
        return NULL;
    }

    if(buffer->position >= buffer->length) {
        return NULL;
    }

    buffer_t* res_buffer = buffer_new_with_capacity(buffer->heap, 128);

    if(!res_buffer) {
        if(length) {
            *length = -1;
        }

        return NULL;
    }

    while(buffer->position < buffer->length) {
        if(buffer->data[buffer->position] == delimiter_char) {
            buffer->position++;
            break;
        }

        if(buffer->position + 1 < buffer->length && buffer->data[buffer->position] == line_continuation_char && buffer->data[buffer->position + 1] == '\n') {
            buffer->position += 2;
            continue;
        }

        buffer_append_byte(res_buffer, buffer->data[buffer->position]);

        buffer->position++;
    }

    buffer_append_byte(res_buffer, '\0');

    char_t* res = (char_t*)buffer_get_all_bytes_and_destroy(res_buffer, length);

    return res;
}
