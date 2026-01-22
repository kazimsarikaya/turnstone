/**
 * @file x25519.h
 * @brief X25519 Key Exchange Header.
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

#define X25519_PRIVATE_KEY_DER_LEN 48
#define X25519_PRIVATE_KEY_RAW_LEN 32
#define X25519_PUBLIC_KEY_DER_LEN 44
#define X25519_PUBLIC_KEY_RAW_LEN 32
#define X25519_SHARED_SECRET_LEN 32

int8_t pem_read_x25519_private_key(const char_t* pem, uint8_t* out_key);
int8_t pem_read_x25519_public_key(const char_t* pem, uint8_t* out_key);
int8_t pem_write_x25519_private_key(const uint8_t* in_key, char_t** out_pem);
int8_t pem_write_x25519_public_key(const uint8_t* in_key, char_t** out_pem);
void   x25519_clamp(uint8_t k[X25519_PRIVATE_KEY_RAW_LEN]);
int8_t x25519_derive_public(uint8_t out_pub[X25519_PUBLIC_KEY_RAW_LEN], const uint8_t in_priv[X25519_PRIVATE_KEY_RAW_LEN]);
int8_t x25519_generate_keypair(uint8_t out_priv[X25519_PRIVATE_KEY_RAW_LEN], uint8_t out_pub[X25519_PUBLIC_KEY_RAW_LEN]);
int8_t x25519_shared_secret(uint8_t       out_shared[X25519_SHARED_SECRET_LEN],
                            const uint8_t my_priv[X25519_PRIVATE_KEY_RAW_LEN],
                            const uint8_t their_pub[X25519_PUBLIC_KEY_RAW_LEN]);

#ifdef __cplusplus
}
#endif

#endif // ___X25519_H
