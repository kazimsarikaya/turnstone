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

typedef enum ellipticcurve_curve_type_t {
    ELLIPTICCURVE_CURVE_TYPE_NONE = 0,
    ELLIPTICCURVE_CURVE_TYPE_SECP256R1,
    ELLIPTICCURVE_CURVE_TYPE_SECP384R1,
    ELLIPTICCURVE_CURVE_TYPE_MAX,
} ellipticcurve_curve_type_t;

#define ELLIPTICCURVE_SECP256R1_BIT_LEN 256
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

#define ELLIPTICCURVE_SECP256R1_SHARED_SECRET_RAW_LEN 32

/**
 * @brief Defines the byte length of a secp256r1 shared secret.
 *
 * The shared secret derived from the Diffie-Hellman key exchange is a 256-bit value, which is 32 bytes.
 */
#define ELLIPTICCURVE_SECP256R1_SHARED_SECRET_LEN 32

#define ELLIPTICCURVE_SECP384R1_BIT_LEN 384
#define ELLIPTICCURVE_SECP384R1_PRIVATE_KEY_RAW_LEN 48
#define ELLIPTICCURVE_SECP384R1_PUBLIC_KEY_RAW_LEN 96
#define ELLIPTICCURVE_SECP384R1_SIGNATURE_RAW_LEN 96
#define ELLIPTICCURVE_SECP384R1_SHARED_SECRET_LEN 48

/**
 * @brief Derives the public key from a given private key for the specified elliptic curve curve.
 *
 * This function takes a private key and computes the corresponding public key based on the specified elliptic curve curve (e.g., secp256r1). The output public key is returned in raw uncompressed format (X || Y).
 *
 * @param curve The elliptic curve curve to use for key derivation (e.g., ELLIPTICCURVE_CURVE_TYPE_SECP256R1).
 * @param out_pub Buffer to store the derived public key (size depends on the curve_type, e.g., 64 bytes for secp256r1).
 * @param priv The private key (size depends on the curve_type, e.g., 32 bytes for secp256r1).
 * @return 0 on success, -1 on failure (e.g., invalid input, unsupported curve_type, internal error).
 */
int8_t ellipticcurve_derive_pubkey(ellipticcurve_curve_type_t curve_type,
                                   uint8_t*                   out_pub,
                                   const uint8_t*             priv);

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
#define ellipticcurve_secp256r1_derive_pubkey(out_pub, priv) \
        ellipticcurve_derive_pubkey(ELLIPTICCURVE_CURVE_TYPE_SECP256R1, out_pub, priv)

/**
 * @brief Derives the public key from a secp384r1 private key.
 *
 * Given a private key (a 384-bit scalar), this function computes the corresponding public key
 * (a point on the secp384r1 curve). The public key is returned in raw uncompressed format (X || Y), where X and Y are 48-byte coordinates.
 *
 * @param out_pub Buffer to store the derived public key (96 bytes, uncompressed format).
 * @param priv The private key (48 bytes).
 * @return 0 on success, -1 on failure (e.g., invalid input, internal error).
 */
#define ellipticcurve_secp384r1_derive_pubkey(out_pub, priv) \
        ellipticcurve_derive_pubkey(ELLIPTICCURVE_CURVE_TYPE_SECP384R1, out_pub, priv)

/**
 * @brief Generates a new key pair (private and public key) for the specified elliptic curve curve.
 *
 * This function generates a cryptographically secure random private key and derives the corresponding public key based on the specified elliptic curve curve (e.g., secp256r1). The output private and public keys are returned in raw format.
 *
 * @param curve The elliptic curve curve to use for key generation (e.g., ELLIPTICCURVE_CURVE_TYPE_SECP256R1).
 * @param out_priv Buffer to store the generated private key (size depends on the curve_type, e.g., 32 bytes for secp256r1).
 * @param out_pub Buffer to store the derived public key (size depends on the curve_type, e.g., 64 bytes for secp256r1).
 * @return 0 on success, -1 on failure (e.g., random number generation error, unsupported curve_type, internal error).
 */
