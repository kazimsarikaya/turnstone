/**
 * @file image_png.64.c
 * @brief PNG image converter methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <graphics/png.h>
#include <errno.h>
#include <buffer.h>
#include <compression.h>
#include <logging.h>
#include <utils.h>
#include <crc.h>

MODULE("turnstone.kernel.graphics.image");

#define PNG_SIGNATURE 0x0A1A0A0D474E5089ULL

typedef enum png_error_types_t {
    PNG_SUCCESS,
    PNG_ERROR_UNKNOWN,
    PNG_DECODER_INVALID_CHUNK_LENGTH,
    PNG_DECODER_INVALID_COLOR_TYPE,
    PNG_DECODER_INVALID_FILTER_METHOD,
    PNG_DECODER_INVALID_COMPRESSION_METHOD,
    PNG_DECODER_INVALID_INTERLACE_METHOD,
    PNG_DECODER_IEND_ALREADY_PARSED,
    PNG_DECODER_IEND_NOT_FOUND,
    PNG_DECODER_UNKNOWN_CHUNK_TYPE,
    PNG_DECODER_MULTIPLE_CHUNK_TYPE_NOT_ALLOWED,
    PNG_DECODER_INCORRECT_CHUNK_ORDER,
    PNG_DECODER_CRC_MISMATCH,
    PNG_DECODER_UNSUPPORTED_COLOR_TYPE,
    PNG_DECODER_INVALID_FILTER_TYPE,
    PNG_DECODER_UNCOMPRESS_SIZE_MISMATCH,
    PNG_DECODER_INVALID_ZLIB_HEADER,
    PNG_DECODER_ADLER32_MISMATCH,
    PNG_DECODER_MEMORY_ERROR,
    PNG_DECODER_INVALID_PNG_HEADER,
    PNG_DECODER_SIZE_MISMATCH,

} png_error_types_t;

typedef struct png_decoder_t png_decoder_t;

typedef enum png_chunk_type_t {
    PNG_CHUNK_TYPE_IHDR,

    PNG_CHUNK_TYPE_cHRM,
    PNG_CHUNK_TYPE_gAMA,
    PNG_CHUNK_TYPE_iCCP,
    PNG_CHUNK_TYPE_sBIT,
    PNG_CHUNK_TYPE_sRGB,

    PNG_CHUNK_TYPE_PLTE,

    PNG_CHUNK_TYPE_bKGD,
    PNG_CHUNK_TYPE_hIST,
    PNG_CHUNK_TYPE_tRNS,

    PNG_CHUNK_TYPE_pHYs,
    PNG_CHUNK_TYPE_sPLT,

    PNG_CHUNK_TYPE_IDAT,
    PNG_CHUNK_TYPE_tIME,

    PNG_CHUNK_TYPE_iTXt,
    PNG_CHUNK_TYPE_tEXt,
    PNG_CHUNK_TYPE_zTXt,
    PNG_CHUNK_TYPE_IEND,

    PNG_CHUNK_TYPE_MAX,
} png_chunk_type_t;

typedef struct png_decoder_t {
    buffer_t*      buffer;
    buffer_t*      compressed_image_buffer;
    buffer_t*      image_buffer;
    compression_t* compression;
    int32_t        chunk_counts[PNG_CHUNK_TYPE_MAX];
    boolean_t      should_plte_exist;
    uint32_t       width;
    uint32_t       height;
    uint8_t        bit_depth;
    uint8_t        color_type;
    uint8_t        compression_method;
    uint8_t        filter_method;
    uint8_t        interlace_method;
    uint64_t       total_idat_length;
} png_decoder_t;

const uint32_t png_chunk_type_strings[] = {
    [PNG_CHUNK_TYPE_IHDR] = 0x52444849, ///< Image Header IHDR
    [PNG_CHUNK_TYPE_PLTE] = 0x45544C50, ///< Palette PLTE
    [PNG_CHUNK_TYPE_IDAT] = 0x54414449, ///< Image Data IDAT
    [PNG_CHUNK_TYPE_IEND] = 0x444E4549, ///< Image End IEND
                                        ///
    [PNG_CHUNK_TYPE_cHRM] = 0x4D524863, ///< Chromaticity cHRM
    [PNG_CHUNK_TYPE_gAMA] = 0x414D4167, ///< Gamma gAMA
    [PNG_CHUNK_TYPE_iCCP] = 0x50434369, ///< ICC Profile iCCP
    [PNG_CHUNK_TYPE_sBIT] = 0x54494273, ///< Significant Bits sBIT
    [PNG_CHUNK_TYPE_sRGB] = 0x42475273, ///< Standard RGB sRGB
    [PNG_CHUNK_TYPE_bKGD] = 0x44474B62, ///< Background bKGD
    [PNG_CHUNK_TYPE_hIST] = 0x54534968, ///< Histogram hIST
    [PNG_CHUNK_TYPE_tRNS] = 0x534E5254, ///< Transparency tRNS
    [PNG_CHUNK_TYPE_pHYs] = 0x73594870, ///< Physical Pixel Dimensions pHYs
    [PNG_CHUNK_TYPE_sPLT] = 0x544C5073, ///< Suggested Palette sPLT
    [PNG_CHUNK_TYPE_tIME] = 0x454D4974, ///< Last-Modification Time tIME
    [PNG_CHUNK_TYPE_iTXt] = 0x74545869, ///< International Text iTXt
    [PNG_CHUNK_TYPE_tEXt] = 0x74584574, ///< Textual Data tEXt
    [PNG_CHUNK_TYPE_zTXt] = 0x7448547A, ///< Compressed Text zTXt

    [PNG_CHUNK_TYPE_MAX] = 0x00000000,
};

const boolean_t png_chunk_type_allow_multiple[] = {
    [PNG_CHUNK_TYPE_IHDR] = false,
    [PNG_CHUNK_TYPE_PLTE] = false,
    [PNG_CHUNK_TYPE_IDAT] = true,
    [PNG_CHUNK_TYPE_IEND] = false,
    [PNG_CHUNK_TYPE_cHRM] = false,
    [PNG_CHUNK_TYPE_gAMA] = false,
    [PNG_CHUNK_TYPE_iCCP] = false,
    [PNG_CHUNK_TYPE_sBIT] = false,
    [PNG_CHUNK_TYPE_sRGB] = false,
    [PNG_CHUNK_TYPE_bKGD] = false,
    [PNG_CHUNK_TYPE_hIST] = false,
    [PNG_CHUNK_TYPE_tRNS] = false,
    [PNG_CHUNK_TYPE_pHYs] = false,
    [PNG_CHUNK_TYPE_sPLT] = true,
    [PNG_CHUNK_TYPE_tIME] = false,
    [PNG_CHUNK_TYPE_iTXt] = true,
    [PNG_CHUNK_TYPE_tEXt] = true,
    [PNG_CHUNK_TYPE_zTXt] = true,

    [PNG_CHUNK_TYPE_MAX] = false,
};

static png_chunk_type_t png_chunk_type_from_uint32(uint32_t type) {
    for(png_chunk_type_t i = 0; i < PNG_CHUNK_TYPE_MAX; i++) {
        if(png_chunk_type_strings[i] == type) {
            return i;
        }
    }

    return PNG_CHUNK_TYPE_MAX;
}

static boolean_t png_chunk_order_allowed(png_decoder_t * png_decoder, png_chunk_type_t chunk_type) {
    if(chunk_type != PNG_CHUNK_TYPE_IHDR && png_decoder->chunk_counts[PNG_CHUNK_TYPE_IHDR] == 0) {
        // IHDR chunk must be first chunk
        return false;
    }

    if(chunk_type == PNG_CHUNK_TYPE_IEND && png_decoder->chunk_counts[PNG_CHUNK_TYPE_IEND] > 0) {
        // IEND chunk must be last chunk
        return false;
    }

    if(chunk_type == PNG_CHUNK_TYPE_PLTE && png_decoder->chunk_counts[PNG_CHUNK_TYPE_IDAT]) {
        // PLTE chunk must be before IDAT chunks
        return false;
    }

    if(chunk_type >= PNG_CHUNK_TYPE_cHRM && chunk_type <= PNG_CHUNK_TYPE_sRGB) {
        // cHRM, gAMA, iCCP, sBIT, sRGB chunks must be before PLTE and IDAT chunks
        if(png_decoder->chunk_counts[PNG_CHUNK_TYPE_PLTE] ||
           png_decoder->chunk_counts[PNG_CHUNK_TYPE_IDAT]) {
            return false;
        }
    }

    if(chunk_type >= PNG_CHUNK_TYPE_bKGD && chunk_type <= PNG_CHUNK_TYPE_tRNS) {
        // bKGD, hIST, tRNS chunks must be after PLTE and before IDAT chunks
        if(png_decoder->should_plte_exist && png_decoder->chunk_counts[PNG_CHUNK_TYPE_PLTE] == 0) {
            return false;
        }

        if(png_decoder->chunk_counts[PNG_CHUNK_TYPE_IDAT]) {
            return false;
        }
    }

    if(chunk_type == PNG_CHUNK_TYPE_pHYs && png_decoder->chunk_counts[PNG_CHUNK_TYPE_IDAT]) {
        // pHYs chunk must be before IDAT chunks
        return false;
    }

    if(chunk_type == PNG_CHUNK_TYPE_sPLT && png_decoder->chunk_counts[PNG_CHUNK_TYPE_IDAT]) {
        // sPLT chunk must be before IDAT chunks
        return false;
    }

    return true;
}

static int8_t png_decoder_is_png(png_decoder_t* png_decoder) {
    if(!png_decoder) {
        return -PNG_ERROR_UNKNOWN;
    }

    if(buffer_remaining(png_decoder->buffer) < 8) {
        return -PNG_DECODER_INVALID_PNG_HEADER;
    }

    uint64_t signature = buffer_read_uint64(png_decoder->buffer);

    if(signature != PNG_SIGNATURE) {
        PRINTLOG(PNG, LOG_TRACE, "invalid png signature 0x%llx", signature);
        errno = -PNG_DECODER_INVALID_PNG_HEADER;
        return -PNG_DECODER_INVALID_PNG_HEADER;
    }

    return PNG_SUCCESS;
}

static int8_t png_decoder_parse_ihdr(png_decoder_t* png_decoder, uint8_t* chunk_data, uint32_t length) {
    if(!png_decoder) {
        return -PNG_DECODER_MEMORY_ERROR;
    }

    if(length != 13) {
        PRINTLOG(PNG, LOG_TRACE, "invalid IHDR chunk length %u", length);
        errno = -PNG_DECODER_INVALID_CHUNK_LENGTH;
        return -PNG_DECODER_INVALID_CHUNK_LENGTH;
    }

    if(chunk_data == NULL) {
        errno = -PNG_DECODER_MEMORY_ERROR;
        return -PNG_DECODER_MEMORY_ERROR;
    }

    uint32_t* data32 = (uint32_t*)chunk_data;

    png_decoder->width = BYTE_SWAP32(data32[0]);
    png_decoder->height = BYTE_SWAP32(data32[1]);

    chunk_data += 8;

    png_decoder->bit_depth = *chunk_data++;

    png_decoder->color_type = *chunk_data++;


    boolean_t color_type_valid = false;

    if(png_decoder->color_type == 0) switch(png_decoder->bit_depth) {
        case 1:
        case 2:
        case 4:
        case 8:
        case 16:
            color_type_valid = true;
            break;
        default:
            break;
        }
    else if(png_decoder->color_type == 2)  switch(png_decoder->bit_depth) {
        case 8:
        case 16:
            color_type_valid = true;
            break;
        default:
            break;
        }
    else if(png_decoder->color_type == 3)  switch(png_decoder->bit_depth) {
        case 1:
        case 2:
        case 4:
        case 8:
            color_type_valid = true;
            break;
        default:
            break;
        }
    else if(png_decoder->color_type == 4)  switch(png_decoder->bit_depth) {
        case 8:
        case 16:
            color_type_valid = true;
            break;
        default:
            break;
        }
    else if(png_decoder->color_type == 6)  switch(png_decoder->bit_depth) {
        case 8:
        case 16:
            color_type_valid = true;
            break;
        default:
            break;
        }

    if(!color_type_valid) {
        PRINTLOG(PNG, LOG_TRACE, "invalid color type %u bit depth %u", png_decoder->color_type, png_decoder->bit_depth);
        errno = -PNG_DECODER_INVALID_COLOR_TYPE;
        return -PNG_DECODER_INVALID_COLOR_TYPE;
    }

    png_decoder->compression_method = *chunk_data++;

    if(png_decoder->compression_method != 0) {
        PRINTLOG(PNG, LOG_TRACE, "invalid compression method %u", png_decoder->compression_method);
        errno = -PNG_DECODER_INVALID_COMPRESSION_METHOD;
        return -PNG_DECODER_INVALID_COMPRESSION_METHOD;
    }

    png_decoder->filter_method = *chunk_data++;

    if(png_decoder->filter_method != 0) {
        PRINTLOG(PNG, LOG_TRACE, "invalid filter method %u", png_decoder->filter_method);
        errno = -PNG_DECODER_INVALID_FILTER_METHOD;
        return -PNG_DECODER_INVALID_FILTER_METHOD;
    }

    png_decoder->interlace_method = *chunk_data;

    if(png_decoder->interlace_method > 1) {
        PRINTLOG(PNG, LOG_TRACE, "invalid interlace method %u", png_decoder->interlace_method);
        errno = -PNG_DECODER_INVALID_INTERLACE_METHOD;
        return -PNG_DECODER_INVALID_INTERLACE_METHOD;
    }

    PRINTLOG(PNG, LOG_TRACE, "IHDR chunk width: %u height: %u bit depth: %u color type: %u compression method: %u filter method: %u interlace method: %u",
             png_decoder->width, png_decoder->height, png_decoder->bit_depth, png_decoder->color_type, png_decoder->compression_method, png_decoder->filter_method, png_decoder->interlace_method);

    return PNG_SUCCESS;
}

static int8_t png_decoder_parse_chunk(png_decoder_t* png_decoder) {
    if(!png_decoder) {
        return -PNG_DECODER_MEMORY_ERROR;
    }

    if(png_decoder->chunk_counts[PNG_CHUNK_TYPE_IEND] > 0) {
        PRINTLOG(PNG, LOG_TRACE, "IEND chunk already parsed, cannot parse more chunks");
        errno = -PNG_DECODER_IEND_ALREADY_PARSED;
        return -PNG_DECODER_IEND_ALREADY_PARSED;
    }

    // at least 12 bytes are required to read chunk
    // 4 bytes for length, 4 bytes for type, 4 bytes for crc
    if(buffer_remaining(png_decoder->buffer) < 12) {
        errno = -PNG_DECODER_INVALID_CHUNK_LENGTH;
        return -PNG_DECODER_INVALID_CHUNK_LENGTH;
    }

    uint32_t length = buffer_read_uint32(png_decoder->buffer);
    length = BYTE_SWAP32(length);

    uint32_t type = buffer_read_uint32(png_decoder->buffer);
    png_chunk_type_t chunk_type = png_chunk_type_from_uint32(type);

    if(chunk_type == PNG_CHUNK_TYPE_MAX) {
        // skip unknown chunk
        if(!buffer_seek(png_decoder->buffer, length + 4, BUFFER_SEEK_DIRECTION_CURRENT)) {
            return -PNG_ERROR_UNKNOWN;
        }

        PRINTLOG(PNG, LOG_TRACE, "unknown chunk type %.4s", (char_t*)&type);
        errno = -PNG_DECODER_UNKNOWN_CHUNK_TYPE;
        return -PNG_DECODER_UNKNOWN_CHUNK_TYPE;
    }

    // check for single chunk constraint
    if(png_decoder->chunk_counts[chunk_type] > 0 && !png_chunk_type_allow_multiple[chunk_type]) {
        PRINTLOG(PNG, LOG_TRACE, "chunk type %.4s already parsed, cannot parse more chunks", (char_t*)&type);
        errno = -PNG_DECODER_MULTIPLE_CHUNK_TYPE_NOT_ALLOWED;
        return -PNG_DECODER_MULTIPLE_CHUNK_TYPE_NOT_ALLOWED;
    }

    // check for chunk order constraint
    if(!png_chunk_order_allowed(png_decoder, chunk_type)) {
        PRINTLOG(PNG, LOG_TRACE, "chunk type %.4s not allowed at this position", (char_t*)&type);
        errno = -PNG_DECODER_INCORRECT_CHUNK_ORDER;
        return -PNG_DECODER_INCORRECT_CHUNK_ORDER;
    }

    png_decoder->chunk_counts[chunk_type]++;


    PRINTLOG(PNG, LOG_TRACE, "chunk type %.4s length %u", (char_t*)&type, length);

    uint8_t* chunk_data = NULL;
    uint32_t chunk_crc = CRC32_SEED;

    chunk_crc = crc32_sum(&type, 4, chunk_crc);

    if(length) {
        chunk_data = buffer_get_view(png_decoder->buffer, length);

        if(!chunk_data) {
            errno = -PNG_DECODER_MEMORY_ERROR;
            return -PNG_DECODER_MEMORY_ERROR;
        }

        chunk_crc = crc32_sum(chunk_data, length, chunk_crc);
    }

    chunk_crc = crc32_finalize(chunk_crc);

    if(!buffer_seek(png_decoder->buffer, length, BUFFER_SEEK_DIRECTION_CURRENT)) {
        errno = -PNG_ERROR_UNKNOWN;
        return -PNG_ERROR_UNKNOWN;
    }

    uint32_t crc = buffer_read_uint32(png_decoder->buffer);
    crc = BYTE_SWAP32(crc);

    if(crc != chunk_crc) {
        PRINTLOG(PNG, LOG_TRACE, "crc mismatch 0x%x != 0x%x", crc, chunk_crc);
        errno = -PNG_DECODER_CRC_MISMATCH;
        return -PNG_DECODER_CRC_MISMATCH;
    }

    if(chunk_type == PNG_CHUNK_TYPE_IHDR) {
        if(png_decoder_parse_ihdr(png_decoder, chunk_data, length) != 0) {
            return errno;
        }

        // we only support R8G8B8A8 format
        if(png_decoder->color_type != 6 || png_decoder->bit_depth != 8) {
            PRINTLOG(PNG, LOG_TRACE, "unsupported color type %u bit depth %u", png_decoder->color_type, png_decoder->bit_depth);
            errno = -PNG_DECODER_UNSUPPORTED_COLOR_TYPE;
            return -PNG_DECODER_UNSUPPORTED_COLOR_TYPE;
        }

        png_decoder->compressed_image_buffer = buffer_new();

        if(!png_decoder->compressed_image_buffer) {
            errno = -PNG_DECODER_MEMORY_ERROR;
            return -PNG_DECODER_MEMORY_ERROR;
        }
    } else if(chunk_type == PNG_CHUNK_TYPE_IDAT) {
        if(length && !buffer_append_bytes(png_decoder->compressed_image_buffer, chunk_data, length)) {
            errno = -PNG_DECODER_MEMORY_ERROR;
            return -PNG_DECODER_MEMORY_ERROR;
        }

        png_decoder->total_idat_length += length;
    }

    return PNG_SUCCESS;
}

static int8_t png_decoder_init(png_decoder_t* png_decoder, buffer_t* buffer) {
    if(!png_decoder || !buffer) {
        return -PNG_DECODER_MEMORY_ERROR;
    }

    png_decoder->buffer = buffer;

    if(png_decoder_is_png(png_decoder) != 0) {
        return errno;
    }

    png_decoder->compression = (compression_t*)compression_get(COMPRESSION_TYPE_DEFLATE);

    if(!png_decoder->compression) {
        return -PNG_DECODER_MEMORY_ERROR;
    }

    return PNG_SUCCESS;
}

static int8_t png_decoder_parse_chunks(png_decoder_t* png_decoder) {
    if(!png_decoder) {
        return -PNG_DECODER_MEMORY_ERROR;
    }

    while(buffer_remaining(png_decoder->buffer) > 0) {
        if(png_decoder_parse_chunk(png_decoder) != 0) {
            if(png_decoder->compressed_image_buffer) {
                buffer_destroy(png_decoder->compressed_image_buffer);
            }

            return errno;
        }
    }

    if(buffer_get_length(png_decoder->compressed_image_buffer) != png_decoder->total_idat_length) {
        buffer_destroy(png_decoder->compressed_image_buffer);

        errno = -PNG_DECODER_SIZE_MISMATCH;
        return -PNG_DECODER_SIZE_MISMATCH;
    }

    if(png_decoder->chunk_counts[PNG_CHUNK_TYPE_IEND] == 0) {
        buffer_destroy(png_decoder->compressed_image_buffer);
        errno = -PNG_DECODER_IEND_NOT_FOUND;
        return -PNG_DECODER_IEND_NOT_FOUND;
    }

    if(!buffer_seek(png_decoder->compressed_image_buffer, 0, BUFFER_SEEK_DIRECTION_START)) {
        buffer_destroy(png_decoder->compressed_image_buffer);
        return -PNG_ERROR_UNKNOWN;
    }

    return PNG_SUCCESS;
}

typedef enum png_filter_type_t {
    PNG_FILTER_TYPE_NONE,
    PNG_FILTER_TYPE_SUB,
    PNG_FILTER_TYPE_UP,
    PNG_FILTER_TYPE_AVERAGE,
    PNG_FILTER_TYPE_PAETH,
    PNG_FILTER_TYPE_MAX,
} png_filter_type_t;

typedef uint8_t (*png_filter_t)(uint8_t* dst, int64_t dst_idx,
                                uint32_t scanline_len, uint32_t bpp,
                                int64_t x, int64_t y);

static uint8_t png_filter_none(uint8_t* dst, int64_t dst_idx,
                               uint32_t scanline_len, uint32_t bpp,
                               int64_t x, int64_t y) {
    UNUSED(dst);
    UNUSED(dst_idx);
    UNUSED(scanline_len);
    UNUSED(bpp);
    UNUSED(x);
    UNUSED(y);

    return 0;
}

static uint8_t png_filter_sub(uint8_t* dst, int64_t dst_idx,
                              uint32_t scanline_len, uint32_t bpp,
                              int64_t x, int64_t y) {
    UNUSED(scanline_len);
    UNUSED(bpp);
    UNUSED(y);

    uint8_t left = 0;

    if(x >= bpp) {
        left = dst[dst_idx - bpp];
    }

    return left;
}

static uint8_t png_filter_up(uint8_t* dst, int64_t dst_idx,
                             uint32_t scanline_len, uint32_t bpp,
                             int64_t x, int64_t y) {
    UNUSED(scanline_len);
    UNUSED(bpp);
    UNUSED(x);

    uint8_t up = 0;

    if(y > 0) {
        up = dst[dst_idx - scanline_len];
    }

    return up;
}

static uint8_t png_filter_average(uint8_t* dst, int64_t dst_idx,
                                  uint32_t scanline_len, uint32_t bpp,
                                  int64_t x, int64_t y) {
    uint16_t left = 0;
    uint16_t up = 0;

    if(x >= bpp) {
        left = dst[dst_idx - bpp];
    }

    if(y > 0) {
        up = dst[dst_idx - scanline_len];
    }

    return (left + up) / 2;
}

static uint8_t png_paeth_predictor(uint16_t a, uint16_t b, uint16_t c) {
    int32_t p = a + b - c;
    int32_t pa = ABS(p - a);
    int32_t pb = ABS(p - b);
    int32_t pc = ABS(p - c);

    if(pa <= pb && pa <= pc) {
        return a;
    }

    if(pb <= pc) {
        return b;
    }

    return c;
}

static uint8_t png_filter_paeth(uint8_t* dst, int64_t dst_idx,
                                uint32_t scanline_len, uint32_t bpp,
                                int64_t x, int64_t y) {
    uint16_t left = 0;
    uint16_t up = 0;
    uint16_t up_left = 0;

    if(x >= bpp) {
        left = dst[dst_idx - bpp];
    }

    if(y > 0) {
        up = dst[dst_idx - scanline_len];

        if(x >= bpp) {
            up_left = dst[dst_idx - scanline_len - bpp];
        }
    }

    return png_paeth_predictor(left, up, up_left);
}

static int8_t png_decoder_apply_filter(png_decoder_t* png_decoder, const uint8_t* img_data, const graphics_raw_image_t* res) {
    const uint8_t* src = img_data;
    uint8_t* dst = (uint8_t*)res->data;
    int64_t src_idx = 0;
    int64_t dst_idx = 0;

    // bbp for R8G8B8A8
    int32_t bpp = sizeof(pixel_t);
    int64_t scanline_len = res->width * bpp;

    for(int64_t y = 0; y < res->height; y++) {
        uint8_t filter = src[src_idx++];

        png_filter_t filter_func = NULL;

        switch(filter) {
        case PNG_FILTER_TYPE_NONE:
            filter_func = png_filter_none;
            break;
        case PNG_FILTER_TYPE_SUB:
            filter_func = png_filter_sub;
            break;
        case PNG_FILTER_TYPE_UP:
            filter_func = png_filter_up;
            break;
        case PNG_FILTER_TYPE_AVERAGE:
            filter_func = png_filter_average;
            break;
        case PNG_FILTER_TYPE_PAETH:
            filter_func = png_filter_paeth;
            break;
        default:
            break;
        }

        if(!filter_func) {
            PRINTLOG(PNG, LOG_TRACE, "invalid filter type %u", filter);
            errno = -PNG_DECODER_INVALID_FILTER_TYPE;
            return -PNG_DECODER_INVALID_FILTER_TYPE;
        }

        for(int64_t x = 0; x < scanline_len; x++) {
            dst[dst_idx] = src[src_idx] + filter_func(dst, dst_idx, scanline_len, bpp, x, y);

            src_idx++;
            dst_idx++;
        }
    }

    uint64_t img_w_f_len = (png_decoder->width * sizeof(pixel_t) + 1) * png_decoder->height;

    if((uint64_t)src_idx != img_w_f_len) {
        PRINTLOG(PNG, LOG_TRACE, "src length mismatch %lli != %lli", src_idx, img_w_f_len);
        errno = -PNG_DECODER_UNCOMPRESS_SIZE_MISMATCH;
        return -PNG_DECODER_UNCOMPRESS_SIZE_MISMATCH;
    }

    uint64_t img_len = res->width * res->height * sizeof(pixel_t);

    if((uint64_t)dst_idx != img_len) {
        PRINTLOG(PNG, LOG_TRACE, "dst length mismatch %lli != %lli", dst_idx, img_len);
        errno = -PNG_DECODER_UNCOMPRESS_SIZE_MISMATCH;
        return -PNG_DECODER_UNCOMPRESS_SIZE_MISMATCH;
    }

    return PNG_SUCCESS;
}

static graphics_raw_image_t* png_decoder_get_image(png_decoder_t* png_decoder) {
    if(!png_decoder) {
        errno = -PNG_DECODER_MEMORY_ERROR;
        return NULL;
    }

    if(!png_decoder->compressed_image_buffer){
        errno = -PNG_DECODER_MEMORY_ERROR;
        return NULL;
    }

    uint16_t zlib_header = buffer_read_uint16(png_decoder->compressed_image_buffer);
    zlib_header = BYTE_SWAP16(zlib_header);

    if(zlib_header != 0x78DA && zlib_header != 0x78D8) {
        PRINTLOG(PNG, LOG_TRACE, "invalid zlib header 0x%x", zlib_header);
        errno = -PNG_DECODER_INVALID_ZLIB_HEADER;
        buffer_destroy(png_decoder->compressed_image_buffer);
        return NULL;
    }

    uint64_t capacity = (sizeof(pixel_t) * png_decoder->width + 1) * png_decoder->height;
    buffer_t* out = buffer_new_with_capacity(NULL, capacity);

    if(!out) {
        PRINTLOG(PNG, LOG_TRACE, "output buffer allocation failed");
        errno = -PNG_DECODER_MEMORY_ERROR;
        buffer_destroy(png_decoder->compressed_image_buffer);
        return NULL;
    }

    if(png_decoder->compression->unpack(png_decoder->compressed_image_buffer, out) != 0) {
        PRINTLOG(PNG, LOG_TRACE, "decompress failed");
        errno = -PNG_ERROR_UNKNOWN;
        buffer_destroy(png_decoder->compressed_image_buffer);
        buffer_destroy(out);
        return NULL;
    }

    if(buffer_remaining(png_decoder->compressed_image_buffer) != 4) {
        PRINTLOG(PNG, LOG_TRACE, "decompress failed. remaining: %lli", buffer_remaining(png_decoder->compressed_image_buffer));
        errno = -PNG_ERROR_UNKNOWN;
        buffer_destroy(png_decoder->compressed_image_buffer);
        buffer_destroy(out);
        return NULL;
    }

    uint32_t adler32 = buffer_read_uint32(png_decoder->compressed_image_buffer);
    adler32 = BYTE_SWAP32(adler32);

    buffer_destroy(png_decoder->compressed_image_buffer);

    uint64_t img_len = buffer_get_length(out);

    if(img_len != capacity) {
        PRINTLOG(PNG, LOG_TRACE, "image data length mismatch %lli != %lli", img_len, capacity);
        errno = -PNG_DECODER_SIZE_MISMATCH;
        buffer_destroy(out);

        return NULL;
    }

    uint8_t* img_data = buffer_get_all_bytes_and_destroy(out, NULL);

    if(!img_data) {
        PRINTLOG(PNG, LOG_TRACE, "image data extraction failed");
        errno = -PNG_DECODER_MEMORY_ERROR;
        memory_free(img_data);
        return NULL;
    }

    uint32_t calc_adler32 = adler32_sum(img_data, img_len, ADLER32_SEED);

    if(calc_adler32 != adler32) {
        PRINTLOG(PNG, LOG_TRACE, "adler32 checksum mismatch 0x%x != 0x%x", calc_adler32, adler32);
        errno = -PNG_DECODER_ADLER32_MISMATCH;
        memory_free(img_data);
        return NULL;
    }

    graphics_raw_image_t* res = memory_malloc(sizeof(graphics_raw_image_t));

    if(!res) {
        PRINTLOG(PNG, LOG_TRACE, "raw image memory allocation failed");
        errno = -PNG_DECODER_MEMORY_ERROR;
        memory_free(img_data);
        return NULL;
    }

    res->width = png_decoder->width;
    res->height = png_decoder->height;

    res->data = memory_malloc(res->width * res->height * sizeof(pixel_t));

    if(!res->data) {
        PRINTLOG(PNG, LOG_TRACE, "raw image data memory allocation failed");
        errno = -PNG_DECODER_MEMORY_ERROR;
        memory_free(img_data);
        memory_free(res);
        return NULL;
    }

    if(png_decoder_apply_filter(png_decoder, img_data, res) != 0) {
        PRINTLOG(PNG, LOG_TRACE, "apply filter failed");
        memory_free(img_data);
        memory_free(res->data);
        memory_free(res);
        return NULL;
    }

    memory_free(img_data);

    return res;
}

graphics_raw_image_t* graphics_load_png_image(const uint8_t* data, uint32_t size) {
    if(!data || !size) {
        return NULL;
    }

    buffer_t* buffer = buffer_encapsulate((uint8_t*)data, size);

    if(!buffer) {
        errno = -PNG_DECODER_MEMORY_ERROR;
        return NULL;
    }

    png_decoder_t png_decoder = {0};

    if(png_decoder_init(&png_decoder, buffer) != 0) {
        buffer_destroy(buffer);
        return NULL;
    }

    if(png_decoder_parse_chunks(&png_decoder) != 0) {
        buffer_destroy(buffer);
        return NULL;
    }

    graphics_raw_image_t* image = png_decoder_get_image(&png_decoder);

    buffer_destroy(buffer);

    return image;
}
