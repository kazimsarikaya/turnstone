/**
 * @file siphash.h
 * @brief sip hash interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___SIPHASH_H
/*! prevent duplicate header error macro */
#define ___SIPHASH_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Computes a 128-bit SipHash of the input data.
 *
 * SipHash is a family of pseudorandom functions (PRFs) optimized for speed
 * on short messages and designed to defend against hash flooding attacks.
 * This function computes a 128-bit hash value using the SipHash-2-4 variant.
 *
 * @param data Pointer to the input data buffer.
 * @param len The length of the input data in bytes.
 * @param seed A 128-bit seed value used to initialize the hash state.
 * @return The computed 128-bit SipHash value.
 */
uint128_t siphash128(const void* data, uint64_t len, uint128_t seed);

/**
 * @brief Computes a 64-bit SipHash of the input data.
 *
 * This function computes a 64-bit hash value using the SipHash-2-4 variant.
 * It is suitable for applications requiring a robust, collision-resistant
 * hash function for short inputs, such as hash table keys.
 *
 * @param data Pointer to the input data buffer.
 * @param len The length of the input data in bytes.
 * @param seed A 64-bit seed value used to initialize the hash state.
 * @return The computed 64-bit SipHash value.
 */
uint64_t siphash64(const void* data, uint64_t len, uint64_t seed);

#ifdef __cplusplus
}
#endif

#endif
