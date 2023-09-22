/**
 * @file deflate.64.c
 * @brief deflate implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <deflate.h>
#include <utils.h>

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
    uint64_t  total_bits;
} bit_buffer_t;

typedef struct huffman_decode_table_t {
    uint16_t counts[16];
    uint16_t symbols[288];
} huffman_decode_table_t;

typedef struct huffman_encode_table_t {
    uint16_t codes[288];
    uint8_t  lengths[288];
} huffman_encode_table_t;

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
void   huffman_decode_table_decode(bit_buffer_t* bit_buffer, huffman_decode_table_t* huffman_lengths, huffman_decode_table_t* huffman_distances);

static inline int64_t bit_buffer_get(bit_buffer_t* bit_buffer, uint8_t bit_count) {
    int64_t result = 0;

    for (uint8_t i = 0; i < bit_count; i++) {
        if(bit_buffer->bit_count == 0) {
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

static inline void bit_buffer_put(bit_buffer_t* bit_buffer, uint8_t bit_count, uint64_t bits) {
    for (uint8_t i = 0; i < bit_count; i++) {
        if(bit_buffer->bit_count == 8) {
            buffer_append_byte(bit_buffer->buffer, bit_buffer->byte);
            bit_buffer->bit_count = 0;
            bit_buffer->byte = 0;
        }

        bit_buffer->byte |= (bits & 1) << bit_buffer->bit_count;
        bits >>= 1;
        bit_buffer->bit_count++;
    }
}

static inline void bit_buffer_push(bit_buffer_t* bit_buffer) {
    if(bit_buffer->bit_count != 0) {
        buffer_append_byte(bit_buffer->buffer, bit_buffer->byte);
        bit_buffer->bit_count = 0;
        bit_buffer->byte = 0;
    }
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

static int32_t huffman_decode(bit_buffer_t* bit_buffer, huffman_decode_table_t* huffman_decode_table) {
    int32_t count = 0, cur = 0;

    for (int32_t i = 1; cur >= 0; i++) {
        cur = (cur << 1) | bit_buffer_get(bit_buffer, 1);
        count += huffman_decode_table->counts[i];
        cur -= huffman_decode_table->counts[i];
    }

    return huffman_decode_table->symbols[count + cur];
}

int8_t deflate_inflate_block(bit_buffer_t* bit_buffer, buffer_t* out, huffman_decode_table_t* lengths, huffman_decode_table_t* distances) {
    while(true) {
        int32_t symbol = huffman_decode(bit_buffer, lengths);

        if(symbol < 256) {
            buffer_append_byte(out, symbol);
        } else if(symbol == 256) {
            break;
        } else {
            symbol -= 257;

            int32_t length = bit_buffer_get(bit_buffer, huffman_length_extra_bits[symbol]) + huffman_length_base[symbol];

            int32_t distance_symbol = huffman_decode(bit_buffer, distances);

            int32_t distance = bit_buffer_get(bit_buffer, huffman_distance_extra_bits[distance_symbol]) + huffman_distance_base[distance_symbol];

            uint64_t current_position = buffer_get_position(out);
            current_position -= distance;

            for (int32_t i = 0; i < length; i++) {
                buffer_append_byte(out, buffer_peek_byte_at_position(out, current_position++));
            }

        }
    }

    return 0;
}

static inline void huffman_decode_table_build(uint8_t* lengths, size_t size, struct huffman_decode_table_t* out) {

    uint16_t offsets[16];
    unsigned int count = 0;

    for(uint32_t i = 0; i < 16; i++) {
        out->counts[i] = 0;
    }

    for(uint32_t i = 0; i < size; ++i) {
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
}


void huffman_decode_table_decode(bit_buffer_t* bit_buffer, huffman_decode_table_t* huffman_lengths, huffman_decode_table_t* huffman_distances) {
    memory_memclean(huffman_lengths, sizeof(huffman_decode_table_t));
    memory_memclean(huffman_distances, sizeof(huffman_decode_table_t));

    uint8_t lengths[320] = {0};

    uint32_t literals  = 257 + bit_buffer_get(bit_buffer, 5);
    uint32_t distances = 1 + bit_buffer_get(bit_buffer, 5);
    uint32_t clengths  = 4 + bit_buffer_get(bit_buffer, 4);

    for (unsigned int i = 0; i < clengths; ++i) {
        lengths[huffman_code_lengths[i]] = bit_buffer_get(bit_buffer, 3);
    }

    huffman_decode_table_t codes;
    huffman_decode_table_build(lengths, 19, &codes);

    uint32_t count = 0;
    while (count < literals + distances) {
        int symbol = huffman_decode(bit_buffer, &codes);

        if (symbol < 16) {
            lengths[count++] = symbol;
        } else if (symbol < 19) {
            int rep = 0, length;
            if (symbol == 16) {
                rep = lengths[count - 1];
                length = bit_buffer_get(bit_buffer, 2) + 3;
            } else if (symbol == 17) {
                length = bit_buffer_get(bit_buffer, 3) + 3;
            } else if (symbol == 18) {
                length = bit_buffer_get(bit_buffer, 7) + 11;
            }
            do {
                lengths[count++] = rep;
                length--;
            } while (length);
        } else {
            break;
        }
    }

    huffman_decode_table_build(lengths, literals, huffman_lengths);
    huffman_decode_table_build(lengths + literals, distances, huffman_distances);
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

static buffer_t* deflate_deflate_lz77(buffer_t* in_block) {
    deflate_hashtable_t* ht = memory_malloc(sizeof(deflate_hashtable_t));

    if(ht == NULL) {
        return NULL;
    }

    memory_memset(ht->head, DEFLATE_NO_POS, sizeof(ht->head));

    buffer_t* out_block = buffer_new();

    if(out_block == NULL) {
        memory_free(ht);

        return NULL;
    }

    int64_t in_len = buffer_get_length(in_block);

    while(buffer_remaining(in_block)) {
        int64_t in_p = (int64_t)buffer_get_position(in_block);

        deflate_match_t dpm = deflate_find_bestmatch(in_block, in_len, in_p, ht);

        if(dpm.best_pos != DEFLATE_NO_POS && dpm.best_size >= DEFLATE_MIN_MATCH) {
            int16_t marker = -1;

            int64_t distance = in_p - dpm.best_pos;

            buffer_append_bytes(out_block, (uint8_t*)&marker, 2);
            buffer_append_bytes(out_block, (uint8_t*)&dpm.best_size, 2);
            buffer_append_bytes(out_block, (uint8_t*)&distance, 2);

            if(!buffer_seek(in_block, dpm.best_size, BUFFER_SEEK_DIRECTION_CURRENT)) {
                memory_free(ht);
                buffer_destroy(out_block);

                return NULL;
            }
        } else {
            int16_t symbol = buffer_get_byte(in_block);
            buffer_append_bytes(out_block, (uint8_t*)&symbol, 2); // deflate needs 16 bit symbols
        }
    }

    memory_free(ht);

    buffer_seek(in_block, 0, BUFFER_SEEK_DIRECTION_START);

    return out_block;
}

static buffer_t* deflate_deflate_no_compress(buffer_t* in_block) {
    int64_t in_len = buffer_get_length(in_block);

    if(in_len > DEFLATE_MAX_BLOCK_SIZE) {
        return NULL;
    }

    buffer_t* out_block = buffer_new();

    if(out_block == NULL) {
        return NULL;
    }

    buffer_append_bytes(out_block, (uint8_t*)&in_len, 2);

    in_len = ~in_len & 0xFFFF;

    buffer_append_bytes(out_block, (uint8_t*)&in_len, 2);

    buffer_append_buffer(out_block, in_block);

    buffer_seek(out_block, 0, BUFFER_SEEK_DIRECTION_START);
    buffer_seek(in_block, 0, BUFFER_SEEK_DIRECTION_START);

    return out_block;
}

static bit_buffer_t* deflate_deflate_block(buffer_t* lz77_block, const huffman_encode_table_t* symbols, const huffman_encode_table_t* distances) {
    buffer_t* out_block = buffer_new();

    if(out_block == NULL) {
        return NULL;
    }

    bit_buffer_t* bit_buffer = memory_malloc(sizeof(bit_buffer_t));

    if(bit_buffer == NULL) {
        buffer_destroy(out_block);

        return NULL;
    }

    bit_buffer->buffer = out_block;

    bit_buffer_put(bit_buffer, 3, 0); // algin to byte for remainder of block

    int64_t lz77_block_len = buffer_get_length(lz77_block);
    uint16_t* lz77_block_data = (uint16_t*)buffer_get_view_at_position(lz77_block, 0, lz77_block_len);

    for(uint32_t i = 0; i < lz77_block_len / 2; i++) {
        if(lz77_block_data[i] == 0xFFFF) {
            i++;
            // length + distance data
            uint16_t length = lz77_block_data[i++];

            uint16_t length_idx = huffman_find_length_index(length);

            if(length_idx == 0xFFFF) {
                memory_free(bit_buffer);
                buffer_destroy(out_block);

                return NULL;
            }

            uint16_t length_base = huffman_length_base[length_idx];
            uint16_t length_extra_bits = huffman_length_extra_bits[length_idx];
            length -= length_base;

            uint16_t length_code = 257 + length_idx;

            bit_buffer_put(bit_buffer, symbols->lengths[length_code], symbols->codes[length_code]);
            bit_buffer_put(bit_buffer, length_extra_bits, length);

            uint16_t distance = lz77_block_data[i];

            uint16_t distance_idx = huffman_find_distance_index(distance);

            if(distance_idx == 0xFFFF) {
                memory_free(bit_buffer);
                buffer_destroy(out_block);

                return NULL;
            }

            uint16_t distance_base = huffman_distance_base[distance_idx];
            uint16_t distance_extra_bits = huffman_distance_extra_bits[distance_idx];

            distance -= distance_base;

            bit_buffer_put(bit_buffer, distances->lengths[distance_idx], distances->codes[distance_idx]);
            bit_buffer_put(bit_buffer, distance_extra_bits, distance);

        } else {
            // symbol data
            uint16_t symbol = lz77_block_data[i];

            uint16_t symbol_code = symbols->codes[symbol];
            uint8_t symbol_len = symbols->lengths[symbol];

            bit_buffer_put(bit_buffer, symbol_len, symbol_code);

        }
    }

    bit_buffer_put(bit_buffer, symbols->lengths[256], symbols->codes[256]); // end of block

    bit_buffer->total_bits = buffer_get_length(out_block) * 8 + bit_buffer->bit_count;

    bit_buffer_push(bit_buffer);

    buffer_seek(out_block, 0, BUFFER_SEEK_DIRECTION_START);

    return bit_buffer;
}

int8_t deflate_deflate(buffer_t* in, buffer_t* out) {
    bit_buffer_t bit_buffer = {
        .buffer = out,
        .byte = 0,
        .bit_count = 0
    };


    while(buffer_remaining(in)) {
        int64_t in_len = buffer_remaining(in);
        int64_t in_pos = buffer_get_position(in);

        int64_t block_len = MIN(in_len, DEFLATE_MAX_BLOCK_SIZE);

        uint8_t* block = buffer_get_view_at_position(in, in_pos, block_len);

        buffer_seek(in, in_pos + block_len, BUFFER_SEEK_DIRECTION_CURRENT);

        buffer_t* in_block = buffer_encapsulate(block, block_len);

        buffer_t* lz77_block = deflate_deflate_lz77(in_block);

        buffer_t* no_compress_block = deflate_deflate_no_compress(in_block);

        if(!no_compress_block) {
            buffer_destroy(in_block);
            buffer_destroy(lz77_block);

            return -1;
        }

        bit_buffer_t* fixed_compressed_block = deflate_deflate_block(lz77_block, &huffman_encode_fixed, &huffman_encode_distance_fixed);

        if(!fixed_compressed_block) {
            buffer_destroy(in_block);
            buffer_destroy(lz77_block);
            buffer_destroy(no_compress_block);

            return -1;
        }


        // dynamic huffman


        buffer_destroy(in_block);
        buffer_destroy(lz77_block);

        uint64_t no_compress_len = buffer_get_length(no_compress_block) * 8;


        boolean_t use_no_compress = no_compress_len < fixed_compressed_block->total_bits;
        boolean_t use_fixed = fixed_compressed_block->total_bits < no_compress_len;

        if(!use_no_compress) {
            buffer_destroy(no_compress_block);
        }

        if(!use_fixed) {
            buffer_destroy(fixed_compressed_block->buffer);
            memory_free(fixed_compressed_block);
        }


        if(use_no_compress) {
            if(!buffer_remaining(in)) {
                bit_buffer_put(&bit_buffer, 1, 1);
                bit_buffer_put(&bit_buffer, 2, 0);
                bit_buffer_push(&bit_buffer);
                buffer_append_buffer(out, no_compress_block);
                buffer_destroy(no_compress_block);
            } else {
                bit_buffer_put(&bit_buffer, 1, 0);
                bit_buffer_put(&bit_buffer, 2, 0);
                bit_buffer_push(&bit_buffer);
                buffer_append_buffer(out, no_compress_block);
                buffer_destroy(no_compress_block);
            }
        }

        if(use_fixed) {
            bit_buffer_get(fixed_compressed_block, 3);

            if(!buffer_remaining(in)) {
                bit_buffer_put(&bit_buffer, 1, 1);
            } else {
                bit_buffer_put(&bit_buffer, 1, 0);
            }

            bit_buffer_put(&bit_buffer, 2, 1);

            int64_t bits = bit_buffer_get(fixed_compressed_block, 5);

            bit_buffer_put(&bit_buffer, 5, bits);

            bit_buffer_push(&bit_buffer);

            if(fixed_compressed_block->bit_count) {
                buffer_destroy(fixed_compressed_block->buffer);
                memory_free(fixed_compressed_block);

                return -1;
            }

            fixed_compressed_block->total_bits -= 8;

            uint64_t rem_bytes = fixed_compressed_block->total_bits / 8;
            uint64_t rem_bits = fixed_compressed_block->total_bits % 8;

            uint8_t* rem_data = buffer_get_view_at_position(fixed_compressed_block->buffer, 1, rem_bytes);

            if(!rem_data) {
                buffer_destroy(fixed_compressed_block->buffer);
                memory_free(fixed_compressed_block);

                return -1;
            }

            buffer_append_bytes(out, rem_data, rem_bytes);

            if(rem_bits) {
                buffer_seek(fixed_compressed_block->buffer, rem_bytes, BUFFER_SEEK_DIRECTION_CURRENT);
                uint8_t rem_byte = buffer_get_byte(fixed_compressed_block->buffer);

                bit_buffer_put(&bit_buffer, rem_bits, rem_byte);
            }

            buffer_destroy(fixed_compressed_block->buffer);
            memory_free(fixed_compressed_block);

            if(!buffer_remaining(in)) {
                bit_buffer_push(&bit_buffer);
            }
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

    while(true) {
        boolean_t last = bit_buffer_get(&bit_buffer, 1);

        uint8_t type = bit_buffer_get(&bit_buffer, 2);

        huffman_decode_table_t lengths = {0};
        memory_memcopy(&huffman_decode_fixed_lengths, &lengths, sizeof(huffman_decode_table_t));
        huffman_decode_table_t distances = {0};
        memory_memcopy(&huffman_decode_fixed_distances, &distances, sizeof(huffman_decode_table_t));

        switch(type) {
        case 0:
            ret = deflate_inflate_uncompressed_block(&bit_buffer, out);
            break;
        case 2:
            huffman_decode_table_decode(&bit_buffer, &lengths, &distances);
            nobreak;
        case 1:
            ret = deflate_inflate_block(&bit_buffer, out, &lengths, &distances);
            break;
        case 3:
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


