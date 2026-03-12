/**
 * @file gzip.64.c
 * @brief gzip deflate packing and unpacking library
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <gzip.h>
#include <compression.h>
#include <deflate.h>
#include <logging.h>
#include <crc.h>

MODULE("turnstone.lib");

int8_t gzip_pack(buffer_t* in, buffer_t* out) {
    if(!in || !out) {
        PRINTLOG(COMPRESSION, LOG_ERROR, "Input and output buffers must not be NULL");
        return -1;
    }

    size_t in_size   = buffer_get_length(in);
    uint8_t* in_data = buffer_get_raw_bytes(in);

    uint32_t crc32 = crc32_sum(in_data, in_size, CRC32_SEED);
    crc32 = crc32_finalize(crc32);

    gzip_header_t header = {
        .id1                = GZIP_HEADER_ID1,
        .id2                = GZIP_HEADER_ID2,
        .compression_method = GZIP_COMPRESSION_METHOD_DEFLATE,
        .flags              = GZIP_FLAG_NONE,
        .mtime              = 0,
        .extra_flags        = GZIP_EXTRA_FLAG_MAXIMUM_COMPRESSION,
        .os_type            = GZIP_OS_TYPE_UNKNOWN,
    };

    buffer_append_bytes(out, (uint8_t*)&header, sizeof(gzip_header_t));

    const compression_t* deflate_compression = compression_get(COMPRESSION_TYPE_DEFLATE);

    if(deflate_compression->pack(in, out) != 0) {
        PRINTLOG(COMPRESSION, LOG_ERROR, "Failed to deflate data for gzip packing");
        return -1;
    }

    gzip_footer_t footer = {
        .crc32 = crc32,
        .isize = in_size % 0x100000000ULL,
    };

    buffer_append_bytes(out, (uint8_t*)&footer, sizeof(gzip_footer_t));

    return 0;
}

int8_t gzip_unpack(buffer_t* in, buffer_t* out) {
    if(!in || !out) {
        PRINTLOG(COMPRESSION, LOG_ERROR, "Input and output buffers must not be NULL");
        return -1;
    }

    size_t in_size   = buffer_get_length(in);
    uint8_t* in_data = buffer_get_raw_bytes(in);

    if(in_size < sizeof(gzip_header_t) + sizeof(gzip_footer_t)) {
        PRINTLOG(COMPRESSION, LOG_ERROR, "Input buffer is too small to be a valid gzip file");
        return -1;
    }

    gzip_header_t* header = (gzip_header_t*)in_data;

    if(header->id1 != GZIP_HEADER_ID1 || header->id2 != GZIP_HEADER_ID2) {
        PRINTLOG(COMPRESSION, LOG_ERROR, "Input buffer is not a valid gzip file");
        return -1;
    }

    if(header->compression_method != GZIP_COMPRESSION_METHOD_DEFLATE) {
        PRINTLOG(COMPRESSION, LOG_ERROR, "Unsupported compression method %d in gzip file", header->compression_method);
        return -1;
    }

    in_data += sizeof(gzip_header_t);
    in_size -= sizeof(gzip_header_t);

    if(header->flags & GZIP_FLAG_FEXTRA) {
        if(in_size < 2) {
            PRINTLOG(COMPRESSION, LOG_ERROR, "Input buffer is too small to be a valid gzip file (missing extra field length)");
            return -1;
        }

        uint16_t extra_len = *(uint16_t*)(void*)in_data;

        in_data += 2;

        if(in_size < 2 + (size_t)extra_len) {
            PRINTLOG(COMPRESSION, LOG_ERROR, "Input buffer is too small to be a valid gzip file (missing extra field data)");
            return -1;
        }

        in_data += extra_len;
        in_size -= 2 + extra_len;
    }

    if(header->flags & GZIP_FLAG_FNAME) {
        while(in_size > 0 && *in_data != '\0') {
            in_data++;
            in_size--;
        }

        if(in_size == 0) {
            PRINTLOG(COMPRESSION, LOG_ERROR, "Input buffer is too small to be a valid gzip file (missing original file name)");
            return -1;
        }

        in_data++;
        in_size--;
    }

    if(header->flags & GZIP_FLAG_FCOMMENT) {
        while(in_size > 0 && *in_data != '\0') {
            in_data++;
            in_size--;
        }

        if(in_size == 0) {
            PRINTLOG(COMPRESSION, LOG_ERROR, "Input buffer is too small to be a valid gzip file (missing comment)");
            return -1;
        }

        in_data++;
        in_size--;
    }

    if(header->flags & GZIP_FLAG_FHCRC) {
        if(in_size < 2) {
            PRINTLOG(COMPRESSION, LOG_ERROR, "Input buffer is too small to be a valid gzip file (missing header CRC16)");
            return -1;
        }

        in_data += 2;
        in_size -= 2;
    }

    if(in_size < sizeof(gzip_footer_t)) {
        PRINTLOG(COMPRESSION, LOG_ERROR, "Input buffer is too small to be a valid gzip file (missing footer)");
        return -1;
    }

    gzip_footer_t* footer = (gzip_footer_t*)(in_data + in_size - sizeof(gzip_footer_t));

    uint32_t expected_crc32 = footer->crc32;
    uint32_t expected_isize = footer->isize;

    size_t compressed_data_size = in_size - sizeof(gzip_footer_t);

    buffer_t* compressed_data_buf = buffer_encapsulate(in_data, compressed_data_size);

    const compression_t* deflate_compression = compression_get(COMPRESSION_TYPE_DEFLATE);

    if(deflate_compression->unpack(compressed_data_buf, out) != 0) {
        PRINTLOG(COMPRESSION, LOG_ERROR, "Failed to inflate data for gzip unpacking");
        buffer_destroy(compressed_data_buf);
        return -1;
    }

    buffer_destroy(compressed_data_buf);

    size_t out_size   = buffer_get_length(out);
    uint8_t* out_data = buffer_get_raw_bytes(out);

    uint32_t actual_crc32 = crc32_sum(out_data, out_size, CRC32_SEED);
    actual_crc32 = crc32_finalize(actual_crc32);

    if(actual_crc32 != expected_crc32) {
        PRINTLOG(COMPRESSION, LOG_ERROR, "CRC32 checksum does not match expected value (expected 0x%08x, got 0x%08x)", expected_crc32, actual_crc32);
        return -1;
    }

    if(out_size != expected_isize) {
        PRINTLOG(COMPRESSION, LOG_ERROR, "Decompressed size does not match expected size (expected %u, got %llu)", expected_isize, out_size);
        return -1;
    }

    return 0;
}
