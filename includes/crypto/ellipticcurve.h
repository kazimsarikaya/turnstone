/**
 * @file ellipticcurve.h
 * @brief Elliptic Curve Cryptography (ECC) Header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___ELLIPTICCURVE_H
#define ___ELLIPTICCURVE_H 0

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

#define ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN 32
#define ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN 64
#define ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN 64
#define ELLIPTICCURVE_SECP256R1_SHARED_SECRET_LEN 32

int8_t ellipticcurve_secp256r1_derive_public_key(const uint8_t priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN],
                                                 uint8_t       out_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]);

int8_t ellipticcurve_secp256r1_generate_keypair(uint8_t out_priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN],
                                                uint8_t out_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]);

int8_t ellipticcurve_secp256r1_sign(uint8_t sig[ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN],
                                    const uint8_t* msg, size_t msg_len,
                                    const uint8_t priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN]);

int8_t ellipticcurve_secp256r1_verify(const uint8_t sig[ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN],
                                      const uint8_t* msg, size_t msg_len,
                                      const uint8_t pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]);

int8_t ellipticcurve_secp256r1_compute_shared_secret(uint8_t       shared_secret[ELLIPTICCURVE_SECP256R1_SHARED_SECRET_LEN],
                                                     const uint8_t priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN],
                                                     const uint8_t pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]);

int8_t pem_read_secp256r1_private_key(const char_t* pem,
                                      uint8_t       out_priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN]);
int8_t pem_read_secp256r1_public_key(const char_t* pem,
                                     uint8_t       out_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]);
int8_t pem_write_secp256r1_private_key(const uint8_t in_priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN],
                                       char_t**      out_pem);
int8_t pem_write_secp256r1_public_key(const uint8_t in_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN],
                                      char_t**      out_pem);

#ifdef __cplusplus
}
#endif

#endif // ___ELLIPTICCURVE_H
