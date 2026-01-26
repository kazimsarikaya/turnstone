/**
 * @file aes.h
 * @brief Advanced Encryption Standard (AES) implementation header file.
 *
 * This header provides the interface for the Advanced Encryption Standard (AES)
 * block cipher. It supports key initialization and single-block encryption/decryption.
 * The implementation follows the FIPS 197 standard.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___AES_HEADER_H
#define ___AES_HEADER_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name AES Operation Modes
 * @{
 */
/** @brief Constant for encryption mode. */
#define AES_ENCRYPT         1
/** @brief Constant for decryption mode. */
#define AES_DECRYPT         0
/** @} */

/**
 * @name AES Key Sizes (in bytes)
 * @{
 */
/** @brief 128-bit key size. */
#define AES128_KEY_SIZE   16
/** @brief 192-bit key size. */
#define AES192_KEY_SIZE   24
/** @brief 256-bit key size. */
#define AES256_KEY_SIZE   32
/** @} */

/**
 * @brief Initializes the internal lookup tables for AES key generation.
 *
 * This function must be called before any other AES functions to populate
 * the static S-boxes and multiplication tables used during key expansion.
 * It is idempotent; subsequent calls will return immediately.
 *
 * @note This function is not thread-safe during the first initialization.
 */
void aes_init_keygen_tables(void);

/**
 * @brief AES context structure.
 *
 * This structure holds the state for an AES operation, including the
 * expanded round keys and the operational mode. It should be initialized
 * via aes_setkey() before being used in aes_cipher().
 */
typedef struct aes_context_t {
    int32_t    mode; /**< Operation mode: AES_ENCRYPT or AES_DECRYPT. */
    int32_t    rounds; /**< Number of rounds (10, 12, or 14 depending on key size). */
    uint32_t * rk; /**< Pointer to the expanded round keys (points into buf). */
    uint32_t   buf[68]; /**< Internal buffer for expanded round keys. */
} aes_context_t;

/**
 * @brief Sets the key for AES encryption or decryption.
 *
 * Expands the provided raw key into the round keys stored in the context.
 * This function prepares the context for subsequent block operations.
 *
 * @param ctx      Pointer to the AES context to be initialized.
 * @param mode     The operation mode (#AES_ENCRYPT or #AES_DECRYPT).
 * @param key      Pointer to the raw key bytes.
 * @param keysize  Size of the key in bytes (must be 16, 24, or 32).
 *
 * @return 0 on success.
 * @return -1 if the key size is invalid or tables are not initialized.
 */
int aes_setkey(aes_context_t* ctx, int32_t mode, const uint8_t* key, uint32_t keysize );

/**
 * @brief Performs AES encryption or decryption on a single 16-byte block.
 *
 * The operation performed (encryption or decryption) depends on the mode
 * set during the call to aes_setkey().
 *
 * @param ctx    Pointer to the initialized AES context.
 * @param input  Pointer to the 16-byte input block (plaintext or ciphertext).
 * @param output Pointer to the 16-byte buffer where the result will be stored.
 *
 * @return 0 on success.
 * @return Non-zero error code on failure.
 *
 * @warning The input and output buffers must be at least 16 bytes long.
 */
int aes_cipher(aes_context_t* ctx, const uint8_t input[16], uint8_t output[16]);

#ifdef __cplusplus
}
#endif

#endif /* ___AES_HEADER_H */
