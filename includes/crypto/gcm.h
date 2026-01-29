/**
 * @file gcm.h
 * @brief Galois/Counter Mode (GCM) implementation header file
 *
 * This header defines the interface for the Galois/Counter Mode (GCM)
 * cryptographic mode. GCM is an authenticated encryption mode that provides
 * both data confidentiality and data authenticity. It is commonly used
 * with the Advanced Encryption Standard (AES) algorithm.
 *
 * This file declares the necessary structures and functions for initializing,
 * configuring, and performing encryption/decryption operations using GCM.
 * It also defines error codes, such as GCM_AUTH_FAILURE, to indicate
 * authentication failures during decryption.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___GCM_HEADER_H
#define ___GCM_HEADER_H

#define GCM_AUTH_FAILURE    0x55555555 // Error code indicating authentication failure

#include <crypto/aes.h> // Includes the AES context structure and related definitions

#ifdef __cplusplus
extern "C" {
#endif

#define GCM_TAG_LENGTH_BYTES 16 ///< Standard length of the GCM authentication tag in bytes

/**
 * @brief Structure to hold the GCM context.
 *
 * This structure stores the internal state required for GCM operations,
 * including the encryption mode, lengths of data processed, precomputed
 * GCM multiplication tables (HL and HH), the base counter block,
 * the current counter block (y), a temporary buffer, and the AES context.
 */
typedef struct gcm_context_t {
    int32_t       mode; ///< Current mode of operation (e.g., encrypt/decrypt).
    uint64_t      len; ///< Total length of data processed so far (in bits).
    uint64_t      add_len; ///< Total length of additional authenticated data (AAD) processed (in bits).
    uint64_t      HL[16]; ///< Precomputed table for GCM multiplication (H * 2^i).
    uint64_t      HH[16]; ///< Precomputed table for GCM multiplication (H * 2^i, for higher bits).
    uint8_t       base_ectr[16]; ///< The initial counter block derived from the IV.
    uint8_t       y[16]; ///< The current counter block used for encryption/decryption.
    uint8_t       buf[16]; ///< A temporary buffer for block processing.
    aes_context_t aes_ctx; ///< The AES context for block cipher operations.
} gcm_context_t;


/**
 * @brief Initializes the GCM module.
 *
 * This function should be called once before any other GCM functions are used.
 * It performs any necessary global initialization for the GCM implementation.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int gcm_initialize(void);

/**
 * @brief Sets the encryption key for GCM operations.
 *
 * This function configures the GCM context with the provided encryption key.
 * The key is used for both confidentiality and authenticity.
 *
 * @param ctx Pointer to the GCM context structure.
 * @param key Pointer to the encryption key.
 * @param keysize The size of the key in bytes (e.g., 16 for AES-128, 24 for AES-192, 32 for AES-256).
 * @return 0 on success, or a negative error code on failure.
 */
int gcm_setkey(gcm_context_t* ctx, const uint8_t* key, const uint32_t keysize);

/**
 * @brief Performs GCM encryption or decryption and generates/verifies the authentication tag.
 *
 * This is a convenience function that encapsulates the entire GCM process:
 * initialization, processing of additional authenticated data (AAD),
 * processing of input data (plaintext for encryption, ciphertext for decryption),
 * and finalization to generate or verify the authentication tag.
 *
 * @param ctx Pointer to the GCM context structure.
 * @param mode The operation mode: 0 for encryption, 1 for decryption.
 * @param iv Pointer to the Initialization Vector (IV) or nonce. Must be unique for each encryption with the same key.
 * @param iv_len Length of the IV in bytes. Typically 12 bytes (96 bits) for GCM.
 * @param add Pointer to the Additional Authenticated Data (AAD). This data is authenticated but not encrypted.
 * @param add_len Length of the AAD in bytes.
 * @param input Pointer to the input data (plaintext for encryption, ciphertext for decryption).
 * @param output Pointer to the buffer where the output data will be stored (ciphertext for encryption, plaintext for decryption).
 * @param length The length of the input/output data in bytes.
 * @param tag Pointer to the buffer where the authentication tag will be stored (during encryption) or from which it will be read (during decryption).
 * @param tag_len The length of the authentication tag in bytes. Typically 16 bytes (128 bits).
 * @return 0 on success, GCM_AUTH_FAILURE if authentication fails during decryption, or a negative error code on other failures.
 */
