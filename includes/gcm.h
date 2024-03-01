/**
 * @file gcm.h
 * @brief Galois/Counter Mode (GCM) implementation header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___GCM_HEADER_H
#define ___GCM_HEADER_H

#define GCM_AUTH_FAILURE    0x55555555

#include <aes.h>

typedef struct gcm_context_t {
    int32_t       mode;
    uint64_t      len;
    uint64_t      add_len;
    uint64_t      HL[16];
    uint64_t      HH[16];
    uint8_t       base_ectr[16];
    uint8_t       y[16];
    uint8_t       buf[16];
    aes_context_t aes_ctx;
} gcm_context_t;


int gcm_initialize(void);

int gcm_setkey(gcm_context_t* ctx, const uint8_t* key, const uint32_t keysize);

int gcm_crypt_and_tag(gcm_context_t* ctx, int32_t mode, const uint8_t* iv, size_t iv_len, const uint8_t* add, size_t add_len, const uint8_t * input, uint8_t* output, size_t length, uint8_t * tag, size_t tag_len);

int gcm_auth_decrypt(gcm_context_t* ctx, const uint8_t* iv, size_t iv_len, const uint8_t* add, size_t add_len, const uint8_t* input, uint8_t* output, size_t length, const uint8_t* tag, size_t tag_len);

int gcm_start(gcm_context_t* ctx, int32_t mode, const uint8_t* iv, size_t iv_len, const uint8_t* add, size_t add_len);

int gcm_update(gcm_context_t* ctx, size_t length, const uint8_t* input, uint8_t* output);

int gcm_finish(gcm_context_t* ctx, uint8_t* tag, size_t tag_len);

void gcm_zero_ctx(gcm_context_t * ctx );


#endif