int8_t ellipticcurve_generate_keypair(ellipticcurve_curve_type_t curve_type,
                                      uint8_t*                   out_priv,
                                      uint8_t*                   out_pub);
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
#define ellipticcurve_secp256r1_generate_keypair(out_priv, out_pub) \
        ellipticcurve_generate_keypair(ELLIPTICCURVE_CURVE_TYPE_SECP256R1, out_priv, out_pub)

/**
 * @brief Generates a new secp384r1 key pair (private and public key).
 *
 * This function generates a cryptographically secure random private key and derives the
 * corresponding public key.
 *
 * @param out_priv Buffer to store the generated private key (48 bytes).
 * @param out_pub Buffer to store the derived public key (96 bytes, uncompressed format).
 * @return 0 on success, -1 on failure (e.g., random number generation error, key derivation error).
 */
#define ellipticcurve_secp384r1_generate_keypair(out_priv, out_pub) \
        ellipticcurve_generate_keypair(ELLIPTICCURVE_CURVE_TYPE_SECP384R1, out_priv, out_pub)


/**
 * @brief Signs a message digest using a private key for the specified elliptic curve curve.
 *
 * This function computes a digital signature for a given message digest using the ECDSA curve
 * with the specified elliptic curve (e.g., secp256r1). The signature is returned in a standard raw format (r || s), where r and s are the two components of the ECDSA signature.
 *
 * @param curve The elliptic curve curve to use for signing (e.g., ELLIPTICCURVE_CURVE_TYPE_SECP256R1).
 * @param out_sig Buffer to store the generated signature (size depends on the curve_type, e.g., 64 bytes for secp256r1).
 * @param msg Pointer to the message digest to be signed.
 * @param msg_len Length of the message digest.
 * @param priv The private key (size depends on the curve_type, e.g., 32 bytes for secp256r1).
 * @return 0 on success, -1 on failure (e.g., invalid input, signing error, invalid signature components).
 */
int8_t ellipticcurve_sign(ellipticcurve_curve_type_t curve_type,
                          uint8_t* out_sig,
                          const uint8_t* msg, size_t msg_len,
                          const uint8_t* priv);

/**
 * @brief Signs a message digest using a secp256r1 private key.
 *
 * This function computes a digital signature for a given message digest using the ECDSA curve
 * with the secp256r1 curve. The signature is returned in a standard 64-byte format (r || s).
 *
 * @param out_sig Buffer to store the generated signature (64 bytes).
 * @param msg Pointer to the message digest to be signed.
 * @param msg_len Length of the message digest.
 * @param priv The private key (32 bytes).
 * @return 0 on success, -1 on failure (e.g., invalid input, signing error, invalid signature components).
 */
#define ellipticcurve_secp256r1_sign(out_sig, msg, msg_len, priv) \
        ellipticcurve_sign(ELLIPTICCURVE_CURVE_TYPE_SECP256R1, out_sig, msg, msg_len, priv)

/**
 * @brief Signs a message digest using a secp384r1 private key.
 *
 * This function computes a digital signature for a given message digest using the ECDSA curve
 * with the secp384r1 curve. The signature is returned in a standard 96-byte format (r || s).
 *
 * @param out_sig Buffer to store the generated signature (96 bytes).
 * @param msg Pointer to the message digest to be signed.
 * @param msg_len Length of the message digest.
 * @param priv The private key (48 bytes).
 * @return 0 on success, -1 on failure (e.g., invalid input, signing error, invalid signature components).
 */
#define ellipticcurve_secp384r1_sign(out_sig, msg, msg_len, priv) \
        ellipticcurve_sign(ELLIPTICCURVE_CURVE_TYPE_SECP384R1, out_sig, msg, msg_len, priv)

