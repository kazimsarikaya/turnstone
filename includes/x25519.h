/**
 * @file x25519.h
 * @brief X25519 Key Exchange Header.
 *
 * This file provides functions for performing X25519 elliptic curve Diffie-Hellman
 * key exchange, including key generation, public key derivation, shared secret
 * computation, and PEM encoding/decoding.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___X25519_H
#define ___X25519_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def X25519_PRIVATE_KEY_DER_LEN
 * @brief Length of an X25519 private key when encoded in DER format (PKCS#8).
 */
#define X25519_PRIVATE_KEY_DER_LEN 48

/**
 * @def X25519_PRIVATE_KEY_RAW_LEN
 * @brief Length of a raw X25519 private key in bytes.
 */
#define X25519_PRIVATE_KEY_RAW_LEN 32

/**
 * @def X25519_PUBLIC_KEY_DER_LEN
 * @brief Length of an X25519 public key when encoded in DER format (SubjectPublicKeyInfo).
 */
#define X25519_PUBLIC_KEY_DER_LEN 44

/**
 * @def X25519_PUBLIC_KEY_RAW_LEN
 * @brief Length of a raw X25519 public key in bytes.
 */
#define X25519_PUBLIC_KEY_RAW_LEN 32

/**
 * @def X25519_SHARED_SECRET_LEN
 * @brief Length of the computed X25519 shared secret in bytes.
 */
#define X25519_SHARED_SECRET_LEN 32

/**
 * @def ED25519_PRIVATE_KEY_DER_LEN
 * @brief Length of an Ed25519 private key when encoded in DER format (PKCS#8).
 */
#define ED25519_PRIVATE_KEY_DER_LEN 48

/**
 * @def ED25519_PRIVATE_KEY_RAW_LEN
 * @brief Length of a raw Ed25519 private key in bytes.
 */
#define ED25519_PRIVATE_KEY_RAW_LEN 32

/**
 * @def ED25519_PUBLIC_KEY_DER_LEN
 * @brief Length of an Ed25519 public key when encoded in DER format (SubjectPublicKeyInfo).
 */
#define ED25519_PUBLIC_KEY_DER_LEN 44

/**
 * @def ED25519_PUBLIC_KEY_RAW_LEN
 * @brief Length of a raw Ed25519 public key in bytes.
 */
#define ED25519_PUBLIC_KEY_RAW_LEN 32

/**
 * @def ED25519_SIGNATURE_LEN
 * @brief Length of an Ed25519 signature in bytes.
 */
#define ED25519_SIGNATURE_LEN 64

/**
 * @brief Reads an X25519 private key from a PEM-encoded string.
 *
 * @param[in] pem A null-terminated string containing the PEM-encoded private key.
 * @param[out] out_key A buffer of size #X25519_PRIVATE_KEY_RAW_LEN to store the raw private key.
 * @return 0 on success, non-zero on failure (e.g., invalid PEM format, buffer too small).
 */
int8_t pem_read_x25519_private_key(const char_t* pem, uint8_t* out_key);

/**
 * @brief Reads an X25519 public key from a PEM-encoded string.
 *
 * @param[in] pem A null-terminated string containing the PEM-encoded public key.
 * @param[out] out_key A buffer of size #X25519_PUBLIC_KEY_RAW_LEN to store the raw public key.
 * @return 0 on success, non-zero on failure (e.g., invalid PEM format, buffer too small).
 */
int8_t pem_read_x25519_public_key(const char_t* pem, uint8_t* out_key);

/**
 * @brief Writes a raw X25519 private key to a newly allocated PEM-encoded string.
 *
 * The caller is responsible for freeing the allocated string `*out_pem`.
 *
 * @param[in] in_key A buffer of size #X25519_PRIVATE_KEY_RAW_LEN containing the raw private key.
 * @param[out] out_pem A pointer to a char_t* that will be allocated and filled with the PEM string.
 * @return 0 on success, non-zero on failure (e.g., memory allocation error).
 */
int8_t pem_write_x25519_private_key(const uint8_t* in_key, char_t** out_pem);

/**
 * @brief Writes a raw X25519 public key to a newly allocated PEM-encoded string.
 *
 * The caller is responsible for freeing the allocated string `*out_pem`.
 *
 * @param[in] in_key A buffer of size #X25519_PUBLIC_KEY_RAW_LEN containing the raw public key.
 * @param[out] out_pem A pointer to a char_t* that will be allocated and filled with the PEM string.
 * @return 0 on success, non-zero on failure (e.g., memory allocation error).
 */
int8_t pem_write_x25519_public_key(const uint8_t* in_key, char_t** out_pem);

/**
 * @brief Clamps an X25519 private key according to RFC 7748.
 *
 * This function modifies the provided private key in place to ensure it
 * conforms to the X25519 specification for private keys (i.e., setting
 * specific bits to 0 or 1).
 *
 * @param[in,out] k The 32-byte raw private key to be clamped.
 */
