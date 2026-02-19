/**
 * @file sha2.h
 * @brief SHA-2 (Secure Hash Algorithm 2) family of cryptographic hash functions.
 *
 * This file provides the interface for the SHA-2 family of cryptographic hash functions,
 * including SHA-224, SHA-256, SHA-384, and SHA-512. These functions are widely used
 * for data integrity verification, digital signatures, and other cryptographic applications.
 * The implementation supports incremental hashing (update and final) as well as
 * one-shot hashing. It also includes HMAC (Hash-based Message Authentication Code)
 * functionality for keyed hashing.
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

/**
 * @brief Output size of SHA-256 in bytes.
 */
#define SHA256_OUTPUT_SIZE  32

/**
 * @brief Block size of SHA-256 in bytes.
 */
#define SHA256_BLOCK_SIZE   64

/**
 * @brief State size of SHA-256 in words (each word is 4 bytes).
 */
#define SHA256_STATE_SIZE    8

/**
 * @brief Context structure for SHA-256 hashing.
 *
 * This structure holds the internal state required for incremental SHA-256
 * hash computations. It includes the message buffer, the number of bytes
 * processed so far, and the internal hash state variables.
 */
typedef struct sha256_ctx_t sha256_ctx_t;

/**
 * @brief Initializes a SHA-256 hashing context.
 *
 * Allocates and initializes a SHA-256 context structure. The context is set
 * to its initial state, ready to process input data.
 *
 * @return A pointer to the initialized SHA-256 context, or NULL if memory allocation fails.
 */
sha256_ctx_t* sha256_init(void);

/**
 * @brief Updates the SHA-256 context with a chunk of data.
 *
 * Processes a block of input data and updates the internal state of the SHA-256
 * context. This function can be called multiple times to hash data incrementally.
 *
 * @param ctx Pointer to the SHA-256 context.
 * @param data Pointer to the data buffer to be hashed.
 * @param len The number of bytes in the data buffer.
 * @return 0 on success, -1 on failure (e.g., NULL context).
 */
int8_t sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalizes the SHA-256 hash computation and returns the digest.
 *
 * Computes the final SHA-256 hash digest based on the data processed so far.
 * The internal state of the context is reset after this call, but the context
 * structure itself is not freed. The returned digest is a newly allocated buffer
 * that must be freed by the caller.
 *
 * @param ctx Pointer to the SHA-256 context.
 * @return A pointer to the computed SHA-256 digest (32 bytes), or NULL if an error occurs
 *         (e.g., NULL context, memory allocation failure). The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha256_final(sha256_ctx_t* ctx);

/**
 * @brief Computes the SHA-256 hash of a data buffer in a single call.
 *
 * This function computes the SHA-256 hash of the entire input data buffer at once.
 * It is equivalent to calling `sha256_init`, `sha256_update` with the data, and `sha256_final`.
 * The returned digest is a newly allocated buffer that must be freed by the caller.
 *
 * @param data Pointer to the data buffer to be hashed.
 * @param length The length of the data buffer in bytes.
 * @return A pointer to the computed SHA-256 digest (32 bytes), or NULL if an error occurs
 *         (e.g., memory allocation failure). The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha256_hash(const uint8_t* data, size_t length);

/**
 * @brief Clones a SHA-256 hashing context.
 *
 * Creates a copy of an existing SHA-256 context, including its current internal state.
 * This is useful for scenarios where you need to compute multiple hashes from the same
 * initial state or branch off computations. The returned context must be freed by the caller.
 *
 * @param ctx Pointer to the SHA-256 context to clone.
 * @return A pointer to the cloned SHA-256 context, or NULL if an error occurs
 *         (e.g., NULL context, memory allocation failure). The caller is responsible for freeing the returned pointer.
 */
sha256_ctx_t* sha256_clone(sha256_ctx_t* ctx);

/**
 * @brief Computes the HMAC-SHA-256 of a message.
 *
 * Calculates the Hash-based Message Authentication Code (HMAC) using SHA-256.
 * HMAC provides message authentication using a secret key.
 * The returned HMAC is a newly allocated buffer that must be freed by the caller.
 *
 * @param key Pointer to the secret key.
 * @param key_len The length of the secret key in bytes.
 * @param data Pointer to the message data to be authenticated.
 * @param data_len The length of the message data in bytes.
 * @return A pointer to the computed HMAC-SHA-256 digest (32 bytes), or NULL if an error occurs
 *         (e.g., memory allocation failure). The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha256_hmac(const uint8_t* key, size_t key_len,
                     const uint8_t* data, size_t data_len);


/**
 * @brief Output size of SHA-224 in bytes.
 */
