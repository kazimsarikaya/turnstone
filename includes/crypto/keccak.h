/**
 * @file keccak.h
 * @brief Keccak (SHA-3) implementation
 *
 * This file provides the interface for the Keccak hash function, which is the basis for the SHA-3 family of hash functions.
 * It includes functions for various SHA-3 output sizes (224, 256, 384, 512) and the SHAKE (Secure Hash Algorithm KECCAK) extendable-output functions.
 *
 * The Keccak algorithm is a sponge construction that provides a flexible and secure hashing mechanism.
 * SHA-3 is a standardized cryptographic hash function developed by NIST as a successor to SHA-2.
 * SHAKE functions allow for variable-length output, making them suitable for applications like key derivation and stream ciphers.
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

/**
 * @brief Output size of SHA3-224 in bytes.
 */
#define SHA3_224_OUTPUT_SIZE 28

/**
 * @brief Output size of SHA3-256 in bytes.
 */
#define SHA3_256_OUTPUT_SIZE 32

/**
 * @brief Output size of SHA3-384 in bytes.
 */
#define SHA3_384_OUTPUT_SIZE 48

/**
 * @brief Output size of SHA3-512 in bytes.
 */
#define SHA3_512_OUTPUT_SIZE 64

/**
 * @brief Context structure for SHA3-224.
 *
 * This structure holds the internal state of the SHA3-224 hashing process.
 */
typedef struct sha3_224_ctx_t sha3_224_ctx_t;

/**
 * @brief Context structure for SHA3-256.
 *
 * This structure holds the internal state of the SHA3-256 hashing process.
 */
typedef struct sha3_256_ctx_t sha3_256_ctx_t;

/**
 * @brief Context structure for SHA3-384.
 *
 * This structure holds the internal state of the SHA3-384 hashing process.
 */
typedef struct sha3_384_ctx_t sha3_384_ctx_t;

/**
 * @brief Context structure for SHA3-512.
 *
 * This structure holds the internal state of the SHA3-512 hashing process.
 */
typedef struct sha3_512_ctx_t sha3_512_ctx_t;

/**
 * @brief Context structure for SHAKE128.
 *
 * This structure holds the internal state of the SHAKE128 hashing process.
 */
typedef struct shake128_ctx_t shake128_ctx_t;

/**
 * @brief Context structure for SHAKE256.
 *
 * This structure holds the internal state of the SHAKE256 hashing process.
 */
typedef struct shake256_ctx_t shake256_ctx_t;

/**
 * @brief Initializes a SHA3-224 hashing context.
 * @return A pointer to the initialized SHA3-224 context, or NULL on failure.
 */
sha3_224_ctx_t* sha3_224_init(void);

/**
 * @brief Updates the SHA3-224 context with data.
 * @param ctx Pointer to the SHA3-224 context.
 * @param data Pointer to the data to be hashed.
 * @param len Length of the data in bytes.
 * @return 0 on success, -1 on failure.
 */
