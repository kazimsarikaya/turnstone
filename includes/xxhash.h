/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___XXHASH_H
#define ___XXHASH_H 0

#include <types.h>

typedef void * xxhash64_context_t;

xxhash64_context_t xxhash64_init(uint64_t seed);
int8_t             xxhash64_update(xxhash64_context_t ctx, const void* input, uint64_t length);
uint64_t           xxhash64_final(xxhash64_context_t ctx);

uint64_t xxhash64_hash_with_seed(const void* input, uint64_t length, uint64_t seed);
#define xxhash64_hash(i, l) xxhash64_hash_with_seed(i, l, 0)

typedef void * xxhash32_context_t;

xxhash32_context_t xxhash32_init(uint32_t seed);
int8_t             xxhash32_update(xxhash32_context_t ctx, const void* input, uint64_t length);
uint32_t           xxhash32_final(xxhash32_context_t ctx);

uint32_t xxhash32_hash_with_seed(const void* input, uint64_t length, uint32_t seed);
#define xxhash32_hash(i, l) xxhash32_hash_with_seed(i, l, 0)


#endif