/**
 * @brief Verifies a digital signature for a given message digest and public key using the specified elliptic curve curve.
 *
 * This function verifies if a given signature is valid for a specific message digest and public key
 * using the ECDSA curve with the secp256r1 curve.
 *
 * @param curve The elliptic curve curve to use for verification (e.g., ELLIPTICCURVE_CURVE_TYPE_SECP256R1).
 * @param sig The signature to verify (size depends on the curve_type, e.g., 64 bytes for secp256r1).
 * @param msg Pointer to the message digest.
 * @param msg_len Length of the message digest.
 * @param pub The public key (64 bytes, uncompressed format).
 * @return 0 if the signature is valid, 1 if the signature is invalid, -1 on failure (e.g., invalid input, verification error).
 */
int8_t ellipticcurve_verify(ellipticcurve_curve_type_t curve_type,
                            const uint8_t* sig,
                            const uint8_t* msg, size_t msg_len,
                            const uint8_t* pub);

/**
 * @brief Verifies a secp256r1 digital signature.
 *
 * This function verifies if a given signature is valid for a specific message digest and public key
 * using the ECDSA curve with the secp256r1 curve.
 *
 * @param sig The signature to verify (64 bytes, r || s).
 * @param msg Pointer to the message digest.
 * @param msg_len Length of the message digest.
 * @param pub The public key (64 bytes, uncompressed format).
 * @return 0 if the signature is valid, 1 if the signature is invalid, -1 on failure (e.g., invalid input, verification error).
 */
#define ellipticcurve_secp256r1_verify(sig, msg, msg_len, pub) \
        ellipticcurve_verify(ELLIPTICCURVE_CURVE_TYPE_SECP256R1, sig, msg, msg_len, pub)

/**
 * @brief Verifies a secp384r1 digital signature.
 *
 * This function verifies if a given signature is valid for a specific message digest and public key
 * using the ECDSA curve with the secp384r1 curve.
 *
 * @param sig The signature to verify (96 bytes, r || s).
 * @param msg Pointer to the message digest.
 * @param msg_len Length of the message digest.
 * @param pub The public key (96 bytes, uncompressed format).
 * @return 0 if the signature is valid, 1 if the signature is invalid, -1 on failure (e.g., invalid input, verification error).
 */
#define ellipticcurve_secp384r1_verify(sig, msg, msg_len, pub) \
        ellipticcurve_verify(ELLIPTICCURVE_CURVE_TYPE_SECP384R1, sig, msg, msg_len, pub)

/**
 * @brief Encodes a raw secp256r1 signature into DER format.
 *
 * Converts the raw 64-byte signature (r || s) into the standard ASN.1 DER encoding for ECDSA signatures.
 * The DER encoding typically wraps the signature in a SEQUENCE containing two INTEGERs (r and s).
 *
 * @param curve The elliptic curve curve to use for encoding (e.g., ELLIPTICCURVE_CURVE_TYPE_SECP256R1).
 * @param sig The raw signature (64 bytes for secp256r1, 96 bytes for secp384r1).
 * @param encoded_length Pointer to a `size_t` that will receive the length of the DER-encoded signature.
 * @return A pointer to the allocated buffer containing the DER-encoded signature, or `NULL` on failure. The caller is responsible for freeing this buffer.
 */
uint8_t* ellipticcurve_encode_signature(ellipticcurve_curve_type_t curve_type, const uint8_t* sig, size_t* encoded_length);

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
#define ellipticcurve_secp256r1_encode_signature(sig, encoded_length) \
        ellipticcurve_encode_signature(ELLIPTICCURVE_CURVE_TYPE_SECP256R1, sig, encoded_length)

/**
 * @brief Encodes a raw secp384r1 signature into DER format.
 *
 * Converts the raw 96-byte signature (r || s) into the standard ASN.1 DER encoding for ECDSA signatures.
 * The DER encoding typically wraps the signature in a SEQUENCE containing two INTEGERs (r and s).
 *
 * @param sig The raw signature (96 bytes).
 * @param encoded_length Pointer to a `size_t` that will receive the length of the DER-encoded signature.
 * @return A pointer to the allocated buffer containing the DER-encoded signature, or `NULL` on failure. The caller is responsible for freeing this buffer.
 */