int8_t sha3_224_update(sha3_224_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalizes the SHA3-224 hash computation and returns the digest.
 * @param ctx Pointer to the SHA3-224 context.
 * @return A pointer to the computed hash digest (SHA3_224_OUTPUT_SIZE bytes), or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha3_224_final(sha3_224_ctx_t* ctx);

/**
 * @brief Computes the SHA3-224 hash of a given data buffer in a single call.
 * @param data Pointer to the data to be hashed.
 * @param length Length of the data in bytes.
 * @return A pointer to the computed hash digest (SHA3_224_OUTPUT_SIZE bytes), or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha3_224_hash(const uint8_t* data, size_t length);

/**
 * @brief Clones a SHA3-224 hashing context.
 * @param ctx Pointer to the SHA3-224 context to clone.
 * @return A pointer to the cloned SHA3-224 context, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
sha3_224_ctx_t* sha3_224_clone(const sha3_224_ctx_t* ctx);

/**
 * @brief Initializes a SHA3-256 hashing context.
 * @return A pointer to the initialized SHA3-256 context, or NULL on failure.
 */
sha3_256_ctx_t* sha3_256_init(void);

/**
 * @brief Updates the SHA3-256 context with data.
 * @param ctx Pointer to the SHA3-256 context.
 * @param data Pointer to the data to be hashed.
 * @param len Length of the data in bytes.
 * @return 0 on success, -1 on failure.
 */
int8_t sha3_256_update(sha3_256_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalizes the SHA3-256 hash computation and returns the digest.
 * @param ctx Pointer to the SHA3-256 context.
 * @return A pointer to the computed hash digest (SHA3_256_OUTPUT_SIZE bytes), or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha3_256_final(sha3_256_ctx_t* ctx);

/**
 * @brief Computes the SHA3-256 hash of a given data buffer in a single call.
 * @param data Pointer to the data to be hashed.
 * @param length Length of the data in bytes.
 * @return A pointer to the computed hash digest (SHA3_256_OUTPUT_SIZE bytes), or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha3_256_hash(const uint8_t* data, size_t length);

/**
 * @brief Clones a SHA3-256 hashing context.
 * @param ctx Pointer to the SHA3-256 context to clone.
 * @return A pointer to the cloned SHA3-256 context, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
sha3_256_ctx_t* sha3_256_clone(const sha3_256_ctx_t* ctx);

/**
 * @brief Initializes a SHA3-384 hashing context.
 * @return A pointer to the initialized SHA3-384 context, or NULL on failure.
 */
sha3_384_ctx_t* sha3_384_init(void);

/**
 * @brief Updates the SHA3-384 context with data.
 * @param ctx Pointer to the SHA3-384 context.
 * @param data Pointer to the data to be hashed.
 * @param len Length of the data in bytes.
 * @return 0 on success, -1 on failure.
 */
int8_t sha3_384_update(sha3_384_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalizes the SHA3-384 hash computation and returns the digest.
 * @param ctx Pointer to the SHA3-384 context.
 * @return A pointer to the computed hash digest (SHA3_384_OUTPUT_SIZE bytes), or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha3_384_final(sha3_384_ctx_t* ctx);

/**
 * @brief Computes the SHA3-384 hash of a given data buffer in a single call.
 * @param data Pointer to the data to be hashed.
 * @param length Length of the data in bytes.
 * @return A pointer to the computed hash digest (SHA3_384_OUTPUT_SIZE bytes), or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha3_384_hash(const uint8_t* data, size_t length);

/**
 * @brief Clones a SHA3-384 hashing context.
 * @param ctx Pointer to the SHA3-384 context to clone.
 * @return A pointer to the cloned SHA3-384 context, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
sha3_384_ctx_t* sha3_384_clone(const sha3_384_ctx_t* ctx);

/**
 * @brief Initializes a SHA3-512 hashing context.
 * @return A pointer to the initialized SHA3-512 context, or NULL on failure.
 */
sha3_512_ctx_t* sha3_512_init(void);

/**
 * @brief Updates the SHA3-512 context with data.
 * @param ctx Pointer to the SHA3-512 context.
 * @param data Pointer to the data to be hashed.
 * @param len Length of the data in bytes.
 * @return 0 on success, -1 on failure.
 */
int8_t sha3_512_update(sha3_512_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalizes the SHA3-512 hash computation and returns the digest.
 * @param ctx Pointer to the SHA3-512 context.
 * @return A pointer to the computed hash digest (SHA3_512_OUTPUT_SIZE bytes), or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha3_512_final(sha3_512_ctx_t* ctx);

/**
 * @brief Computes the SHA3-512 hash of a given data buffer in a single call.
 * @param data Pointer to the data to be hashed.
 * @param length Length of the data in bytes.
 * @return A pointer to the computed hash digest (SHA3_512_OUTPUT_SIZE bytes), or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha3_512_hash(const uint8_t* data, size_t length);

/**
 * @brief Clones a SHA3-512 hashing context.
 * @param ctx Pointer to the SHA3-512 context to clone.
 * @return A pointer to the cloned SHA3-512 context, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
sha3_512_ctx_t* sha3_512_clone(const sha3_512_ctx_t* ctx);

/**
 * @brief Initializes a SHAKE128 extendable-output function context.
 * @return A pointer to the initialized SHAKE128 context, or NULL on failure.
 */
shake128_ctx_t* shake128_init(void);

/**
 * @brief Updates the SHAKE128 context with data.
 * @param ctx Pointer to the SHAKE128 context.
 * @param data Pointer to the data to be processed.
 * @param len Length of the data in bytes.
 * @return 0 on success, -1 on failure.
 */
int8_t shake128_update(shake128_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalizes the SHAKE128 computation and returns an output of the specified length.
 * @param ctx Pointer to the SHAKE128 context.
 * @param output_len The desired length of the output digest in bytes.
 * @return A pointer to the computed output digest, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* shake128_final(shake128_ctx_t* ctx, size_t output_len);

/**
 * @brief Generates the next block of output from a SHAKE128 context.
 *
 * This function can be called multiple times after `shake128_init` or `shake128_update`
 * to generate an arbitrary amount of output data. It is for XOF (eXtendable Output Function) usage, allowing the caller to specify how much output they want.
 *
 * @param ctx Pointer to the SHAKE128 context.
 * @param output_len The desired length of the output block in bytes.
 * @return A pointer to the computed output digest, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* shake128_next(shake128_ctx_t* ctx, size_t output_len);

/**
 * @brief Computes the SHAKE128 hash of a given data buffer and produces an output of the specified length.
 * @param data Pointer to the data to be hashed.
 * @param data_len Length of the data in bytes.
 * @param output_len The desired length of the output digest in bytes.
 * @return A pointer to the computed output digest, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* shake128_hash(const uint8_t* data, size_t data_len, size_t output_len);

/**
 * @brief Clones a SHAKE128 hashing context.
 * @param ctx Pointer to the SHAKE128 context to clone.
 * @return A pointer to the cloned SHAKE128 context, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
shake128_ctx_t* shake128_clone(const shake128_ctx_t* ctx);

/**
 * @brief Initializes a SHAKE256 extendable-output function context.
 * @return A pointer to the initialized SHAKE256 context, or NULL on failure.
 */
shake256_ctx_t* shake256_init(void);

/**
 * @brief Updates the SHAKE256 context with data.
 * @param ctx Pointer to the SHAKE256 context.
 * @param data Pointer to the data to be processed.
 * @param len Length of the data in bytes.
 * @return 0 on success, -1 on failure.
 */
int8_t shake256_update(shake256_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalizes the SHAKE256 computation and returns an output of the specified length.
 * @param ctx Pointer to the SHAKE256 context.
 * @param output_len The desired length of the output digest in bytes.
 * @return A pointer to the computed output digest, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* shake256_final(shake256_ctx_t* ctx, size_t output_len);

/**
 * @brief Generates the next block of output from a SHAKE256 context.
 *
 * This function can be called multiple times after `shake256_init` or `shake256_update`
 * to generate an arbitrary amount of output data. It is for XOF (eXtendable Output Function) use cases where the output length is not predetermined.
 *
 * @param ctx Pointer to the SHAKE256 context.
 * @param output_len The desired length of the output block in bytes.
 * @return A pointer to the computed output digest, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* shake256_next(shake256_ctx_t* ctx, size_t output_len);

/**
 * @brief Computes the SHAKE256 hash of a given data buffer and produces an output of the specified length.
 * @param data Pointer to the data to be hashed.
 * @param data_len Length of the data in bytes.
 * @param output_len The desired length of the output digest in bytes.
 * @return A pointer to the computed output digest, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
uint8_t* shake256_hash(const uint8_t* data, size_t data_len, size_t output_len);

/**
 * @brief Clones a SHAKE256 hashing context.
 * @param ctx Pointer to the SHAKE256 context to clone.
 * @return A pointer to the cloned SHAKE256 context, or NULL on failure. The caller is responsible for freeing the returned pointer.
 */
shake256_ctx_t* shake256_clone(const shake256_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* ___KECCAK_H */
