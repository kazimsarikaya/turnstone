/**
 * @file deflate.64.c
 * @brief deflate implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <deflate.h>
#include <utils.h>
#include <quicksort.h>
#include <logging.h>

MODULE("turnstone.lib");

#define DEFLATE_MAX_MATCH (258)
#define DEFLATE_MIN_MATCH (3)
#define DEFLATE_WINDOW_SIZE 32768
#define DEFLATE_HASHTABLE_SIZE 15
#define DEFLATE_HASHTABLE_MUL 2654435761U
#define DEFLATE_HASHTABLE_PREV_SIZE 32765
#define DEFLATE_NO_POS (-1)
#define DEFLATE_MAX_BLOCK_SIZE 65535


typedef struct deflate_match_t {
    int32_t best_size;
    int64_t best_pos;
} deflate_match_t;

typedef struct deflate_hashtable_t {
    int64_t prev[DEFLATE_HASHTABLE_PREV_SIZE];
    int64_t head[1ULL << DEFLATE_HASHTABLE_SIZE];
} deflate_hashtable_t;

typedef struct bit_buffer_t {
    buffer_t* buffer;
    uint8_t   byte;
    uint8_t   bit_count;
} bit_buffer_t;

typedef struct huffman_decode_table_t {
    uint16_t counts[16];
    uint16_t symbols[288];
} huffman_decode_table_t;

typedef struct huffman_encode_table_t {
    uint16_t codes[288];
    uint8_t  lengths[288];
} huffman_encode_table_t;

typedef struct huffman_encode_freq_t {
    uint64_t literal_freqs[288];
    uint64_t distance_freqs[30];
    uint64_t extra_bits_count;
} huffman_encode_freq_t;

const huffman_encode_table_t huffman_encode_fixed = {
    {
        0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
        0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
        0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
        0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
        0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
        0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
        0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
        0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
        0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
        0x13, 0x113, 0x93, 0x193, 0x53, 0x153, 0xd3, 0x1d3, 0x33, 0x133, 0xb3, 0x1b3, 0x73, 0x173, 0xf3, 0x1f3,
        0x0b, 0x10b, 0x8b, 0x18b, 0x4b, 0x14b, 0xcb, 0x1cb, 0x2b, 0x12b, 0xab, 0x1ab, 0x6b, 0x16b, 0xeb, 0x1eb,
        0x1b, 0x11b, 0x9b, 0x19b, 0x5b, 0x15b, 0xdb, 0x1db, 0x3b, 0x13b, 0xbb, 0x1bb, 0x7b, 0x17b, 0xfb, 0x1fb,
        0x07, 0x107, 0x87, 0x187, 0x47, 0x147, 0xc7, 0x1c7, 0x27, 0x127, 0xa7, 0x1a7, 0x67, 0x167, 0xe7, 0x1e7,
        0x17, 0x117, 0x97, 0x197, 0x57, 0x157, 0xd7, 0x1d7, 0x37, 0x137, 0xb7, 0x1b7, 0x77, 0x177, 0xf7, 0x1f7,
        0x0f, 0x10f, 0x8f, 0x18f, 0x4f, 0x14f, 0xcf, 0x1cf, 0x2f, 0x12f, 0xaf, 0x1af, 0x6f, 0x16f, 0xef, 0x1ef,
        0x1f, 0x11f, 0x9f, 0x19f, 0x5f, 0x15f, 0xdf, 0x1df, 0x3f, 0x13f, 0xbf, 0x1bf, 0x7f, 0x17f, 0xff, 0x1ff,
        0x00, 0x40, 0x20, 0x60, 0x10, 0x50, 0x30, 0x70, 0x08, 0x48, 0x28, 0x68, 0x18, 0x58, 0x38, 0x78,
        0x04, 0x44, 0x24, 0x64, 0x14, 0x54, 0x34, 0x74, 0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xa3,
    },
    {
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 0-15
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 16-31
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 32-47
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 48-63
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 64-79
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 80-95
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 96-111
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 112-127
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 128-143
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 144-159
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 160-175
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 176-191
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 192-207
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 208-223
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 224-239
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 240-255
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, // 256-279
        8, 8, 8, 8, 8, 8, 8, 8, // 280-287
    },
};

const huffman_encode_table_t huffman_encode_distance_fixed = {
    {
        0x00, 0x10, 0x08, 0x18, 0x04, 0x14, 0x0c, 0x1c, 0x02, 0x12, 0x0a, 0x1a, 0x06, 0x16, 0x0e, 0x1e, // 0-15
        0x01, 0x11, 0x09, 0x19, 0x05, 0x15, 0x0d, 0x1d, 0x03, 0x13, 0x0b, 0x1b, 0x07, 0x17, 0x0f, 0x1f, // 16-31
    },
    {
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    },
};

const huffman_decode_table_t huffman_decode_fixed_lengths = {
    {
        0, 0, 0, 0, 0, 0, 0, 24, 152, 112, 0, 0, 0, 0, 0, 0,
    },
    {
        256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271,
        272, 273, 274, 275, 276, 277, 278, 279, 0, 1, 2, 3, 4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
        40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
        72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
        88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103,
        104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
        120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135,
        136, 137, 138, 139, 140, 141, 142, 143, 280, 281, 282, 283, 284, 285, 286, 287,
        144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
    }
};

const huffman_decode_table_t huffman_decode_fixed_distances = {
    {
        0, 0, 0, 0, 0, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    }
};

const uint16_t huffman_length_base[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
};

_Static_assert(sizeof(huffman_length_base) / sizeof(huffman_length_base[0]) == 29, "huffman_length_base must have 29 elements");

const uint16_t huffman_length_extra_bits[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
};

_Static_assert(sizeof(huffman_length_extra_bits) / sizeof(huffman_length_extra_bits[0]) == 29, "huffman_length_extra_bits must have 29 elements");

static inline int16_t huffman_find_length_index(uint16_t length) {
    if (length < 3) {
        return -1;
    }

    if (length > 258) {
        return -1;
    }

    if (length == 258) {
        return 28;
    }

    uint16_t i = 0;
    while (huffman_length_base[i] <= length) {
        i++;
    }

    return i - 1;
}

const uint16_t huffman_distance_base[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
};

_Static_assert(sizeof(huffman_distance_base) / sizeof(huffman_distance_base[0]) == 30, "huffman_distance_base must have 30 elements");

const uint16_t huffman_distance_extra_bits[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
};

_Static_assert(sizeof(huffman_distance_extra_bits) / sizeof(huffman_distance_extra_bits[0]) == 30, "huffman_distance_extra_bits must have 30 elements");

static inline int16_t huffman_find_distance_index(uint16_t distance) {
    if (distance < 1) {
        return -1;
    }

    if (distance > 32768) {
        return -1;
    }

    if (distance >= 24577) {
        return 29;
    }

    uint16_t i = 0;
    while (huffman_distance_base[i] <= distance) {
        i++;
    }

    return i - 1;
}

const uint8_t huffman_code_lengths[] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
};

int8_t deflate_inflate_uncompressed_block(bit_buffer_t* bit_buffer, buffer_t* out);
int8_t deflate_inflate_block(bit_buffer_t* bit_buffer, buffer_t* out, huffman_decode_table_t* lengths, huffman_decode_table_t* distances);
int8_t huffman_decode_table_decode(bit_buffer_t* bit_buffer, huffman_decode_table_t* huffman_lengths, huffman_decode_table_t* huffman_distances);

static inline int64_t bit_buffer_get(bit_buffer_t* bit_buffer, uint8_t bit_count) {
    int64_t result = 0;

    for (uint8_t i = 0; i < bit_count; i++) {
        if(bit_buffer->bit_count == 0) {
            if(buffer_remaining(bit_buffer->buffer) == 0) {
                PRINTLOG(COMPRESSION, LOG_ERROR, "unexpected end of input stream");

                return -1;
            }

            bit_buffer->byte = buffer_get_byte(bit_buffer->buffer);
            bit_buffer->bit_count = 8;
        }

        int64_t bit = bit_buffer->byte & 1;

        bit_buffer->byte >>= 1;
        bit_buffer->bit_count--;

        result |= bit << i;

    }

    return result;
}

static inline int8_t bit_buffer_put(bit_buffer_t* bit_buffer, uint8_t bit_count, uint64_t bits) {
    for (uint8_t i = 0; i < bit_count; i++) {
        if(bit_buffer->bit_count == 8) {
            if(!buffer_append_byte(bit_buffer->buffer, bit_buffer->byte)) {
                PRINTLOG(COMPRESSION, LOG_ERROR, "failed to append byte to buffer");

                return -1;
            }

            bit_buffer->bit_count = 0;
            bit_buffer->byte = 0;
        }

        bit_buffer->byte |= (bits & 1) << bit_buffer->bit_count;
        bits >>= 1;
        bit_buffer->bit_count++;
    }

    return 0;
}

static inline int8_t bit_buffer_push(bit_buffer_t* bit_buffer) {
    if(bit_buffer->bit_count != 0) {
        if(!buffer_append_byte(bit_buffer->buffer, bit_buffer->byte)) {
            PRINTLOG(COMPRESSION, LOG_ERROR, "failed to append byte to buffer");

            return -1;
        }

        bit_buffer->bit_count = 0;
        bit_buffer->byte = 0;
    }

    return 0;
}

static inline void bitbuffer_discard(bit_buffer_t* bit_buffer) {
    bit_buffer->bit_count = 0;
    bit_buffer->byte = 0;
}

static inline int16_t bit_buffer_get_16le(bit_buffer_t* bit_buffer) {
    int16_t result = buffer_get_byte(bit_buffer->buffer);

    result |= buffer_get_byte(bit_buffer->buffer) << 8;

    return result;
}

int8_t deflate_inflate_uncompressed_block(bit_buffer_t* bit_buffer, buffer_t* out) {
    bitbuffer_discard(bit_buffer); // Align to byte boundary

    uint16_t len = bit_buffer_get_16le(bit_buffer);
    uint16_t nlen = bit_buffer_get_16le(bit_buffer);

    if (len != (~nlen & 0xFFFF)) {
        return -1;
    }

    uint8_t* data = buffer_get_bytes(bit_buffer->buffer, len);

    buffer_append_bytes(out, data, len);

    memory_free(data);

    return 0;
}

static int32_t huffman_decode(bit_buffer_t* bit_buffer, huffman_decode_table_t* huffman_decode_table, int32_t max_symbol) {
    int32_t count = 0, cur = 0;

    for (int32_t i = 1; cur >= 0; i++) {
        int64_t bit = bit_buffer_get(bit_buffer, 1);

        if (bit < 0) {
            return -1;
        }

        cur = (cur << 1) | bit;
        count += huffman_decode_table->counts[i];
        cur -= huffman_decode_table->counts[i];
    }

    if (count + cur < 0 || count + cur > max_symbol) {
        PRINTLOG(COMPRESSION, LOG_ERROR, "invalid huffman symbol index %i %i", count, cur);

        return -1;
    }

    return huffman_decode_table->symbols[count + cur];
}

int8_t deflate_inflate_block(bit_buffer_t* bit_buffer, buffer_t* out, huffman_decode_table_t* lengths, huffman_decode_table_t* distances) {
    while(true) {
        int32_t symbol = huffman_decode(bit_buffer, lengths, 288);

        if(symbol < 0) {
            PRINTLOG(COMPRESSION, LOG_ERROR, "corruption before %lli %i", buffer_get_position(bit_buffer->buffer), bit_buffer->bit_count);

            return -1;
        }

        if(symbol < 256) {
            buffer_append_byte(out, symbol);
        } else if(symbol == 256) {
            break;
        } else {
            symbol -= 257;

            int32_t length = bit_buffer_get(bit_buffer, huffman_length_extra_bits[symbol]) + huffman_length_base[symbol];

            if(length < 0) {
                PRINTLOG(COMPRESSION, LOG_ERROR, "corruption before %lli %i", buffer_get_position(bit_buffer->buffer), bit_buffer->bit_count);

                return -1;
            }

            int32_t distance_symbol = huffman_decode(bit_buffer, distances, 30);

            if(distance_symbol < 0) {
                PRINTLOG(COMPRESSION, LOG_ERROR, "corruption before %lli %i", buffer_get_position(bit_buffer->buffer), bit_buffer->bit_count);

                return -1;
            }

            int32_t distance = bit_buffer_get(bit_buffer, huffman_distance_extra_bits[distance_symbol]) + huffman_distance_base[distance_symbol];

            if(distance < 0) {
                PRINTLOG(COMPRESSION, LOG_ERROR, "corruption before %lli %i", buffer_get_position(bit_buffer->buffer), bit_buffer->bit_count);

                return -1;
            }

            uint64_t current_position = buffer_get_position(out);
            current_position -= distance;

            for (int32_t i = 0; i < length; i++) {
                buffer_append_byte(out, buffer_peek_byte_at_position(out, current_position++));
            }

        }
    }

    return 0;
}

static inline int8_t huffman_decode_table_build(uint8_t* lengths, size_t size, struct huffman_decode_table_t* out, uint32_t max_length) {

    uint16_t offsets[16];
    uint32_t count = 0;

    for(uint32_t i = 0; i < 16; i++) {
        out->counts[i] = 0;
    }

    for(uint32_t i = 0; i < size; ++i) {
        if(lengths[i] > max_length) {
            return -1;
        }

        out->counts[lengths[i]]++;
    }

    out->counts[0] = 0;

    for (uint32_t i = 0; i < 16; ++i) {
        offsets[i] = count;
        count += out->counts[i];
    }

    for (uint32_t i = 0; i < size; ++i) {
        if (lengths[i]) {
            out->symbols[offsets[lengths[i]]++] = i;
        }
    }

    return 0;
}


int8_t huffman_decode_table_decode(bit_buffer_t* bit_buffer, huffman_decode_table_t* huffman_lengths, huffman_decode_table_t* huffman_distances) {
    memory_memclean(huffman_lengths, sizeof(huffman_decode_table_t));
    memory_memclean(huffman_distances, sizeof(huffman_decode_table_t));

    uint8_t lengths[320];
    memory_memclean(lengths, sizeof(lengths));

    int32_t bit = 0;

    bit = bit_buffer_get(bit_buffer, 5);

    if(bit < 0) {
        return -1;
    }

    uint32_t literals  = 257 + bit;

    bit = bit_buffer_get(bit_buffer, 5);

    if(bit < 0) {
        return -1;
    }

    uint32_t distances = 1 + bit;

    bit = bit_buffer_get(bit_buffer, 4);

    if(bit < 0) {
        return -1;
    }

    uint32_t clengths  = 4 + bit;

    for (uint32_t i = 0; i < clengths; i++) {
        bit = bit_buffer_get(bit_buffer, 3);

        if(bit < 0) {
            return -1;
        }

        lengths[huffman_code_lengths[i]] = bit;
    }

    huffman_decode_table_t codes;
    memory_memclean(&codes, sizeof(huffman_decode_table_t));

    if(huffman_decode_table_build(lengths, 19, &codes, 7) < 0) {
        return -1;
    }

    uint32_t count = 0;

    while (count < literals + distances) {
        int32_t symbol = huffman_decode(bit_buffer, &codes, 19);

        if(count == 0 && symbol == 16) {
            return -1;
        }

        if (symbol < 0) {
            return -1;
        }else if (symbol < 16) {
            lengths[count++] = symbol;
        } else if (symbol < 19) {
            int32_t rep = 0, length;

            if (symbol == 16) {
                rep = lengths[count - 1];

                bit = bit_buffer_get(bit_buffer, 2);

                if(bit < 0) {
                    return -1;
                }

                length = bit + 3;
            } else if (symbol == 17) {
                bit = bit_buffer_get(bit_buffer, 3);

                if(bit < 0) {
                    return -1;
                }

                length = bit + 3;
            } else if (symbol == 18) {
                bit = bit_buffer_get(bit_buffer, 7);

                if(bit < 0) {
                    return -1;
                }

                length = bit + 11;
            }

            do {
                lengths[count++] = rep;
                length--;
            } while (length);

        } else {
            return -1;
        }
    }

    if(huffman_decode_table_build(lengths, literals, huffman_lengths, 15) < 0) {
        return -1;
    }

    if(huffman_decode_table_build(lengths + literals, distances, huffman_distances, 15) < 0) {
        return -1;
    }

    return 0;
}

static inline uint32_t deflate_hash4(uint32_t data) {
    return (data * DEFLATE_HASHTABLE_MUL) >> (32 - DEFLATE_HASHTABLE_SIZE);
}

static inline void deflate_hash_insert(int64_t pos, uint32_t h, deflate_hashtable_t* ht) {
    ht->prev[pos % DEFLATE_WINDOW_SIZE] = ht->head[h];
    ht->head[h] = pos;
}

static deflate_match_t deflate_find_bestmatch (buffer_t* in, int64_t in_len, int64_t in_p, deflate_hashtable_t* ht) {
    int64_t max_match = MIN(in_len - in_p, DEFLATE_MAX_MATCH);
    int64_t start = in_p - DEFLATE_WINDOW_SIZE;

    if(start < 0) {
        start = 0;
    }

    uint32_t hash = buffer_peek_ints(in, DEFLATE_MIN_MATCH);
    hash = deflate_hash4(hash);

    int64_t best_size = 0;
    int64_t best_pos = -1;
    int64_t i = ht->head[hash];
    int64_t max_match_step = 4096;

    while(i != DEFLATE_NO_POS && max_match_step > 0) {
        if(i < start) {
            break;
        }

        if(best_size >= 32) {
            max_match_step >>= 2;
        }

        max_match_step--;

        int64_t j = 0;
        boolean_t not_matched = false;

        for(; j < best_size && j < max_match; j++) {
            if(buffer_peek_byte_at_position(in, i + j) != buffer_peek_byte_at_position(in, in_p + j)) {
                not_matched = true;

                break;
            }
        }

        if(not_matched) {
            i = ht->prev[i % DEFLATE_HASHTABLE_PREV_SIZE];

            continue;
        }

        for(; j < max_match; j++) {
            if(buffer_peek_byte_at_position(in, i + j) != buffer_peek_byte_at_position(in, in_p + j)) {
                break;
            }
        }

        if(j >= DEFLATE_MIN_MATCH && j > best_size) {
            best_size = j;
            best_pos = i;
        }

        if(j == max_match) {
            break;
        }

        int64_t prev_i = ht->prev[i % DEFLATE_HASHTABLE_PREV_SIZE];

        if(prev_i == i) {
            break;
        }

        i = prev_i;
    }

    deflate_hash_insert(in_p, hash, ht);

    return (deflate_match_t){.best_size = best_size, .best_pos = best_pos};
}

static buffer_t* deflate_deflate_lz77(buffer_t* in_block, huffman_encode_freq_t* freqs) {
    deflate_hashtable_t* ht = memory_malloc(sizeof(deflate_hashtable_t));

    if(ht == NULL) {
        return NULL;
    }

    memory_memset(ht->head, DEFLATE_NO_POS, sizeof(ht->head));

    buffer_t* out_block = buffer_new_with_capacity(NULL, buffer_get_length(in_block) * 2);

    if(out_block == NULL) {
        memory_free(ht);

        return NULL;
    }

    int64_t in_len = buffer_get_length(in_block);

    while(buffer_remaining(in_block)) {
        int64_t in_p = (int64_t)buffer_get_position(in_block);

        deflate_match_t dpm = deflate_find_bestmatch(in_block, in_len, in_p, ht);

        if(dpm.best_pos != DEFLATE_NO_POS && dpm.best_size >= DEFLATE_MIN_MATCH) {
            uint16_t marker = 0xFFFF;

            int64_t distance = in_p - dpm.best_pos;

            buffer_append_bytes(out_block, (uint8_t*)&marker, 2);
            buffer_append_bytes(out_block, (uint8_t*)&dpm.best_size, 2);
            buffer_append_bytes(out_block, (uint8_t*)&distance, 2);

            int16_t length_idx = huffman_find_length_index(dpm.best_size);
            int16_t dist_idx = huffman_find_distance_index(distance);

            if(length_idx == -1 || dist_idx == -1) {
                memory_free(ht);
                buffer_destroy(out_block);

                return NULL;
            }

            freqs->literal_freqs[257 + length_idx]++;

            freqs->distance_freqs[dist_idx]++;

            freqs->extra_bits_count += huffman_length_extra_bits[length_idx] + huffman_distance_extra_bits[dist_idx];

            if(!buffer_seek(in_block, dpm.best_size, BUFFER_SEEK_DIRECTION_CURRENT)) {
                memory_free(ht);
                buffer_destroy(out_block);

                return NULL;
            }
        } else {
            int16_t symbol = buffer_get_byte(in_block);

            freqs->literal_freqs[symbol]++;

            buffer_append_bytes(out_block, (uint8_t*)&symbol, 2); // deflate needs 16 bit symbols
        }
    }

    memory_free(ht);

    buffer_seek(in_block, 0, BUFFER_SEEK_DIRECTION_START);

    freqs->literal_freqs[256] = 1;

    return out_block;
}

static int8_t deflate_deflate_no_compress(buffer_t* in_block, bit_buffer_t* bit_buffer, boolean_t is_last_block) {
    int64_t in_len = buffer_get_length(in_block);

    if(in_len > DEFLATE_MAX_BLOCK_SIZE) {
        return -1;
    }

    int8_t ret = 0;

    if(is_last_block) {
        ret = bit_buffer_put(bit_buffer, 1, 1);
    } else {
        ret = bit_buffer_put(bit_buffer, 1, 0);
    }

    if(ret != 0) {
        return ret;
    }

    ret = bit_buffer_put(bit_buffer, 2, 0);

    if(ret != 0) {
        return ret;
    }

    ret = bit_buffer_push(bit_buffer);

    if(ret != 0) {
        return ret;
    }

    if(!buffer_append_bytes(bit_buffer->buffer, (uint8_t*)&in_len, 2)) {
        return -1;
    }

    in_len = ~in_len & 0xFFFF;

    if(!buffer_append_bytes(bit_buffer->buffer, (uint8_t*)&in_len, 2)) {
        return -1;
    }

    if(!buffer_append_buffer(bit_buffer->buffer, in_block)) {
        return -1;
    }

    return 0;
}

static int64_t deflate_deflate_calculate_out_size(huffman_encode_freq_t* freqs, const huffman_encode_table_t* symbols, const huffman_encode_table_t* distances) {
    int64_t out_size = 0;

    for(int64_t i = 0; i < 288; i++) {
        out_size += freqs->literal_freqs[i] * symbols->lengths[i];
    }

    for(int64_t i = 0; i < 30; i++) {
        out_size += freqs->distance_freqs[i] * distances->lengths[i];
    }

    out_size += freqs->extra_bits_count;

    return out_size;
}

static int8_t deflate_deflate_block(buffer_t* lz77_block, bit_buffer_t* bit_buffer, const huffman_encode_table_t* symbols, const huffman_encode_table_t* distances) {
    int64_t lz77_block_len = buffer_get_length(lz77_block);
    uint16_t* lz77_block_data = (uint16_t*)buffer_get_view_at_position(lz77_block, 0, lz77_block_len);

    int8_t ret = 0;

    for(uint32_t i = 0; i < lz77_block_len / 2; i++) {
        if(lz77_block_data[i] == 0xFFFF) {
            i++;
            // length + distance data
            uint16_t length = lz77_block_data[i++];

            int16_t length_idx = huffman_find_length_index(length);

            if(length_idx == -1 || length_idx > 28) {
                return -1;
            }

            uint16_t length_base = huffman_length_base[length_idx];
            uint16_t length_extra_bits = huffman_length_extra_bits[length_idx];
            length -= length_base;

            uint16_t length_code = 257 + length_idx;

            ret = bit_buffer_put(bit_buffer, symbols->lengths[length_code], symbols->codes[length_code]);

            if(ret != 0) {
                return ret;
            }

            ret = bit_buffer_put(bit_buffer, length_extra_bits, length);

            if(ret != 0) {
                return ret;
            }

            uint16_t distance = lz77_block_data[i];

            int16_t distance_idx = huffman_find_distance_index(distance);

            if(distance_idx == -1 || distance_idx > 29) {
                return -1;
            }

            uint16_t distance_base = huffman_distance_base[distance_idx];
            uint16_t distance_extra_bits = huffman_distance_extra_bits[distance_idx];

            distance -= distance_base;

            ret = bit_buffer_put(bit_buffer, distances->lengths[distance_idx], distances->codes[distance_idx]);

            if(ret != 0) {
                return ret;
            }

            ret = bit_buffer_put(bit_buffer, distance_extra_bits, distance);

            if(ret != 0) {
                return ret;
            }
        } else {
            // symbol data
            uint16_t symbol = lz77_block_data[i];

            uint16_t symbol_code = symbols->codes[symbol];
            uint8_t symbol_len = symbols->lengths[symbol];
            ret = bit_buffer_put(bit_buffer, symbol_len, symbol_code);

            if(ret != 0) {
                return ret;
            }

        }
    }

    ret = bit_buffer_put(bit_buffer, symbols->lengths[256], symbols->codes[256]); // end of block

    if(ret != 0) {
        return ret;
    }

    return 0;
}

typedef struct huffman_symbol_freq_t {
    uint16_t symbol;
    uint32_t freq;
} huffman_symbol_freq_t;

typedef struct huffman_bit_level_info_t {
    int32_t  level;
    uint32_t last_freq;
    uint32_t next_char_freq;
    uint32_t next_pair_freq;
    int32_t  needed;
} huffman_bit_level_info_t;

#define DEFLATE_MAX_BITS_LIMIT 16

int8_t huffman_sort_by_freq(const void * a, const void* b);
int8_t huffman_sort_by_symbol(const void * a, const void* b);

int8_t huffman_sort_by_freq(const void * a, const void* b) {
    const huffman_symbol_freq_t* hsf1 = a;
    const huffman_symbol_freq_t* hsf2 = b;

    if(hsf1->freq < hsf2->freq) {
        return -1;
    } else if(hsf1->freq > hsf2->freq) {
        return 1;
    }

    if(hsf1->symbol < hsf2->symbol) {
        return -1;
    } else if(hsf1->symbol > hsf2->symbol) {
        return 1;
    }

    return 0;
}

int8_t huffman_sort_by_symbol(const void * a, const void* b) {
    const huffman_symbol_freq_t* hsf1 = a;
    const huffman_symbol_freq_t* hsf2 = b;

    if(hsf1->symbol < hsf2->symbol) {
        return -1;
    } else if(hsf1->symbol > hsf2->symbol) {
        return 1;
    }

    return 0;
}

#define HUFFMAN_LEAF_COUNT(x, y) leaf_counts[(x) * DEFLATE_MAX_BITS_LIMIT + (y)]

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static boolean_t huffman_encode_build_table_internal(huffman_symbol_freq_t** symbol_freqs, int32_t count, int32_t max_bits, huffman_encode_table_t* huffman_table) {
    if(max_bits > count - 1) {
        max_bits = count - 1;
    }

    boolean_t res = false;

    int32_t old_count = count;
    uint8_t* bit_counts = NULL;


    huffman_bit_level_info_t * bit_level_infos = memory_malloc(sizeof(huffman_bit_level_info_t) * DEFLATE_MAX_BITS_LIMIT);

    if(!bit_level_infos) {
        return false;
    }

    int32_t* leaf_counts = memory_malloc(sizeof(int32_t) * DEFLATE_MAX_BITS_LIMIT * DEFLATE_MAX_BITS_LIMIT);

    if(!leaf_counts) {
        memory_free(bit_level_infos);

        return false;
    }

    for(int32_t i = 1; i <= max_bits; i++) {
        bit_level_infos[i].level = i;
        bit_level_infos[i].last_freq = symbol_freqs[1]->freq;
        bit_level_infos[i].next_char_freq = symbol_freqs[2]->freq;
        bit_level_infos[i].next_pair_freq = symbol_freqs[0]->freq + symbol_freqs[1]->freq;

        HUFFMAN_LEAF_COUNT(i, i) = 2;
    }

    bit_level_infos[1].next_pair_freq = 0xFFFFFFFF;

    symbol_freqs[count] = memory_malloc(sizeof(huffman_symbol_freq_t));

    if(!symbol_freqs[count]) {
        goto exit;
    }

    symbol_freqs[count]->symbol = 0xFFFF;
    symbol_freqs[count]->freq = 0xFFFFFFFF;

    bit_level_infos[max_bits].needed = 2 * count - 4;

    int32_t level = max_bits;

    while(true) {
        huffman_bit_level_info_t* bli = &bit_level_infos[level];

        if(bli->next_pair_freq == 0xFFFFFFFF && bli->next_char_freq == 0xFFFFFFFF) {
            bli->needed = 0;
            bit_level_infos[level + 1].next_pair_freq = 0xFFFFFFFF;
            level++;

            continue;
        }

        int32_t prev_freq = bli->last_freq;

        if(bli->next_char_freq < bli->next_pair_freq) {
            int32_t n = HUFFMAN_LEAF_COUNT(level, level) + 1;

            bli->last_freq = bli->next_char_freq;

            HUFFMAN_LEAF_COUNT(level, level) = n;

            bli->next_char_freq = symbol_freqs[n]->freq;
        } else {
            bli->last_freq = bli->next_pair_freq;

            for(int32_t i = 0; i < level; i++) {
                HUFFMAN_LEAF_COUNT(level, i) = HUFFMAN_LEAF_COUNT(level - 1, i);
            }

            bit_level_infos[bli->level - 1].needed = 2;
        }

        bli->needed--;

        if(bli->needed == 0) {
            if(bli->level == max_bits) {
                break;
            }

            bit_level_infos[bli->level + 1].next_pair_freq = prev_freq + bli->last_freq;
            level++;
        } else {
            while(bit_level_infos[level - 1].needed > 0) {
                level--;
            }
        }
    }

    if(HUFFMAN_LEAF_COUNT(max_bits, max_bits) != count) {
        goto exit;
    }

    bit_counts = memory_malloc(sizeof(uint8_t) * (max_bits + 1));

    if(!bit_counts) {
        goto exit;
    }

    uint8_t bits = 1;

    for(level = max_bits; level > 0; level--) {
        bit_counts[bits] = HUFFMAN_LEAF_COUNT(max_bits, level) - HUFFMAN_LEAF_COUNT(max_bits, level - 1);
        bits++;
    }

    uint16_t code = 0;

    for(level = 0; level <= max_bits; level++) {
        code <<= 1;

        if(level == 0 || bit_counts[level] == 0) {
            continue;
        }

        uint64_t start = count - bit_counts[level];
        uint64_t end = count - 1;

        quicksort2_partial((void**)symbol_freqs, start, end, huffman_sort_by_symbol);

        for(uint64_t i = start; i <= end; i++) {
            huffman_table->codes[symbol_freqs[i]->symbol] = reverse_bits(code, level);
            huffman_table->lengths[symbol_freqs[i]->symbol] = level;

            code++;
        }

        count -= bit_counts[level];
    }

    res = true;

exit:
    memory_free(bit_counts);
    memory_free(leaf_counts);
    memory_free(symbol_freqs[old_count]);
    memory_free(bit_level_infos);

    return res;
}

static boolean_t huffman_encode_build_table(huffman_encode_freq_t* freqs, boolean_t for_literal, huffman_encode_table_t* huffman_table, int32_t max_bits, int32_t* max_used_symbol) {
    if(!freqs || !huffman_table || max_bits < 1 || max_bits > 15 || !max_used_symbol) {
        return false;
    }

    boolean_t res = false;

    int32_t symbol_freqs_count = 0;

    huffman_symbol_freq_t** symbol_freqs = NULL;

    int32_t end_of = for_literal?288:30;

    symbol_freqs = memory_malloc(sizeof(huffman_symbol_freq_t*) * (end_of + 1));

    if(!symbol_freqs) {
        return NULL;
    }

    for(int32_t i = 0; i < end_of; i++) {
        uint32_t freq = for_literal?freqs->literal_freqs[i]:freqs->distance_freqs[i];

        if(freq > 0) {
            symbol_freqs[symbol_freqs_count] = memory_malloc(sizeof(huffman_symbol_freq_t));

            if(!symbol_freqs[symbol_freqs_count]) {
                goto exit;
            }

            symbol_freqs[symbol_freqs_count]->symbol = i;
            symbol_freqs[symbol_freqs_count]->freq = freq;
            symbol_freqs_count++;

            *max_used_symbol = i;
        }
    }

    if(symbol_freqs_count <= 2) {
        for(int32_t i = 0; i < symbol_freqs_count; i++) {
            huffman_table->codes[symbol_freqs[i]->symbol] = i;
            huffman_table->lengths[symbol_freqs[i]->symbol] = 1;
        }
    } else {
        quicksort2((void**)symbol_freqs, symbol_freqs_count, huffman_sort_by_freq);

        if(!huffman_encode_build_table_internal(symbol_freqs, symbol_freqs_count, max_bits, huffman_table)) {
            goto exit;
        }
    }

    res = true;
exit:
    for(int32_t j = 0; j < symbol_freqs_count; j++) {
        memory_free(symbol_freqs[j]);
    }

    memory_free(symbol_freqs);

    return res;
}
#pragma GCC diagnostic pop

/**
 * @brief Builds a huffman tree from the given frequencies and also returns the header for the tree
 * @param[in] freqs The frequencies to build the tree from
 * @param[out] symbols The huffman tree
 * @param[out] distances The huffman tree
 * @return The header for the tree
 */