#define ellipticcurve_secp384r1_encode_signature(sig, encoded_length) \
        ellipticcurve_encode_signature(ELLIPTICCURVE_CURVE_TYPE_SECP384R1, sig, encoded_length)

/**
 * @brief Decodes a DER-encoded ECDSA signature into raw format (r || s).
 *
 * This function takes a DER-encoded ECDSA signature (typically a SEQUENCE of two INTEGERs) and extracts the raw r and s components, returning them in a standard format (r || s).
 *
 * @param curve The elliptic curve curve to use for decoding (e.g., ELLIPTICCURVE_CURVE_TYPE_SECP256R1).
 * @param der_sig Pointer to the DER-encoded signature.
 * @param der_sig_len Length of the DER-encoded signature.
 * @return A pointer to the allocated buffer containing the raw signature (64 bytes), or `NULL` on failure. The caller is responsible for freeing this buffer.
 */
uint8_t* ellipticcurve_decode_signature(ellipticcurve_curve_type_t curve_type, const uint8_t* der_sig, size_t der_sig_len);

/**
 * @brief Decodes a DER-encoded secp256r1 ECDSA signature into raw format (r || s).
 *
 * This function takes a DER-encoded ECDSA signature (typically a SEQUENCE of two INTEGERs) and extracts the raw r and s components, returning them in a standard format (r || s).
 *
 * @param der_sig Pointer to the DER-encoded signature.
 * @param der_sig_len Length of the DER-encoded signature.
 * @return A pointer to the allocated buffer containing the raw signature (64 bytes), or `NULL` on failure. The caller is responsible for freeing this buffer.
 */
#define ellipticcurve_secp256r1_decode_signature(der_sig, der_sig_len) \
        ellipticcurve_decode_signature(ELLIPTICCURVE_CURVE_TYPE_SECP256R1, der_sig, der_sig_len)

/**
 * @brief Decodes a DER-encoded secp384r1 ECDSA signature into raw format (r || s).
 *
 * This function takes a DER-encoded ECDSA signature (typically a SEQUENCE of two INTEGERs) and extracts the raw r and s components, returning them in a standard format (r || s).
 *
 * @param der_sig Pointer to the DER-encoded signature.
 * @param der_sig_len Length of the DER-encoded signature.
 * @return A pointer to the allocated buffer containing the raw signature (96 bytes), or `NULL` on failure. The caller is responsible for freeing this buffer.
 */
#define ellipticcurve_secp384r1_decode_signature(der_sig, der_sig_len) \
        ellipticcurve_decode_signature(ELLIPTICCURVE_CURVE_TYPE_SECP384R1, der_sig, der_sig_len)

/**
 * @brief Computes the shared secret using Elliptic Curve Diffie-Hellman (ECDH) key exchange.
 *
 * This function computes the shared secret by performing scalar multiplication of the private key with the peer's public key on the specified elliptic curve. The resulting shared secret is typically derived from the X coordinate of the resulting point.
 *
 * @param curve The elliptic curve curve to use for shared secret computation (e.g., ELLIPTICCURVE_CURVE_TYPE_SECP256R1).
 * @param shared_secret Buffer to store the computed shared secret (size depends on the curve_type, e.g., 32 bytes for secp256r1).
 * @param priv The private key (size depends on the curve_type, e.g., 32 bytes for secp256r1).
 * @param pub The peer's public key (size depends on the curve_type, e.g., 64 bytes for secp256r1, uncompressed format).
 * @return 0 on success, -1 on failure (e.g., invalid input, key agreement error, unsupported curve).
 */
int8_t ellipticcurve_shared_secret(ellipticcurve_curve_type_t curve_type,
                                   uint8_t*                   shared_secret,
                                   const uint8_t*             priv,
                                   const uint8_t*             pub);
