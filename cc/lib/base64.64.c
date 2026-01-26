/**
 * @file base64.64.c
 * @brief base64 encoder decoder implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <base64.h>
#include <memory.h>
#include <utils.h>

MODULE("turnstone.lib");

#define NEWLINE_RFC2045 76
#define NEWLINE_RFC7468 64

static const uint8_t charset[] = {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

uint8_t byte64_convert(uint8_t ch);

uint8_t byte64_convert(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        ch -= 'A';
    }else if (ch >= 'a' && ch <= 'z') {
        ch = ch - 'a' + 26;
    }else if (ch >= '0' && ch <= '9') {
        ch = ch - '0' + 52;
    }else if (ch == '+') {
        ch = 62;
    }else if (ch == '/') {
        ch = 63;
    }

    return(ch);
}

static size_t base64_encode_internal(const uint8_t* in, size_t len, boolean_t add_newline, size_t newline_pos, uint8_t** out) {
    if (out == NULL || (add_newline && newline_pos == 0)) {
        return 0;
    }

    size_t blks = len / 3;
    size_t left_over = len % 3;

    // 1. Calculate Raw Base64 Length
    size_t raw_len = blks * 4;
    if (left_over) {
        raw_len += 4;
    }

    // 2. Calculate Allocation Size
    // We allocate enough for the data + newlines.
    // Logic: 1 newline for every 'newline_pos' characters, plus potential 1 at the end.
    // We add a small safety margin (+2) to avoid complex integer math edge cases.
    size_t alloc_len = raw_len;
    if (add_newline) {
        alloc_len += (raw_len / newline_pos) + 2;
    }

    // Optional: Add space for null terminator if your OS prefers strings to be safe
    // alloc_len += 1;

    uint8_t* tmp_out = memory_malloc(alloc_len);
    if (tmp_out == NULL) {
        return 0;
    }

    size_t in_idx = 0;
    size_t out_idx = 0;
    size_t line_chars = 0; // Track characters on the current line

    // 3. Main Encoding Loop
    for (in_idx = 0; in_idx < blks * 3; in_idx += 3) {
        tmp_out[out_idx++] = charset[in[in_idx] >> 2];
        tmp_out[out_idx++] = charset[((in[in_idx] & 0x03) << 4) | (in[in_idx + 1] >> 4)];
        tmp_out[out_idx++] = charset[((in[in_idx + 1] & 0x0f) << 2) | (in[in_idx + 2] >> 6)];
        tmp_out[out_idx++] = charset[in[in_idx + 2] & 0x3F];

        line_chars += 4;

        // Add newline if we hit the limit
        if (add_newline && line_chars >= newline_pos) {
            tmp_out[out_idx++] = '\n';
            line_chars = 0;
        }
    }

    // 4. Handle Padding (Leftovers)
    if (left_over == 1) {
        tmp_out[out_idx++] = charset[in[in_idx] >> 2];
        tmp_out[out_idx++] = charset[(in[in_idx] & 0x03) << 4];
        tmp_out[out_idx++] = '=';
        tmp_out[out_idx++] = '=';
        line_chars += 4;
    } else if (left_over == 2) {
        tmp_out[out_idx++] = charset[in[in_idx] >> 2];
        tmp_out[out_idx++] = charset[((in[in_idx] & 0x03) << 4) | (in[in_idx + 1] >> 4)];
        tmp_out[out_idx++] = charset[(in[in_idx + 1] & 0x0F) << 2];
        tmp_out[out_idx++] = '=';
        line_chars += 4;
    }

    // 5. Force Final Newline (If requested and not already at start of new line)
    // If add_newline is true, the file MUST end with \n.
    // If line_chars == 0, we just wrote a newline in the loop, so we are good.
    // If line_chars > 0, we have data sitting on the last line, so we must terminate it.
    if (add_newline && line_chars > 0) {
        tmp_out[out_idx++] = '\n';
    }

    *out = tmp_out;
    return out_idx; // Return the ACTUAL bytes written
}

size_t base64_encode(const uint8_t* in, size_t len, uint8_t** out) {
    return base64_encode_internal(in, len, false, 0, out);
}

size_t base64_encode_rfc7468(const uint8_t* in, size_t len, uint8_t** out) {
    return base64_encode_internal(in, len, true, NEWLINE_RFC7468, out);
}

size_t base64_encode_rfc2045(const uint8_t* in, size_t len, uint8_t** out) {
    return base64_encode_internal(in, len, true, NEWLINE_RFC2045, out);
}

size_t base64_decode(const uint8_t* in, size_t len, uint8_t** out) {
    if (out == NULL || in == NULL) {
        return 0;
    }

    // --- Pass 1: Calculate output size ---
    // We count valid characters to determine exact allocation size.
    size_t valid_chars = 0;
    size_t padding_equals = 0;

    for (size_t i = 0; i < len; i++) {
        if (in[i] == '=') {
            padding_equals++;
        } else if (!isspace((char_t)in[i])) { // Using your helper
            valid_chars++;
        }
    }

    // 4 Base64 chars = 3 Bytes.
    size_t total_blocks = (valid_chars + padding_equals) / 4;

    // Allocate exactly what is needed
    size_t alloc_len = total_blocks * 3;

    uint8_t* tmp_out = memory_malloc(alloc_len);
    if (tmp_out == NULL) {
        return 0;
    }

    // --- Pass 2: Decode ---
    size_t out_idx = 0;
    uint8_t quantum[4]; // Accumulator for 4 valid chars
    size_t q_count = 0;

    for (size_t i = 0; i < len; i++) {
        uint8_t c = in[i];

        // 1. Skip Whitespace (using your helper)
        if (isspace((char_t)c)) {
            continue;
        }

        // 2. Handle Padding or End
        if (c == '=') {
            break; // Stop decoding at padding
        }

        // 3. Accumulate valid char
        quantum[q_count++] = c;

        // 4. Decode when we have a full block (4 chars)
        if (q_count == 4) {
            uint32_t val = (byte64_convert(quantum[0]) << 18) |
                           (byte64_convert(quantum[1]) << 12) |
                           (byte64_convert(quantum[2]) << 6)  |
                           (byte64_convert(quantum[3]));

            tmp_out[out_idx++] = (val >> 16) & 0xFF;
            tmp_out[out_idx++] = (val >> 8) & 0xFF;
            tmp_out[out_idx++] = val & 0xFF;

            q_count = 0; // Reset
        }
    }

    // --- Handle Leftovers (Robustness for non-padded inputs) ---
    // If the input string didn't have '=' padding, we might still have data in 'quantum'
    if (q_count > 1) {
        uint32_t val = (byte64_convert(quantum[0]) << 18) |
                       (byte64_convert(quantum[1]) << 12);

        if (q_count > 2) {
            val |= (byte64_convert(quantum[2]) << 6);
        }

        tmp_out[out_idx++] = (val >> 16) & 0xFF;

        if (q_count > 2) {
            tmp_out[out_idx++] = (val >> 8) & 0xFF;
        }
    }

    *out = tmp_out;
    return out_idx;
}
