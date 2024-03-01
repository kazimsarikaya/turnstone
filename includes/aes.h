/**
 * @file aes.h
 * @brief Advanced Encryption Standard (AES) implementation header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___AES_HEADER_H
#define ___AES_HEADER_H

#include <types.h>

#define AES_ENCRYPT         1
#define AES_DECRYPT         0


void aes_init_keygen_tables(void);


typedef struct aes_context_t {
    int32_t    mode;
    int32_t    rounds;
    uint32_t * rk;
    uint32_t   buf[68];
} aes_context_t;


int aes_setkey(aes_context_t* ctx, int32_t mode, const uint8_t* key, uint32_t keysize );

int aes_cipher(aes_context_t* ctx, const uint8_t input[16], uint8_t output[16]);


#endif