static bit_buffer_t* huffman_encode_build_tables_and_code(huffman_encode_freq_t* freqs, huffman_encode_table_t** symbols, huffman_encode_table_t** distances, int64_t* header_bit_count) {
    if(!freqs || !symbols || !distances || !header_bit_count) {
        return NULL;
    }

    bit_buffer_t* header = NULL;
    uint8_t* code_lengths = NULL;

    int32_t max_used_symbol = 0;
    int32_t max_used_distance = 0;
    int32_t max_used_code_length = 0;

    *symbols = memory_malloc(sizeof(huffman_encode_table_t));

    if(!*symbols) {
        goto exit;
    }

    *distances = memory_malloc(sizeof(huffman_encode_table_t));

    if(!*distances) {
        goto exit;
    }


    if(!huffman_encode_build_table(freqs, true, *symbols, 15, &max_used_symbol)) {
        goto exit;
    }

    if(!huffman_encode_build_table(freqs, false, *distances, 15, &max_used_distance)) {
        goto exit;
    }


    int32_t code_count = max_used_symbol + 1 + max_used_distance + 1 + 1;

    code_lengths = memory_malloc(sizeof(uint8_t) * code_count);

    if(!code_lengths) {
        goto exit;
    }

    huffman_encode_freq_t code_lengths_freqs;
    memory_memclean(&code_lengths_freqs, sizeof(huffman_encode_freq_t));

    for(int32_t i = 0; i <= max_used_symbol; i++) {
        code_lengths[i] = (*symbols)->lengths[i];
    }

    for(int32_t i = 0; i <= max_used_distance; i++) {
        code_lengths[i + max_used_symbol + 1] = (*distances)->lengths[i];
    }

    code_lengths[code_count - 1] = 255;

    int32_t size = code_lengths[0];
    int32_t count = 1;
    int32_t out_idx = 0;

    int32_t tmp_out_len_bits = 0;

    for(int32_t in_idx = 1; size != 255; in_idx++) {
        int32_t next_size = code_lengths[in_idx];

        if(next_size == size) {
            count++;

            continue;
        }

        if(size != 0) {
            code_lengths[out_idx] = size;
            out_idx++;

            tmp_out_len_bits += 7;

            code_lengths_freqs.literal_freqs[size]++;

            count--;

            while(count >= 3) {
                int32_t n = 6;

                if(n > count) {
                    n = count;
                }

                code_lengths[out_idx] = 16;
                out_idx++;

                code_lengths[out_idx] = n - 3;
                out_idx++;

                tmp_out_len_bits += 9;

                code_lengths_freqs.literal_freqs[16]++;

                count -= n;
            }

        } else {
            while(count >= 11) {
                int32_t n = 138;

                if(n > count) {
                    n = count;
                }

                code_lengths[out_idx] = 18;
                out_idx++;

                code_lengths[out_idx] = n - 11;
                out_idx++;

                tmp_out_len_bits += 18;

                code_lengths_freqs.literal_freqs[18]++;

                count -= n;
            }

            if(count >= 3) {
                code_lengths[out_idx] = 17;
                out_idx++;

                code_lengths[out_idx] = count - 3;
                out_idx++;

                tmp_out_len_bits += 9;

                code_lengths_freqs.literal_freqs[17]++;

                count = 0;
            }

        }

        count--;

        while(count >= 0) {
            code_lengths[out_idx] = size;
            out_idx++;

            tmp_out_len_bits += 7;

            code_lengths_freqs.literal_freqs[size]++;
            count--;
        }

        size = next_size;
        count = 1;
    }

    tmp_out_len_bits += 14 + 32;

    code_lengths[out_idx] = 255;

    huffman_encode_table_t code_lengths_table;
    memory_memclean(&code_lengths_table, sizeof(huffman_encode_table_t));

    if(!huffman_encode_build_table(&code_lengths_freqs, true, &code_lengths_table, 7, &max_used_code_length)) {
        goto exit;
    }

    int32_t bit_count = 0;

    buffer_t* header_buffer = buffer_new_with_capacity(NULL, tmp_out_len_bits / 8 + 1);

    if(!header_buffer) {
        goto exit;
    }

    header = memory_malloc(sizeof(bit_buffer_t));

    if(!header) {
        memory_free(header_buffer);

        goto exit;
    }

    header->buffer = header_buffer;

    max_used_code_length = 18;

    int8_t ret = 0;

    ret = bit_buffer_put(header, 5, max_used_symbol + 1 - 257);

    if(ret < 0) {
        goto exit_error;
    }

    ret = bit_buffer_put(header, 5, max_used_distance + 1 - 1);

    if(ret < 0) {
        goto exit_error;
    }

    ret = bit_buffer_put(header, 4, max_used_code_length + 1 - 4);

    if(ret < 0) {
        goto exit_error;
    }

    bit_count += 5 + 5 + 4;

    for(int32_t i = 0; i <= max_used_code_length; i++) {
        ret = bit_buffer_put(header, 3, code_lengths_table.lengths[huffman_code_lengths[i]]);

        if(ret < 0) {
            goto exit_error;
        }

        bit_count += 3;
    }

    int32_t i = 0;

    while(true) {
        int32_t code_length = code_lengths[i];

        i++;

        if(code_length == 255) {
            break;
        }

        ret = bit_buffer_put(header, code_lengths_table.lengths[code_length], code_lengths_table.codes[code_length]);

        if(ret < 0) {
            goto exit_error;
        }

        bit_count += code_lengths_table.lengths[code_length];

        switch(code_length) {
        case 16:
            ret = bit_buffer_put(header, 2, code_lengths[i]);

            if(ret < 0) {
                goto exit_error;
            }

            bit_count += 2;
            i++;
            break;

        case 17:
            ret = bit_buffer_put(header, 3, code_lengths[i]);

            if(ret < 0) {
                goto exit_error;
            }

            bit_count += 3;
            i++;
            break;

        case 18:
            ret = bit_buffer_put(header, 7, code_lengths[i]);

            if(ret < 0) {
                goto exit_error;
            }

            bit_count += 7;
            i++;
            break;
        }
    }

    ret = bit_buffer_push(header);

    if(ret < 0) {
        goto exit_error;
    }

    buffer_seek(header_buffer, 0, BUFFER_SEEK_DIRECTION_START);

    *header_bit_count = bit_count;

exit:
    memory_free(code_lengths);

    return header;

exit_error:
    buffer_destroy(header_buffer);
    memory_free(code_lengths);
    memory_free(header);

    return NULL;
}


