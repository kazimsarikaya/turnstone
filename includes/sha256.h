/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___SHA256_H
#define ___SHA256_H 0

#include <types.h>

#define SHA256_OUTPUT_SIZE  32
#define SHA256_BLOCK_SIZE   64
#define SHA256_STATE_SIZE    8

typedef void* sha256_ctx_t;

sha256_ctx_t sha256_init();
int8_t sha256_update(sha256_ctx_t ctx, const uint8_t* data, size_t len);
uint8_t* sha256_final(sha256_ctx_t ctx);
uint8_t* sha256_hash(uint8_t* data, size_t length);

#endif
