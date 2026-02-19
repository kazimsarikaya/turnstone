/**
 * @file ellipticcurve.h
 * @brief Elliptic Curve Cryptography (ECC) Header.
 *
 * This file defines the structures and function prototypes for Elliptic Curve Cryptography (ECC) operations,
 * specifically focusing on the secp256r1 curve. It provides functionalities for key generation,
 * signing, verification, and shared secret computation using ECC.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___ELLIPTICCURVE_H
#define ___ELLIPTICCURVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

/**
 * @brief Defines the raw byte length of a secp256r1 private key.
 *
 * A secp256r1 private key is a 256-bit scalar, which corresponds to 32 bytes.
 */
#define ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN 32

/**
 * @brief Defines the raw byte length of a secp256r1 public key.
 *
 * A secp256r1 public key is represented as point (X || Y),
 * where X and Y are 32-byte coordinates. Therefore, the total raw length of the public key is 64 bytes.
 */
#define ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN 64

/**
 * @brief Defines the raw byte length of a secp256r1 signature.
 *
 * A secp256r1 signature consists of two 32-byte integers (r and s).
 */
#define ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN 64

/**
 * @brief Defines the byte length of a secp256r1 shared secret.
 *
 * The shared secret derived from the Diffie-Hellman key exchange is a 256-bit value, which is 32 bytes.
 */
#define ELLIPTICCURVE_SECP256R1_SHARED_SECRET_LEN 32

/**
 * @brief Derives the public key from a secp256r1 private key.
 *
 * Given a private key (a 256-bit scalar), this function computes the corresponding public key
 * (a point on the secp256r1 curve). The public key is returned in raw uncompressed format (X || Y), where X and Y are 32-byte coordinates.
 *
 * @param out_pub Buffer to store the derived public key (64 bytes, uncompressed format).
 * @param priv The private key (32 bytes).
 * @return 0 on success, -1 on failure (e.g., invalid input, internal error).
 */
int8_t ellipticcurve_secp256r1_derive_pubkey(uint8_t       out_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN],
                                             const uint8_t priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN]);

/**
 * @brief Generates a new secp256r1 key pair (private and public key).
 *
 * This function generates a cryptographically secure random private key and derives the
 * corresponding public key.
 *
 * @param out_priv Buffer to store the generated private key (32 bytes).
 * @param out_pub Buffer to store the derived public key (64 bytes, uncompressed format).
 * @return 0 on success, -1 on failure (e.g., random number generation error, key derivation error).
 */
int8_t ellipticcurve_secp256r1_generate_keypair(uint8_t out_priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN],
                                                uint8_t out_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]);

/**
 * @brief Signs a message digest using a secp256r1 private key.
 *
 * This function computes a digital signature for a given message digest using the ECDSA algorithm
 * with the secp256r1 curve. The signature is returned in a standard 64-byte format (r || s).
 *
 * @param out_sig Buffer to store the generated signature (64 bytes).
 * @param msg Pointer to the message digest to be signed.
 * @param msg_len Length of the message digest.
 * @param priv The private key (32 bytes).
 * @return 0 on success, -1 on failure (e.g., invalid input, signing error, invalid signature components).
 */
int8_t ellipticcurve_secp256r1_sign(uint8_t out_sig[ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN],
                                    const uint8_t* msg, size_t msg_len,
                                    const uint8_t priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN]);

/**
 * @brief Verifies a secp256r1 digital signature.
 *
 * This function verifies if a given signature is valid for a specific message digest and public key
 * using the ECDSA algorithm with the secp256r1 curve.
 *
 * @param sig The signature to verify (64 bytes, r || s).
 * @param msg Pointer to the message digest.
 * @param msg_len Length of the message digest.
 * @param pub The public key (64 bytes, uncompressed format).
 * @return 0 if the signature is valid, 1 if the signature is invalid, -1 on failure (e.g., invalid input, verification error).
 */
int8_t ellipticcurve_secp256r1_verify(const uint8_t sig[ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN],
                                      const uint8_t* msg, size_t msg_len,
                                      const uint8_t pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]);