#define SHA224_OUTPUT_SIZE  28

/**
 * @brief Context structure for SHA-224 hashing.
 *
 * This structure holds the internal state required for incremental SHA-224
 * hash computations. It is functionally identical to `sha256_ctx_t` but operates
 * with SHA-224 parameters.
 */
typedef struct sha224_ctx_t sha224_ctx_t;

/**
 * @brief Initializes a SHA-224 hashing context.
 *
 * Allocates and initializes a SHA-224 context structure.
 *
 * @return A pointer to the initialized SHA-224 context, or NULL if memory allocation fails.
 */
sha224_ctx_t* sha224_init(void);

/**
 * @brief Updates the SHA-224 context with a chunk of data.
 *
 * Processes a block of input data and updates the internal state of the SHA-224
 * context.
 *
 * @param ctx Pointer to the SHA-224 context.
 * @param data Pointer to the data buffer to be hashed.
 * @param len The number of bytes in the data buffer.
 * @return 0 on success, -1 on failure (e.g., NULL context).
 */
int8_t sha224_update(sha224_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalizes the SHA-224 hash computation and returns the digest.
 *
 * Computes the final SHA-224 hash digest. The returned digest is a newly allocated buffer
 * that must be freed by the caller.
 *
 * @param ctx Pointer to the SHA-224 context.
 * @return A pointer to the computed SHA-224 digest (28 bytes), or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha224_final(sha224_ctx_t* ctx);

/**
 * @brief Computes the SHA-224 hash of a data buffer in a single call.
 *
 * Computes the SHA-224 hash of the entire input data buffer at once.
 * The returned digest is a newly allocated buffer that must be freed by the caller.
 *
 * @param data Pointer to the data buffer to be hashed.
 * @param length The length of the data buffer in bytes.
 * @return A pointer to the computed SHA-224 digest (28 bytes), or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha224_hash(const uint8_t* data, size_t length);

/**
 * @brief Clones a SHA-224 hashing context.
 *
 * Creates a copy of an existing SHA-224 context. The returned context must be freed by the caller.
 *
 * @param ctx Pointer to the SHA-224 context to clone.
 * @return A pointer to the cloned SHA-224 context, or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
sha224_ctx_t* sha224_clone(sha224_ctx_t* ctx);

/**
 * @brief Computes the HMAC-SHA-224 of a message.
 *
 * Calculates the HMAC using SHA-224.
 * The returned HMAC is a newly allocated buffer that must be freed by the caller.
 *
 * @param key Pointer to the secret key.
 * @param key_len The length of the secret key in bytes.
 * @param data Pointer to the message data to be authenticated.
 * @param data_len The length of the message data in bytes.
 * @return A pointer to the computed HMAC-SHA-224 digest (28 bytes), or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha224_hmac(const uint8_t* key, size_t key_len,
                     const uint8_t* data, size_t data_len);

/**
 * @brief Output size of SHA-512 in bytes.
 */
#define SHA512_OUTPUT_SIZE   64
/**
 * @brief Block size of SHA-512 in bytes.
 */
#define SHA512_BLOCK_SIZE   128
/**
 * @brief State size of SHA-512 in words (each word is 8 bytes).
 */
#define SHA512_STATE_SIZE     8

/**
 * @brief Context structure for SHA-512 hashing.
 *
 * This structure holds the internal state required for incremental SHA-512
 * hash computations. It uses 64-bit words for internal state and calculations.
 */
typedef struct sha512_ctx_t sha512_ctx_t;

/**
 * @brief Initializes a SHA-512 hashing context.
 *
 * Allocates and initializes a SHA-512 context structure.
 *
 * @return A pointer to the initialized SHA-512 context, or NULL if memory allocation fails.
 */
sha512_ctx_t* sha512_init(void);

/**
 * @brief Updates the SHA-512 context with a chunk of data.
 *
 * Processes a block of input data and updates the internal state of the SHA-512
 * context.
 *
 * @param ctx Pointer to the SHA-512 context.
 * @param data Pointer to the data buffer to be hashed.
 * @param len The number of bytes in the data buffer.
 * @return 0 on success, -1 on failure (e.g., NULL context).
 */
int8_t sha512_update(sha512_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalizes the SHA-512 hash computation and returns the digest.
 *
 * Computes the final SHA-512 hash digest. The returned digest is a newly allocated buffer
 * that must be freed by the caller.
 *
 * @param ctx Pointer to the SHA-512 context.
 * @return A pointer to the computed SHA-512 digest (64 bytes), or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha512_final(sha512_ctx_t* ctx);

/**
 * @brief Computes the SHA-512 hash of a data buffer in a single call.
 *
 * Computes the SHA-512 hash of the entire input data buffer at once.
 * The returned digest is a newly allocated buffer that must be freed by the caller.
 *
 * @param data Pointer to the data buffer to be hashed.
 * @param length The length of the data buffer in bytes.
 * @return A pointer to the computed SHA-512 digest (64 bytes), or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha512_hash(const uint8_t* data, size_t length);

/**
 * @brief Clones a SHA-512 hashing context.
 *
 * Creates a copy of an existing SHA-512 context. The returned context must be freed by the caller.
 *
 * @param ctx Pointer to the SHA-512 context to clone.
 * @return A pointer to the cloned SHA-512 context, or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
sha512_ctx_t* sha512_clone(sha512_ctx_t* ctx);

/**
 * @brief Computes the HMAC-SHA-512 of a message.
 *
 * Calculates the HMAC using SHA-512.
 * The returned HMAC is a newly allocated buffer that must be freed by the caller.
 *
 * @param key Pointer to the secret key.
 * @param key_len The length of the secret key in bytes.
 * @param data Pointer to the message data to be authenticated.
 * @param data_len The length of the message data in bytes.
 * @return A pointer to the computed HMAC-SHA-512 digest (64 bytes), or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha512_hmac(const uint8_t* key, size_t key_len,
                     const uint8_t* data, size_t data_len);

/**
 * @brief Output size of SHA-384 in bytes.
 */
#define SHA384_OUTPUT_SIZE  48

/**
 * @brief Context structure for SHA-384 hashing.
 *
 * This structure holds the internal state required for incremental SHA-384
 * hash computations. It uses the same underlying implementation as SHA-512
 * but operates with SHA-384 parameters.
 */
typedef struct sha384_ctx_t sha384_ctx_t; // SHA-384 uses the same context structure as SHA-512 internally

/**
 * @brief Initializes a SHA-384 hashing context.
 *
 * Allocates and initializes a SHA-384 context structure.
 *
 * @return A pointer to the initialized SHA-384 context, or NULL if memory allocation fails.
 */
sha384_ctx_t* sha384_init(void);

/**
 * @brief Updates the SHA-384 context with a chunk of data.
 *
 * Processes a block of input data and updates the internal state of the SHA-384
 * context.
 *
 * @param ctx Pointer to the SHA-384 context.
 * @param data Pointer to the data buffer to be hashed.
 * @param len The number of bytes in the data buffer.
 * @return 0 on success, -1 on failure (e.g., NULL context).
 */
int8_t sha384_update(sha384_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalizes the SHA-384 hash computation and returns the digest.
 *
 * Computes the final SHA-384 hash digest. The returned digest is a newly allocated buffer
 * that must be freed by the caller.
 *
 * @param ctx Pointer to the SHA-384 context.
 * @return A pointer to the computed SHA-384 digest (48 bytes), or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha384_final(sha384_ctx_t* ctx);

/**
 * @brief Computes the SHA-384 hash of a data buffer in a single call.
 *
 * Computes the SHA-384 hash of the entire input data buffer at once.
 * The returned digest is a newly allocated buffer that must be freed by the caller.
 *
 * @param data Pointer to the data buffer to be hashed.
 * @param length The length of the data buffer in bytes.
 * @return A pointer to the computed SHA-384 digest (48 bytes), or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha384_hash(const uint8_t* data, size_t length);

/**
 * @brief Clones a SHA-384 hashing context.
 *
 * Creates a copy of an existing SHA-384 context. The returned context must be freed by the caller.
 *
 * @param ctx Pointer to the SHA-384 context to clone.
 * @return A pointer to the cloned SHA-384 context, or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
sha384_ctx_t* sha384_clone(sha384_ctx_t* ctx);

/**
 * @brief Computes the HMAC-SHA-384 of a message.
 *
 * Calculates the HMAC using SHA-384.
 * The returned HMAC is a newly allocated buffer that must be freed by the caller.
 *
 * @param key Pointer to the secret key.
 * @param key_len The length of the secret key in bytes.
 * @param data Pointer to the message data to be authenticated.
 * @param data_len The length of the message data in bytes.
 * @return A pointer to the computed HMAC-SHA-384 digest (48 bytes), or NULL if an error occurs. The caller is responsible for freeing the returned pointer.
 */
uint8_t* sha384_hmac(const uint8_t* key, size_t key_len,
                     const uint8_t* data, size_t data_len);

#ifdef __cplusplus
}
#endif

#endif
