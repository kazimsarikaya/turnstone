/**
 * @file base64.64.c
 * @brief base64 encoder decoder implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <base64.h>
#include <memory.h>

MODULE("turnstone.lib");

#define NEWLINE_INVL 76
static const uint8_t charset[] = {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

uint8_t byte64_convert(uint8_t ch);

uint8_t byte64_convert(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'Z')
        ch -= 'A';
    else if (ch >= 'a' && ch <= 'z')
        ch = ch - 'a' + 26;
    else if (ch >= '0' && ch <= '9')
        ch = ch - '0' + 52;
    else if (ch == '+')
        ch = 62;
    else if (ch == '/')
        ch = 63;

    return(ch);
}


size_t base64_encode(const uint8_t* in, size_t len, boolean_t add_newline, uint8_t** out) {
    if(out == NULL) {
        return 0;
    }

    size_t blks = (len / 3);
    size_t left_over = len % 3;

    size_t out_len = blks * 4;

    if(left_over) {
        out_len += 4;
    }

    if(add_newline) {
        out_len += len / 57;
    }

    size_t in_idx = 0, out_idx = 0, newline_count = 0;

    uint8_t* tmp_out = memory_malloc(out_len);

    if(tmp_out == NULL) {
        return 0;
    }

    for(; in_idx < blks * 3; in_idx += 3, out_idx += 4) {
        tmp_out[out_idx]     = charset[in[in_idx] >> 2];
        tmp_out[out_idx + 1] = charset[((in[in_idx] & 0x03) << 4) | (in[in_idx + 1] >> 4)];
        tmp_out[out_idx + 2] = charset[((in[in_idx + 1] & 0x0f) << 2) | (in[in_idx + 2] >> 6)];
        tmp_out[out_idx + 3] = charset[in[in_idx + 2] & 0x3F];

        if (((out_idx - newline_count + 4) % NEWLINE_INVL == 0) && add_newline) {
            tmp_out[out_idx + 4] = '\n';
            out_idx++;
            newline_count++;
        }

    }

    if (left_over == 1) {
        tmp_out[out_idx]     = charset[in[in_idx] >> 2];
        tmp_out[out_idx + 1] = charset[(in[in_idx] & 0x03) << 4];
        tmp_out[out_idx + 2] = '=';
        tmp_out[out_idx + 3] = '=';
        out_idx += 4;
    }
    else if (left_over == 2) {
        tmp_out[out_idx]     = charset[in[in_idx] >> 2];
        tmp_out[out_idx + 1] = charset[((in[in_idx] & 0x03) << 4) | (in[in_idx + 1] >> 4)];
        tmp_out[out_idx + 2] = charset[(in[in_idx + 1] & 0x0F) << 2];
        tmp_out[out_idx + 3] = '=';
        out_idx += 4;
    }

    if(out_idx != out_len) {
        memory_free(tmp_out);

        return 0;
    }

    *out = tmp_out;

    return out_idx;
}


size_t base64_decode(const uint8_t* in, size_t len, uint8_t** out) {
    if(out == NULL) {
        return NULL;
    }

    if (in[len - 1] == '=') {
        len--;
    }

    if (in[len - 1] == '=') {
        len--;
    }

    size_t blks = len / 4;
    size_t left_over = len % 4;

    size_t newline_count = 0;

    if(len >= 77 && in[NEWLINE_INVL] == '\n') {
        newline_count = len / (NEWLINE_INVL + 1);
    }

    size_t out_len = (len - newline_count) / 4 * 3;

    if(((len - newline_count) % 4) == 2) {
        out_len++;
    } else if(((len - newline_count) % 4) == 3) {
        out_len += 2;
    }

    uint8_t* tmp_out = memory_malloc(out_len);

    if(tmp_out == NULL) {
        return 0;
    }

    size_t in_idx = 0, out_idx = 0;

    for (; in_idx < blks * 4; out_idx += 3, in_idx += 4) {

        if (in[in_idx] == '\n') {
            in_idx++;
        }

        tmp_out[out_idx]     = (byte64_convert(in[in_idx]) << 2) | ((byte64_convert(in[in_idx + 1]) & 0x30) >> 4);
        tmp_out[out_idx + 1] = (byte64_convert(in[in_idx + 1]) << 4) | (byte64_convert(in[in_idx + 2]) >> 2);
        tmp_out[out_idx + 2] = (byte64_convert(in[in_idx + 2]) << 6) | byte64_convert(in[in_idx + 3]);
    }

    if (left_over == 2) {
        tmp_out[out_idx]     = (byte64_convert(in[in_idx]) << 2) | ((byte64_convert(in[in_idx + 1]) & 0x30) >> 4);
        out_idx++;
    } else if (left_over == 3) {
        tmp_out[out_idx]     = (byte64_convert(in[in_idx]) << 2) | ((byte64_convert(in[in_idx + 1]) & 0x30) >> 4);
        tmp_out[out_idx + 1] = (byte64_convert(in[in_idx + 1]) << 4) | (byte64_convert(in[in_idx + 2]) >> 2);
        out_idx += 2;
    }

    if(out_idx != out_len) {
        memory_free(tmp_out);

        return 0;
    }

    *out = tmp_out;

    return out_idx;
}
