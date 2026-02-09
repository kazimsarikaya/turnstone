/**
 * @file keccak.h
 * @brief Keccak (SHA-3) implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___KECCAK_H
#define ___KECCAK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

#define SHA3_224_OUTPUT_SIZE 28
#define SHA3_256_OUTPUT_SIZE 32
#define SHA3_384_OUTPUT_SIZE 48
#define SHA3_512_OUTPUT_SIZE 64

typedef struct sha3_224_ctx_t sha3_224_ctx_t;
typedef struct sha3_256_ctx_t sha3_256_ctx_t;
typedef struct sha3_384_ctx_t sha3_384_ctx_t;
typedef struct sha3_512_ctx_t sha3_512_ctx_t;
typedef struct shake128_ctx_t shake128_ctx_t;
typedef struct shake256_ctx_t shake256_ctx_t;

sha3_224_ctx_t* sha3_224_init(void);
int8_t          sha3_224_update(sha3_224_ctx_t* ctx, const uint8_t* data, size_t len);
uint8_t*        sha3_224_final(sha3_224_ctx_t* ctx);
uint8_t*        sha3_224_hash(const uint8_t* data, size_t length);
sha3_224_ctx_t* sha3_224_clone(const sha3_224_ctx_t* ctx);

sha3_256_ctx_t* sha3_256_init(void);
int8_t          sha3_256_update(sha3_256_ctx_t* ctx, const uint8_t* data, size_t len);
uint8_t*        sha3_256_final(sha3_256_ctx_t* ctx);
uint8_t*        sha3_256_hash(const uint8_t* data, size_t length);
sha3_256_ctx_t* sha3_256_clone(const sha3_256_ctx_t* ctx);

sha3_384_ctx_t* sha3_384_init(void);
int8_t          sha3_384_update(sha3_384_ctx_t* ctx, const uint8_t* data, size_t len);
uint8_t*        sha3_384_final(sha3_384_ctx_t* ctx);
uint8_t*        sha3_384_hash(const uint8_t* data, size_t length);
sha3_384_ctx_t* sha3_384_clone(const sha3_384_ctx_t* ctx);

sha3_512_ctx_t* sha3_512_init(void);
int8_t          sha3_512_update(sha3_512_ctx_t* ctx, const uint8_t* data, size_t len);
uint8_t*        sha3_512_final(sha3_512_ctx_t* ctx);
uint8_t*        sha3_512_hash(const uint8_t* data, size_t length);
sha3_512_ctx_t* sha3_512_clone(const sha3_512_ctx_t* ctx);

shake128_ctx_t* shake128_init(void);
int8_t          shake128_update(shake128_ctx_t* ctx, const uint8_t* data, size_t len);
uint8_t*        shake128_final(shake128_ctx_t* ctx, size_t output_len);
uint8_t*        shake128_hash(const uint8_t* data, size_t data_len, size_t output_len);
shake128_ctx_t* shake128_clone(const shake128_ctx_t* ctx);

shake256_ctx_t* shake256_init(void);
int8_t          shake256_update(shake256_ctx_t* ctx, const uint8_t* data, size_t len);
uint8_t*        shake256_final(shake256_ctx_t* ctx, size_t output_len);
uint8_t*        shake256_hash(const uint8_t* data, size_t data_len, size_t output_len);
shake256_ctx_t* shake256_clone(const shake256_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* ___KECCAK_H */