int gcm_crypt_and_tag(gcm_context_t* ctx, int32_t mode, const uint8_t* iv, size_t iv_len, const uint8_t* add, size_t add_len, const uint8_t * input, uint8_t* output, size_t length, uint8_t * tag, size_t tag_len);

/**
 * @brief Performs GCM authenticated decryption.
 *
 * This function decrypts ciphertext and verifies the authentication tag.
 * It is an alternative to `gcm_crypt_and_tag` for decryption, providing a more
 * granular control flow if needed.
 *
 * @param ctx Pointer to the GCM context structure.
 * @param iv Pointer to the Initialization Vector (IV) or nonce used during encryption.
 * @param iv_len Length of the IV in bytes.
 * @param add Pointer to the Additional Authenticated Data (AAD) used during encryption.
 * @param add_len Length of the AAD in bytes.
 * @param input Pointer to the ciphertext.
 * @param output Pointer to the buffer where the decrypted plaintext will be stored.
 * @param length The length of the ciphertext in bytes.
 * @param tag Pointer to the authentication tag to be verified.
 * @param tag_len The length of the authentication tag in bytes.
 * @return 0 on successful decryption and authentication, GCM_AUTH_FAILURE if authentication fails, or a negative error code on other failures.
 */
int gcm_auth_decrypt(gcm_context_t* ctx, const uint8_t* iv, size_t iv_len, const uint8_t* add, size_t add_len, const uint8_t* input, uint8_t* output, size_t length, const uint8_t* tag, size_t tag_len);

/**
 * @brief Starts a GCM encryption or decryption process.
 *
 * This function initializes the GCM state for a new operation, processing
 * the IV and any Additional Authenticated Data (AAD). It prepares the context
 * for subsequent calls to `gcm_update` and `gcm_finish`.
 *
 * @param ctx Pointer to the GCM context structure.
 * @param mode The operation mode: 0 for encryption, 1 for decryption.
 * @param iv Pointer to the Initialization Vector (IV) or nonce.
 * @param iv_len Length of the IV in bytes.
 * @param add Pointer to the Additional Authenticated Data (AAD).
 * @param add_len Length of the AAD in bytes.
 * @return 0 on success, or a negative error code on failure.
 */
int gcm_start(gcm_context_t* ctx, int32_t mode, const uint8_t* iv, size_t iv_len, const uint8_t* add, size_t add_len);

/**
 * @brief Updates the GCM state with input data.
 *
 * This function processes a chunk of input data (either plaintext during encryption
 * or ciphertext during decryption) and updates the GCM state. The processed data
 * is written to the output buffer. This function can be called multiple times
 * for large data inputs.
 *
 * @param ctx Pointer to the GCM context structure.
 * @param length The length of the input data in bytes.
 * @param input Pointer to the input data buffer.
 * @param output Pointer to the output buffer where the processed data will be stored.
 * @return 0 on success, or a negative error code on failure.
 */
int gcm_update(gcm_context_t* ctx, size_t length, const uint8_t* input, uint8_t* output);

/**
 * @brief Finalizes the GCM operation and generates/verifies the authentication tag.
 *
 * This function completes the GCM operation. For encryption, it generates the
 * authentication tag. For decryption, it verifies the provided tag against the
 * computed tag.
 *
 * @param ctx Pointer to the GCM context structure.
 * @param tag Pointer to the buffer where the authentication tag will be stored (encryption)
 *            or from which the tag to be verified will be read (decryption).
 * @param tag_len The length of the authentication tag in bytes.
 * @return 0 on success (or successful tag verification during decryption),
 *         GCM_AUTH_FAILURE if tag verification fails during decryption,
 *         or a negative error code on other failures.
 */
int gcm_finish(gcm_context_t* ctx, uint8_t* tag, size_t tag_len);

/**
 * @brief Clears and zeroes out the GCM context structure.
 *
 * This function securely erases the sensitive information stored within the
 * GCM context, such as keys and intermediate states, to prevent memory leakage.
 * It should be called after the GCM operation is completed.
 *
 * @param ctx Pointer to the GCM context structure to be zeroed out.
 */
void gcm_zero_ctx(gcm_context_t * ctx );

#ifdef __cplusplus
}
#endif

#endif
