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

/**
 * @enum compression_type_t
 * @brief Enumerates the supported compression algorithms.
 */
typedef enum compression_type_t {
    COMPRESSION_TYPE_NONE = 0, ///< No compression applied.
    COMPRESSION_TYPE_ZPACK, ///< ZPACK compression algorithm.
    COMPRESSION_TYPE_DEFLATE, ///< DEFLATE compression algorithm.
    COMPRESSION_TYPE_GZIP, ///< GZIP compression algorithm.
    COMPRESSION_MAX, ///< Sentinel value for the maximum compression type.
} compression_type_t;


/*! @brief ZPACK file header magic string. */
#define COMPRESSION_HEADER_MAGIC "TOSCMP"

/**
 * @struct compression_header_t
 * @brief Defines the header structure for compressed data.
 *
 * This header precedes compressed data and contains metadata necessary
 * for decompression and integrity checking.
 */
typedef struct compression_header_t {
    const char magic[7]; ///< @brief Magic string "TOSCMP" to identify compressed data.
    uint8_t    type; ///< @brief Type of compression used, from @ref compression_type_t.
    uint64_t   unpacked_size; ///< @brief The size of the data after it has been unpacked.
    uint64_t   packed_size; ///< @brief The size of the data when it is packed (compressed).
    uint64_t   unpacked_hash; ///< @brief XXH64 hash value of the unpacked data for integrity verification.
    uint64_t   packed_hash; ///< @brief XXH64 hash value of the packed data for integrity verification.
} __attribute__((packed)) compression_header_t;

/**
 * @struct compression_t
 * @brief Represents a compression algorithm interface.
 *
 * This structure encapsulates the type of compression and pointers to
 * its corresponding pack and unpack functions.
 */
typedef struct compression_t {
    compression_type_t type; ///< @brief The type of compression algorithm.
    /**
     * @brief Function pointer to the packing (compression) routine.
     * @param in Pointer to the input buffer containing uncompressed data.
     * @param out Pointer to the output buffer to store compressed data.
     * @return 0 on success, non-zero on error.
     */
    int8_t (*pack)(buffer_t* in, buffer_t* out);
    /**
     * @brief Function pointer to the unpacking (decompression) routine.
     * @param in Pointer to the input buffer containing compressed data.
     * @param out Pointer to the output buffer to store decompressed data.
     * @return 0 on success, non-zero on error.
     */
    int8_t (*unpack)(buffer_t* in, buffer_t* out);
} compression_t;

/**
 * @brief Null compression packing function.
 *
 * This function performs no actual compression; it simply copies the input
 * buffer to the output buffer.
 *
 * @param in Pointer to the input buffer.
 * @param out Pointer to the output buffer.
 * @return 0 on success.
 */
int8_t compression_null_pack(buffer_t* in, buffer_t* out);

/**
 * @brief Null compression unpacking function.
 *
 * This function performs no actual decompression; it simply copies the input
 * buffer to the output buffer.
 *
 * @param in Pointer to the input buffer.
 * @param out Pointer to the output buffer.
 * @return 0 on success.
 */
int8_t compression_null_unpack(buffer_t* in, buffer_t* out);

/**
 * @brief Retrieves the compression interface for a given type.
 *
 * @param type The desired compression type from @ref compression_type_t.
 * @return A pointer to the @ref compression_t structure for the specified type,
 *         or NULL if the type is not supported or invalid.
 */
const compression_t* compression_get(compression_type_t type);

#ifdef __cplusplus
}
#endif

#endif
