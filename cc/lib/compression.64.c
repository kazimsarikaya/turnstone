/**
 * @file compression.64.c
 * @brief Compression library.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <compression.h>
#include <deflate.h>
#include <zpack.h>

MODULE("turnstone.lib");

int8_t compression_null_pack(buffer_t* in, buffer_t* out) {
    return buffer_append_buffer(out, in) == NULL ? -1 : 0;
}

int8_t compression_null_unpack(buffer_t* in, buffer_t* out) {
    return buffer_append_buffer(out, in) == NULL ? -1 : 0;
}

compression_t compression_null = {
    .type = COMPRESSION_TYPE_NONE,
    .pack = compression_null_pack,
    .unpack = compression_null_unpack,
};

compression_t deflate_compression = {
    .type = COMPRESSION_TYPE_DEFLATE,
    .pack = deflate_deflate,
    .unpack = deflate_inflate,
};

compression_t zpack_compression = {
    .type = COMPRESSION_TYPE_ZPACK,
    .pack = zpack_pack,
    .unpack = zpack_unpack,
};

compression_t* compression_get(compression_type_t type) {
    switch(type) {
    case COMPRESSION_TYPE_NONE:
        return &compression_null;
    case COMPRESSION_TYPE_DEFLATE:
        return &deflate_compression;
    case COMPRESSION_TYPE_ZPACK:
        return &zpack_compression;
    default:
        return NULL;
    }

    return NULL;
}