int8_t deflate_deflate (buffer_t * in, buffer_t* out) {
    bit_buffer_t bit_buffer = {
        .buffer = out,
        .byte = 0,
        .bit_count = 0
    };

    while(buffer_remaining(in)) {
        int64_t in_rem = buffer_remaining(in);
        int64_t in_pos = buffer_get_position(in);

        int64_t block_len = MIN(in_rem, DEFLATE_MAX_BLOCK_SIZE);

        uint8_t* block = buffer_get_view_at_position(in, in_pos, block_len);

        if(!buffer_seek(in, block_len, BUFFER_SEEK_DIRECTION_CURRENT)) {
            return -1;
        }

        boolean_t is_last_block = buffer_remaining(in) == 0;

        buffer_t* in_block = buffer_encapsulate(block, block_len);

        if(!in_block) {
            return -1;
        }

        huffman_encode_freq_t* freqs = memory_malloc(sizeof(huffman_encode_freq_t));

        if(!freqs) {
            buffer_destroy(in_block);

            return -1;
        }

        buffer_t* lz77_block = deflate_deflate_lz77(in_block, freqs);

        if(!lz77_block) {
            buffer_destroy(in_block);
            memory_free(freqs);

            return -1;
        }

        huffman_encode_table_t* dyn_symbols = NULL;
        huffman_encode_table_t* dyn_distances = NULL;
        int64_t dyn_header_len = 0;

        bit_buffer_t* dyn_header = huffman_encode_build_tables_and_code(freqs, &dyn_symbols, &dyn_distances, &dyn_header_len);

        if(!dyn_header || !dyn_symbols || !dyn_distances) {
            if(dyn_header) {
                buffer_destroy(dyn_header->buffer);
            }

            memory_free(dyn_header);
            memory_free(dyn_symbols);
            memory_free(dyn_distances);
            buffer_destroy(in_block);
            buffer_destroy(lz77_block);
            memory_free(freqs);

            return -1;
        }


        uint64_t nocompress_len = 0;

        if(bit_buffer.bit_count <= 5) {
            nocompress_len = 8 - bit_buffer.bit_count;
        } else {
            nocompress_len = 16 - bit_buffer.bit_count;
        }

        nocompress_len += 16 + 16 + buffer_get_length(in_block) * 8;

        uint64_t fixedcompress_len = deflate_deflate_calculate_out_size(freqs, &huffman_encode_fixed, &huffman_encode_distance_fixed);
        uint64_t dyncompress_len = deflate_deflate_calculate_out_size(freqs, dyn_symbols, dyn_distances) + dyn_header_len;

        int8_t ret = 0;

        if(nocompress_len < fixedcompress_len) {
            ret = deflate_deflate_no_compress(in_block, &bit_buffer, is_last_block);
        } else if(fixedcompress_len < dyncompress_len) {
            if(is_last_block) {
                ret = bit_buffer_put(&bit_buffer, 1, 1);
            } else {
                ret = bit_buffer_put(&bit_buffer, 1, 0);
            }

            if(ret < 0) {
                goto end_block_op;
            }

            ret = bit_buffer_put(&bit_buffer, 2, 1);

            if(ret < 0) {
                goto end_block_op;
            }

            ret = deflate_deflate_block(lz77_block, &bit_buffer, &huffman_encode_fixed, &huffman_encode_distance_fixed);
        } else {
            if(is_last_block) {
                ret = bit_buffer_put(&bit_buffer, 1, 1);
            } else {
                ret = bit_buffer_put(&bit_buffer, 1, 0);
            }

            if(ret < 0) {
                goto end_block_op;
            }

            ret = bit_buffer_put(&bit_buffer, 2, 2);

            if(ret < 0) {
                goto end_block_op;
            }

            for(int32_t i = 0; i < dyn_header_len; i++) {
                int8_t dyn_header_bit = bit_buffer_get(dyn_header, 1);
                ret = bit_buffer_put(&bit_buffer, 1, dyn_header_bit);

                if(ret < 0) {
                    goto end_block_op;
                }
            }

            ret = deflate_deflate_block(lz77_block, &bit_buffer, dyn_symbols, dyn_distances);
        }

        if(is_last_block) {
            bit_buffer_push(&bit_buffer);
        }

end_block_op:
        memory_free(freqs);
        buffer_destroy(in_block);
        buffer_destroy(lz77_block);
        buffer_destroy(dyn_header->buffer);
        memory_free(dyn_header);
        memory_free(dyn_symbols);
        memory_free(dyn_distances);

        if(ret != 0) {
            PRINTLOG(COMPRESSION, LOG_ERROR, "Failed to deflate block");

            return ret;
        }
    }

    return 0;
}

