/**
 * @file aes-gcm.h
 * @brief AES-GCM (Galois/Counter Mode) encryption and decryption functions.
 *
 * This header provides an interface for performing authenticated encryption
 * and decryption using the AES algorithm in GCM mode. It relies on the
 * underlying GCM implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___AES_GCM_HEADER_H
#define ___AES_GCM_HEADER_H

#include <gcm.h>
#include <types.h> // Assuming types.h defines uint8_t, int32_t, size_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encrypts data using AES-GCM.
 *
 * This function performs AES-GCM encryption on the provided input data.
 * It generates ciphertext and an authentication tag. The tag is typically
 * appended to the ciphertext or handled separately by the caller.
 *
 * @param output Pointer to the buffer where the ciphertext and authentication tag will be stored.
 *               The caller must ensure this buffer is large enough to hold the encrypted data
 *               (input_length + GCM_TAG_LENGTH_BYTES).
 * @param input Pointer to the plaintext data to be encrypted.
 * @param input_length Length of the plaintext data in bytes.
 * @param key Pointer to the AES encryption key.
 * @param key_len Length of the AES key in bytes (e.g., 16 for AES-128, 24 for AES-192, 32 for AES-256).
 * @param iv Pointer to the Initialization Vector (IV). For GCM, this is typically a 12-byte (96-bit) nonce.
 * @param iv_len Length of the IV in bytes.
 * @return 0 on success, or a negative error code on failure.
 */
int32_t aes_gcm_encrypt(uint8_t* output, const uint8_t* input, int32_t input_length, const uint8_t* key, const size_t key_len, const uint8_t* iv, const size_t iv_len);

/**
 * @brief Decrypts data using AES-GCM and verifies its authenticity.
 *
 * This function performs AES-GCM decryption on the provided ciphertext.
 * It also verifies the authentication tag to ensure the integrity and
 * authenticity of the data.
 *
 * @param output Pointer to the buffer where the decrypted plaintext will be stored.
 *               The caller must ensure this buffer is large enough to hold the plaintext
 *               (input_length - GCM_TAG_LENGTH_BYTES).
 * @param input Pointer to the ciphertext and authentication tag. The tag is expected
 *              to be appended to the ciphertext.
 * @param input_length Total length of the input data (ciphertext + authentication tag) in bytes.
 * @param key Pointer to the AES decryption key.
 * @param key_len Length of the AES key in bytes.
 * @param iv Pointer to the Initialization Vector (IV) used during encryption.
 * @param iv_len Length of the IV in bytes.
 * @return 0 on successful decryption and authentication, or a negative error code on failure
 *         (e.g., authentication failure, invalid input).
 */
int32_t aes_gcm_decrypt(uint8_t* output, const uint8_t* input, int32_t input_length, const uint8_t* key, const size_t key_len, const uint8_t* iv, const size_t iv_len);

#ifdef __cplusplus
}
#endif

#endif