void x25519_clamp(uint8_t k[X25519_PRIVATE_KEY_RAW_LEN]);

/**
 * @brief Derives the public key from a given private key.
 *
 * @param[out] out_pub A buffer of size #X25519_PUBLIC_KEY_RAW_LEN to store the derived public key.
 * @param[in] in_priv A buffer of size #X25519_PRIVATE_KEY_RAW_LEN containing the raw private key.
 * @return 0 on success, non-zero on failure.
 */
int8_t x25519_derive_public(uint8_t out_pub[X25519_PUBLIC_KEY_RAW_LEN], const uint8_t in_priv[X25519_PRIVATE_KEY_RAW_LEN]);

/**
 * @brief Generates a new X25519 key pair (private and public keys).
 *
 * The private key is generated randomly and then clamped. The public key
 * is derived from the clamped private key.
 *
 * @param[out] out_priv A buffer of size #X25519_PRIVATE_KEY_RAW_LEN to store the generated private key.
 * @param[out] out_pub A buffer of size #X25519_PUBLIC_KEY_RAW_LEN to store the generated public key.
 * @return 0 on success, non-zero on failure (e.g., random number generation failure).
 */
int8_t x25519_generate_keypair(uint8_t out_priv[X25519_PRIVATE_KEY_RAW_LEN], uint8_t out_pub[X25519_PUBLIC_KEY_RAW_LEN]);

/**
 * @brief Computes the shared secret using the local private key and a remote public key.
 *
 * @param[out] out_shared A buffer of size #X25519_SHARED_SECRET_LEN to store the computed shared secret.
 * @param[in] my_priv A buffer of size #X25519_PRIVATE_KEY_RAW_LEN containing the local raw private key.
 * @param[in] their_pub A buffer of size #X25519_PUBLIC_KEY_RAW_LEN containing the remote raw public key.
 * @return 0 on success, non-zero on failure.
 */
int8_t x25519_shared_secret(uint8_t       out_shared[X25519_SHARED_SECRET_LEN],
                            const uint8_t my_priv[X25519_PRIVATE_KEY_RAW_LEN],
                            const uint8_t their_pub[X25519_PUBLIC_KEY_RAW_LEN]);

/**
 * @brief Derives the Ed25519 public key from a given private seed.
 * @param[out] pub_out A buffer of size 32 bytes to store the derived public key.
 * @param[in] priv_seed A buffer of size 32 bytes containing the private seed.
 * @return 0 on success, non-zero on failure.
 */
int8_t ed25519_derive_pubkey(uint8_t pub_out[32], const uint8_t priv_seed[32]);

/**
 * @brief Signs a message using the Ed25519 signature algorithm.
 *
 * @param[out] sig A buffer of size 64 bytes to store the generated signature.
 * @param[in] msg A pointer to the message to be signed.
 * @param[in] msg_len The length of the message in bytes.
 * @param[in] priv_seed A buffer of size 32 bytes containing the private seed.
 * @return 0 on success, non-zero on failure.
 */
int8_t ed25519_sign(uint8_t sig[64], const uint8_t* msg, size_t msg_len,
                    const uint8_t priv_seed[32]);

/**
 * @brief Verifies an Ed25519 signature for a given message and public key.
 * @param[in] sig A buffer of size 64 bytes containing the signature to verify.
 * @param[in] msg A pointer to the message whose signature is to be verified.
 * @param[in] msg_len The length of the message in bytes.
 * @param[in] pub_key A buffer of size 32 bytes containing the public key.
 * @return 0 if the signature is valid, non-zero if invalid or on failure.
 */
int8_t ed25519_verify(const uint8_t sig[64], const uint8_t* msg, size_t msg_len, const uint8_t pub_key[32]);

/**
 * @brief Generates a new Ed25519 key pair (private seed and public key).
 *
 * The private seed is generated randomly, and the public key is derived from it.
 *
 * @param[out] out_priv A buffer of size 32 bytes to store the generated private seed.
 * @param[out] out_pub A buffer of size 32 bytes to store the generated public key.
 * @return 0 on success, non-zero on failure.
 */
int8_t ed25519_generate_keypair(uint8_t out_priv[32], uint8_t out_pub[32]);

/**
 * @brief Writes a raw Ed25519 private key to a newly allocated PEM-encoded string.
 *
 * The caller is responsible for freeing the allocated string `*out_pem`.
 *
 * @param[in] in_key A buffer of size #ED25519_PRIVATE_KEY_RAW_LEN containing the raw private key.
 * @param[out] out_pem A pointer to a char_t* that will be allocated and filled with the PEM string.
 * @return 0 on success, non-zero on failure (e.g., memory allocation error).
 */
int8_t pem_write_ed25519_private_key(const uint8_t* in_key, char_t** out_pem);

#ifdef __cplusplus
}
#endif

#endif // ___X25519_H