/**
 * @brief Computes the shared secret using Elliptic Curve Diffie-Hellman (ECDH) key exchange for secp256r1.
 *
 * This function computes the shared secret by performing scalar multiplication of the private key with the peer's public key on the secp256r1 curve. The resulting shared secret is typically derived from the X coordinate of the resulting point.
 *
 * @param shared_secret Buffer to store the computed shared secret (32 bytes).
 * @param priv The private key (32 bytes).
 * @param pub The peer's public key (64 bytes, uncompressed format).
 * @return 0 on success, -1 on failure (e.g., invalid input, key agreement error).
 */
#define ellipticcurve_secp256r1_shared_secret(shared_secret, priv, pub) \
        ellipticcurve_shared_secret(ELLIPTICCURVE_CURVE_TYPE_SECP256R1, shared_secret, priv, pub)

/**
 * @brief Computes the shared secret using Elliptic Curve Diffie-Hellman (ECDH) key exchange for secp384r1.
 *
 * This function computes the shared secret by performing scalar multiplication of the private key with the peer's public key on the secp384r1 curve. The resulting shared secret is typically derived from the X coordinate of the resulting point.
 *
 * @param shared_secret Buffer to store the computed shared secret (48 bytes).
 * @param priv The private key (48 bytes).
 * @param pub The peer's public key (96 bytes, uncompressed format).
 * @return 0 on success, -1 on failure (e.g., invalid input, key agreement error).
 */
#define ellipticcurve_secp384r1_shared_secret(shared_secret, priv, pub) \
        ellipticcurve_shared_secret(ELLIPTICCURVE_CURVE_TYPE_SECP384R1, shared_secret, priv, pub)


/**
 * @brief Reads an elliptic curve private key from a PEM-encoded string.
 *
 * This function parses a PEM-encoded private key for the specified elliptic curve and extracts the raw private key bytes. The PEM string should be in the format "EC PRIVATE KEY" and contain the appropriate ASN.1 structure for the specified curve.
 *
 * @param curve The elliptic curve curve to use for parsing (e.g., ELLIPTICCURVE_CURVE_TYPE_SECP256R1).
 * @param pem The PEM-encoded private key string.
 * @param out_priv Buffer to store the extracted raw private key (size depends on the curve_type, e.g., 32 bytes for secp256r1).
 * @return 0 on success, -1 on failure (e.g., invalid PEM format, unsupported curve_type, parsing error).
 */
int8_t pem_read_ellipticcurve_private_key(ellipticcurve_curve_type_t curve_type,
                                          const char_t*              pem,
                                          uint8_t*                   out_priv);

/**
 * @brief Reads a secp256r1 private key from a PEM-encoded string.
 *
 * This function parses a PEM-encoded private key for the secp256r1 curve and extracts the raw private key bytes. The PEM string should be in the format "EC PRIVATE KEY" and contain the appropriate ASN.1 structure for secp256r1.
 *
 * @param pem The PEM-encoded private key string.
 * @param out_priv Buffer to store the extracted raw private key (32 bytes).
 * @return 0 on success, -1 on failure (e.g., invalid PEM format, parsing error).
 */
#define pem_read_secp256r1_private_key(pem, out_priv) \
        pem_read_ellipticcurve_private_key(ELLIPTICCURVE_CURVE_TYPE_SECP256R1, pem, out_priv)

/**
 * @brief Reads a secp384r1 private key from a PEM-encoded string.
 *
 * This function parses a PEM-encoded private key for the secp384r1 curve and extracts the raw private key bytes. The PEM string should be in the format "EC PRIVATE KEY" and contain the appropriate ASN.1 structure for secp384r1.
 *
 * @param pem The PEM-encoded private key string.
 * @param out_priv Buffer to store the extracted raw private key (48 bytes).
 * @return 0 on success, -1 on failure (e.g., invalid PEM format, parsing error).
 */
