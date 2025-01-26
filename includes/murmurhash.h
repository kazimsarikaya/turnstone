/**
 * @file murmurhash.h
 * @brief murmur hash interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___MURMURHASH_H
/*! prevent duplicate header error macro */
#define ___MURMURHASH_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief computes murmur hash 64 bit a version
 * @param[in] data data for hashing
 * @param[in] len data length
 * @param[in] seed seed
 * @return hash value
 */
uint64_t murmurhash64a(const void* data, uint64_t len, uint64_t seed);


/**
 * @brief computes murmur hash v3 128 bit
 * @param[in] data data for hashing
 * @param[in] len data length
 * @param[in] seed seed
 * @return hash value
 */
uint128_t murmurhash3_128(const void* data, uint64_t len, uint64_t seed);

#ifdef __cplusplus
}
#endif

#endif
