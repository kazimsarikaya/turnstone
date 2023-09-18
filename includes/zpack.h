/**
 * @file zpack.h
 * @brief z77 compression algorithm header

 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___ZPACK_H
/*! prevent duplicate header error macro */
#define ___ZPACK_H 0

#include <buffer.h>

/*! zpack file header magic*/
#define ZPACK_FORMAT_MAGIC 0x544D464B4341505AULL

/**
 * @struct zpack_format_t
 * @brief a zpacked data has this header at the beginig of the file.
 */
typedef struct zpack_format_t {
    uint64_t magic; ///< header magic @ref ZPACK_FORMAT_MAGIC
    uint64_t unpacked_size; ///< the size of data when it is unpacked
    uint64_t packed_size; ///< size of data when packed.
    uint64_t unpacked_hash; ///< xhash64 value of unpacked data
    uint64_t packed_hash; ///< xhash64 value of packed data
} __attribute__((packed)) zpack_format_t;

/**
 * @brief packs data at input buffer to output buffer with z77 algorithm
 * @param[in] in input buffer
 * @param[in] out output buffer
 * @return size of output buffer
 */
int64_t zpack_pack(buffer_t* in, buffer_t* out);

/**
 * @brief unpacks data at input buffer to output buffer with z77 algorithm
 * @param[in] in input buffer
 * @param[in] out output buffer
 * @return size of output buffer
 */
int64_t zpack_unpack(buffer_t* in, buffer_t* out);

#endif
