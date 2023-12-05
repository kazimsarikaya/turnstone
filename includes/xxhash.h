/**
 * @file xxhash.h
 * @brief xxHash - Extremely Fast Hash algorithm.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___XXHASH_H
/*! macro for preventing multiple inclusions */
#define ___XXHASH_H 0

#include <types.h>

/**
 * @typedef xxhash64_context_t
 * @brief opaque xxhash64 context structure
 */
typedef struct xxhash64_context_t xxhash64_context_t; ///< opaque xxhash64 context structure

/**
 * @brief initialize xxhash64 context
 * @param[in] seed seed value
 * @return xxhash64 context
 */
xxhash64_context_t* xxhash64_init(uint64_t seed);

/**
 * @brief update xxhash64 context
 * @param[in] ctx xxhash64 context
 * @param[in] input input data
 * @param[in] length input data length
 * @return 0 if success, -1 if error
 */
int8_t xxhash64_update(xxhash64_context_t* ctx, const void* input, uint64_t length);

/**
 * @brief finalize xxhash64 context
 * @details call this function after calling xxhash64_update(), also destroy xxhash64 context
 * @param[in] ctx xxhash64 context
 * @return xxhash64 value
 */
uint64_t xxhash64_final(xxhash64_context_t* ctx);

/**
 * @brief calculate xxhash64 value
 * @param[in] input input data
 * @param[in] length input data length
 * @param[in] seed seed value
 * @return xxhash64 value
 */
uint64_t xxhash64_hash_with_seed(const void* input, uint64_t length, uint64_t seed);

/**
 * @brief calculate xxhash64 value with seed 0
 * @param[in] i input data
 * @param[in] l input data length
 * @return xxhash64 value
 */
#define xxhash64_hash(i, l) xxhash64_hash_with_seed(i, l, 0)

/**
 * @typedef xxhash32_context_t
 * @brief opaque xxhash32 context structure
 */
typedef struct xxhash32_context_t xxhash32_context_t; ///< opaque xxhash32 context structure

/**
 * @brief initialize xxhash32 context
 * @param[in] seed seed value
 * @return xxhash32 context
 */
xxhash32_context_t* xxhash32_init(uint32_t seed);

/**
 * @brief update xxhash32 context
 * @param[in] ctx xxhash32 context
 * @param[in] input input data
 * @param[in] length input data length
 * @return 0 if success, -1 if error
 */
int8_t xxhash32_update(xxhash32_context_t* ctx, const void* input, uint64_t length);

/**
 * @brief finalize xxhash32 context
 * @details call this function after calling xxhash32_update(), also destroy xxhash32 context
 * @param[in] ctx xxhash32 context
 * @return xxhash32 value
 */
uint32_t xxhash32_final(xxhash32_context_t* ctx);

/**
 * @brief calculate xxhash32 value
 * @param[in] input input data
 * @param[in] length input data length
 * @param[in] seed seed value
 * @return xxhash32 value
 */
uint32_t xxhash32_hash_with_seed(const void* input, uint64_t length, uint32_t seed);

/**
 * @brief calculate xxhash32 value with seed 0
 * @param[in] i input data
 * @param[in] l input data length
 * @return xxhash32 value
 */
#define xxhash32_hash(i, l) xxhash32_hash_with_seed(i, l, 0)

#endif