#define pem_read_secp384r1_private_key(pem, out_priv) \
        pem_read_ellipticcurve_private_key(ELLIPTICCURVE_CURVE_TYPE_SECP384R1, pem, out_priv)


/**
 * @brief Reads an elliptic curve public key from a PEM-encoded string.
 *
 * This function parses a PEM-encoded public key for the specified elliptic curve and extracts the raw public key bytes. The PEM string should be in the format "PUBLIC KEY" and contain the appropriate ASN.1 structure for the specified curve.
 *
 * @param curve The elliptic curve curve to use for parsing (e.g., ELLIPTICCURVE_CURVE_TYPE_SECP256R1).
 * @param pem The PEM-encoded public key string.
 * @param out_pub Buffer to store the extracted raw public key (size depends on the curve_type, e.g., 64 bytes for secp256r1, uncompressed format).
 * @return 0 on success, -1 on failure (e.g., invalid PEM format, unsupported curve_type, parsing error).
 */
int8_t pem_read_ellipticcurve_public_key(ellipticcurve_curve_type_t curve_type,
                                         const char_t*              pem,
                                         uint8_t*                   out_pub);

/**
 * @brief Reads a secp256r1 public key from a PEM-encoded string.
 *
 * This function parses a PEM-encoded public key for the secp256r1 curve and extracts the raw public key bytes. The PEM string should be in the format "PUBLIC KEY" and contain the appropriate ASN.1 structure for secp256r1.
 *
 * @param pem The PEM-encoded public key string.
 * @param out_pub Buffer to store the extracted raw public key (64 bytes, uncompressed format).
 * @return 0 on success, -1 on failure (e.g., invalid PEM format, parsing error).
 */
#define pem_read_secp256r1_public_key(pem, out_pub) \
        pem_read_ellipticcurve_public_key(ELLIPTICCURVE_CURVE_TYPE_SECP256R1, pem, out_pub)

/**
 * @brief Reads a secp384r1 public key from a PEM-encoded string.
 *
 * This function parses a PEM-encoded public key for the secp384r1 curve and extracts the raw public key bytes. The PEM string should be in the format "PUBLIC KEY" and contain the appropriate ASN.1 structure for secp384r1.
 *
 * @param pem The PEM-encoded public key string.
 * @param out_pub Buffer to store the extracted raw public key (96 bytes, uncompressed format).
 * @return 0 on success, -1 on failure (e.g., invalid PEM format, parsing error).
 */
#define pem_read_secp384r1_public_key(pem, out_pub) \
        pem_read_ellipticcurve_public_key(ELLIPTICCURVE_CURVE_TYPE_SECP384R1, pem, out_pub)

/**
 * @brief Writes an elliptic curve private key to a PEM-encoded string.
 *
 * This function takes a raw private key and encodes it into a PEM format string for the specified elliptic curve. The resulting PEM string will be in the format "EC PRIVATE KEY" and will contain the appropriate ASN.1 structure for the specified curve.
 *
 * @param curve The elliptic curve curve to use for encoding (e.g., ELLIPTICCURVE_CURVE_TYPE_SECP256R1).
 * @param in_priv The raw private key bytes (size depends on the curve_type, e.g., 32 bytes for secp256r1).
 * @param out_pem Pointer to a `char_t*` that will receive the allocated PEM string. The caller is responsible for freeing this string.
 * @return 0 on success, -1 on failure (e.g., invalid input, unsupported curve_type, encoding error).
 */
int8_t pem_write_ellipticcurve_private_key(ellipticcurve_curve_type_t curve_type,
                                           const uint8_t*             in_priv,
                                           char_t**                   out_pem);

/**
 * @brief Writes a secp256r1 private key to a PEM-encoded string.
 *
 * This function takes a raw secp256r1 private key and encodes it into a PEM format string. The resulting PEM string will be in the format "EC PRIVATE KEY" and will contain the appropriate ASN.1 structure for secp256r1.
 *
 * @param in_priv The raw private key bytes (32 bytes).
 * @param out_pem Pointer to a `char_t*` that will receive the allocated PEM string. The caller is responsible for freeing this string.
 * @return 0 on success, -1 on failure (e.g., invalid input, encoding error).
 */
