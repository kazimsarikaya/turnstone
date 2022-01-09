#ifndef ___XXHASH_H
#define ___XXHASH_H 0

#include <types.h>

typedef void* xxhash_context_t;

xxhash_context_t xxhash64_init(uint64_t seed);
int8_t xxhash64_update(xxhash_context_t ctx, const void* input, uint64_t length);
uint64_t xxhash64_final(xxhash_context_t ctx);

uint64_t xxhash64_hash_with_seed(const void* input, uint64_t length, uint64_t seed);
#define xxhash64_hash(i, l) xxhash64_hash_with_seed(i, l, 0)


#endif
