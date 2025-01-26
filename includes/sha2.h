/**
 * @file sha2.h
 * @brief SHA-2 (Secure Hash Algorithm 2) family of cryptographic hash functions.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___SHA_H
#define ___SHA_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHA256_OUTPUT_SIZE  32
#define SHA256_BLOCK_SIZE   64
#define SHA256_STATE_SIZE    8

typedef struct sha256_ctx_t sha256_ctx_t;

sha256_ctx_t* sha256_init(void);
int8_t        sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len);
uint8_t*      sha256_final(sha256_ctx_t* ctx);
uint8_t*      sha256_hash(uint8_t* data, size_t length);


#define SHA224_OUTPUT_SIZE  28

typedef struct sha256_ctx_t sha224_ctx_t;

sha224_ctx_t* sha224_init(void);
int8_t        sha224_update(sha224_ctx_t* ctx, const uint8_t* data, size_t len);
uint8_t*      sha224_final(sha224_ctx_t* ctx);
uint8_t*      sha224_hash(uint8_t* data, size_t length);

#define SHA512_OUTPUT_SIZE   64
#define SHA512_BLOCK_SIZE   128
#define SHA512_STATE_SIZE     8

typedef struct sha512_ctx_t sha512_ctx_t;

sha512_ctx_t* sha512_init(void);
int8_t        sha512_update(sha512_ctx_t* ctx, const uint8_t* data, size_t len);
uint8_t*      sha512_final(sha512_ctx_t* ctx);
uint8_t*      sha512_hash(uint8_t* data, size_t length);

#define SHA384_OUTPUT_SIZE  48

typedef struct sha512_ctx_t sha384_ctx_t;

sha384_ctx_t* sha384_init(void);
int8_t        sha384_update(sha384_ctx_t* ctx, const uint8_t* data, size_t len);
uint8_t*      sha384_final(sha384_ctx_t* ctx);
uint8_t*      sha384_hash(uint8_t* data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