#define pem_write_secp256r1_private_key(in_priv, out_pem) \
        pem_write_ellipticcurve_private_key(ELLIPTICCURVE_CURVE_TYPE_SECP256R1, in_priv, out_pem)

/**
 * @brief Writes a secp384r1 private key to a PEM-encoded string.
 *
 * This function takes a raw secp384r1 private key and encodes it into a PEM format string. The resulting PEM string will be in the format "EC PRIVATE KEY" and will contain the appropriate ASN.1 structure for secp384r1.
 *
 * @param in_priv The raw private key bytes (48 bytes).
 * @param out_pem Pointer to a `char_t*` that will receive the allocated PEM string. The caller is responsible for freeing this string.
 * @return 0 on success, -1 on failure (e.g., invalid input, encoding error).
 */
#define pem_write_secp384r1_private_key(in_priv, out_pem) \
        pem_write_ellipticcurve_private_key(ELLIPTICCURVE_CURVE_TYPE_SECP384R1, in_priv, out_pem)

/**
 * @brief Writes an elliptic curve public key to a PEM-encoded string.
 * This function takes a raw public key and encodes it into a PEM format string for the specified elliptic curve. The resulting PEM string will be in the format "PUBLIC KEY" and will contain the appropriate ASN.1 structure for the specified curve.
 * @param curve The elliptic curve curve to use for encoding (e.g., ELLIPTICCURVE_CURVE_TYPE_SECP256R1).
 * @param in_pub The raw public key bytes (size depends on the curve_type, e.g., 64 bytes for secp256r1, uncompressed format).
 * @param out_pem Pointer to a `char_t*` that will receive the allocated PEM string. The caller is responsible for freeing this string.
 * @return 0 on success, -1 on failure (e.g., invalid input, unsupported curve_type, encoding error).
 */
int8_t pem_write_ellipticcurve_public_key(ellipticcurve_curve_type_t curve_type,
                                          const uint8_t*             in_pub,
                                          char_t**                   out_pem);
/**
 * @brief Writes a secp256r1 public key to a PEM-encoded string.
 * This function takes a raw secp256r1 public key and encodes it into a PEM format string. The resulting PEM string will be in the format "PUBLIC KEY" and will contain the appropriate ASN.1 structure for secp256r1.
 * @param in_pub The raw public key bytes (64 bytes, uncompressed format).
 * @param out_pem Pointer to a `char_t*` that will receive the allocated PEM string. The caller is responsible for freeing this string.
 * @return 0 on success, -1 on failure (e.g., invalid input, encoding error).
 */
#define pem_write_secp256r1_public_key(in_pub, out_pem) \
        pem_write_ellipticcurve_public_key(ELLIPTICCURVE_CURVE_TYPE_SECP256R1, in_pub, out_pem)

/**
 * @brief Writes a secp384r1 public key to a PEM-encoded string.
 * This function takes a raw secp384r1 public key and encodes it into a PEM format string. The resulting PEM string will be in the format "PUBLIC KEY" and will contain the appropriate ASN.1 structure for secp384r1.
 * @param in_pub The raw public key bytes (96 bytes, uncompressed format).
 * @param out_pem Pointer to a `char_t*` that will receive the allocated PEM string. The caller is responsible for freeing this string.
 * @return 0 on success, -1 on failure (e.g., invalid input, encoding error).
 */
#define pem_write_secp384r1_public_key(in_pub, out_pem) \
        pem_write_ellipticcurve_public_key(ELLIPTICCURVE_CURVE_TYPE_SECP384R1, in_pub, out_pem)

#ifdef __cplusplus
}
#endif

#endif // ___ELLIPTICCURVE_H
