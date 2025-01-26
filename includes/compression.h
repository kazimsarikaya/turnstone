/**
 * @file compression.h
 * @brief compression interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___COMPRESSION_H
#define ___COMPRESSION_H

#include <types.h>
#include <buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum compression_type_t {
    COMPRESSION_TYPE_NONE = 0,
    COMPRESSION_TYPE_ZPACK,
    COMPRESSION_TYPE_DEFLATE,
    COMPRESSION_MAX,
} compression_type_t;


/*! zpack file header magic*/
#define COMPRESSION_HEADER_MAGIC "TOSCMP"

/**
 * @struct compression_header_t
 * @brief compression header
 */
typedef struct compression_header_t {
    const char magic[7]; ///< magic string
    uint8_t    type; ///< compression type
    uint64_t   unpacked_size; ///< the size of data when it is unpacked
    uint64_t   packed_size; ///< size of data when packed.
    uint64_t   unpacked_hash; ///< xhash64 value of unpacked data
    uint64_t   packed_hash; ///< xhash64 value of packed data
} __attribute__((packed)) compression_header_t;

typedef struct compression_t {
    compression_type_t type;
    int8_t (*pack)(buffer_t* in, buffer_t* out);
    int8_t (*unpack)(buffer_t* in, buffer_t* out);
} compression_t;

int8_t compression_null_pack(buffer_t* in, buffer_t* out);
int8_t compression_null_unpack(buffer_t* in, buffer_t* out);

const compression_t* compression_get(compression_type_t type);

#ifdef __cplusplus
}
#endif

#endif
