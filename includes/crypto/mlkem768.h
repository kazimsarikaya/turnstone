/**
 * @file mlkem768.h
 * @brief ML-KEM-768 implementation header
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___MLKEM768_H
#define ___MLKEM768_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>


#define MLKEM768_SHARED_SECRET_BYTES 32
#define MLKEM768_PUBLICKEYBYTES 1184 // 3 polynomials of 384 bytes each + 32 bytes for seed = 1152 + 32 = 1184 bytes
#define MLKEM768_CIPHERTEXTBYTES  1088 // 960 bytes for u + 128 bytes for v = 1088 bytes
#define MLKEM768_SECRETKEYBYTES 2400 // 3 polynomials of 384 bytes each + 1184 bytes for public key + 32 bytes for H(pk) + 32 bytes for z = 1152 + 1184 + 32 + 32 = 2400 bytes

void mlkem768_keygen(uint8_t * pk, uint8_t * sk);
void mlkem768_encaps(uint8_t * ct, uint8_t * ss, const uint8_t * pk);
void mlkem768_decaps(uint8_t * ss, const uint8_t * ct, const uint8_t * sk);


#ifdef __cplusplus
}
#endif

#endif