int8_t deflate_inflate(buffer_t* in, buffer_t* out) {
    bit_buffer_t bit_buffer = {
        .buffer = in,
        .byte = 0,
        .bit_count = 0
    };

    int8_t ret = 0;
    int64_t bit;

    while(true) {
        bit = bit_buffer_get(&bit_buffer, 1);

        if(bit == -1) {
            return -1;
        }

        boolean_t last = bit == 1;

        bit = bit_buffer_get(&bit_buffer, 2);

        if(bit == -1) {
            PRINTLOG(COMPRESSION, LOG_ERROR, "Failed to read block type");

            return -1;
        }

        uint8_t type = bit;

        huffman_decode_table_t lengths;
        memory_memcopy(&huffman_decode_fixed_lengths, &lengths, sizeof(huffman_decode_table_t));
        huffman_decode_table_t distances;
        memory_memcopy(&huffman_decode_fixed_distances, &distances, sizeof(huffman_decode_table_t));

        switch(type) {
        case 0:
            ret = deflate_inflate_uncompressed_block(&bit_buffer, out);

            if(ret != 0) {
                PRINTLOG(COMPRESSION, LOG_ERROR, "Failed to decode uncompressed block");
            }

            break;
        case 2:
            if(huffman_decode_table_decode(&bit_buffer, &lengths, &distances) != 0) {
                PRINTLOG(COMPRESSION, LOG_ERROR, "Failed to decode huffman table");

                return -1;
            }

            nobreak;
        case 1:
            ret = deflate_inflate_block(&bit_buffer, out, &lengths, &distances);

            if(ret != 0) {
                PRINTLOG(COMPRESSION, LOG_ERROR, "Failed to decode block");
            }

            break;
        case 3:
            PRINTLOG(COMPRESSION, LOG_ERROR, "Reserved block type");

            return -1;
        }

        if (ret != 0) {
            return ret;
        }

        if (last) {
            break;
        }
    }

    return 0;
}