/**
 * @brief Encodes a raw secp256r1 signature into DER format.
 *
 * Converts the raw 64-byte signature (r || s) into the standard ASN.1 DER encoding for ECDSA signatures.
 * The DER encoding typically wraps the signature in a SEQUENCE containing two INTEGERs (r and s).
 *
 * @param sig The raw signature (64 bytes).
 * @param encoded_length Pointer to a `size_t` that will receive the length of the DER-encoded signature.
 * @return A pointer to the allocated buffer containing the DER-encoded signature, or `NULL` on failure. The caller is responsible for freeing this buffer.
 */
uint8_t* ellipticcurve_secp256r1_encode_signature(const uint8_t sig[ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN], size_t* encoded_length);

/**
 * @brief Decodes a DER-encoded secp256r1 signature into raw format.
 *
 * Parses the ASN.1 DER encoding of an ECDSA signature and extracts the raw 64-byte signature (r || s).
 *
 * @param der_sig Pointer to the DER-encoded signature.
 * @param der_sig_len Length of the DER-encoded signature.
 * @return A pointer to the allocated buffer containing the raw signature (64 bytes), or `NULL` on failure. The caller is responsible for freeing this buffer.
 */
uint8_t* ellipticcurve_secp256r1_decode_signature(const uint8_t* der_sig, size_t der_sig_len);

/**
 * @brief Computes the shared secret using Diffie-Hellman key exchange (ECDH).
 *
 * Given a private key and the peer's public key, this function computes the shared secret
 * using the ECDH protocol with the secp256r1 curve.
 *
 * @param shared_secret Buffer to store the computed shared secret (32 bytes).
 * @param priv The local private key (32 bytes).
 * @param pub The peer's public key (64 bytes, uncompressed format).
 * @return 0 on success, -1 on failure (e.g., invalid input, computation error).
 */
int8_t ellipticcurve_secp256r1_shared_secret(uint8_t       shared_secret[ELLIPTICCURVE_SECP256R1_SHARED_SECRET_LEN],
                                             const uint8_t priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN],
                                             const uint8_t pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]);

/**
 * @brief Reads a secp256r1 private key from PEM format.
 *
 * Parses a PEM-encoded private key (typically in PKCS#8 format) and extracts the raw private key bytes.
 *
 * @param pem Pointer to the null-terminated PEM-encoded string.
 * @param out_priv Buffer to store the extracted private key (32 bytes).
 * @return 0 on success, -1 on failure (e.g., invalid PEM format, incorrect key type, parsing error).
 */
int8_t pem_read_secp256r1_private_key(const char_t* pem,
                                      uint8_t       out_priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN]);

/**
 * @brief Reads a secp256r1 public key from PEM format.
 *
 * Parses a PEM-encoded public key (typically in SubjectPublicKeyInfo format) and extracts the raw public key bytes.
 *
 * @param pem Pointer to the null-terminated PEM-encoded string.
 * @param out_pub Buffer to store the extracted public key (64 bytes, uncompressed format).
 * @return 0 on success, -1 on failure (e.g., invalid PEM format, incorrect key type, parsing error).
 */
int8_t pem_read_secp256r1_public_key(const char_t* pem,
                                     uint8_t       out_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]);

/**
 * @brief Writes a secp256r1 private key to PEM format.
 *
 * Converts a raw secp256r1 private key into a PEM-encoded string (typically PKCS#8 format).
 * The caller is responsible for freeing the allocated PEM string.
 *
 * @param in_priv The raw private key (32 bytes).
 * @param out_pem Pointer to a `char_t**` that will receive the allocated PEM string.
 * @return 0 on success, -1 on failure (e.g., invalid input, memory allocation error, PEM encoding error).
 */
int8_t pem_write_secp256r1_private_key(const uint8_t in_priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN],
                                       char_t**      out_pem);

/**
 * @brief Writes a secp256r1 public key to PEM format.
 *
 * Converts a raw secp256r1 public key into a PEM-encoded string (typically SubjectPublicKeyInfo format).
 * The caller is responsible for freeing the allocated PEM string.
 *
 * @param in_pub The raw public key (64 bytes, uncompressed format).
 * @param out_pem Pointer to a `char_t**` that will receive the allocated PEM string.
 * @return 0 on success, -1 on failure (e.g., invalid input, memory allocation error, PEM encoding error).
 */
int8_t pem_write_secp256r1_public_key(const uint8_t in_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN],
                                      char_t**      out_pem);

#ifdef __cplusplus
}
#endif

#endif // ___ELLIPTICCURVE_H
