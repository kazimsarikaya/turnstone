/**
 * @file aes-gcm.64.c
 * @brief AES-GCM encryption and decryption functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <aes-gcm.h>

MODULE("turnstone.lib.crypto");

int32_t aes_gcm_encrypt(uint8_t* output, const uint8_t* input, int32_t input_length, const uint8_t* key, const size_t key_len, const uint8_t * iv, const size_t iv_len){

    int32_t ret = 0;
    gcm_context_t ctx;

    size_t tag_len = 0;
    uint8_t * tag_buf = NULL;

    gcm_setkey( &ctx, key, key_len );

    ret = gcm_crypt_and_tag( &ctx, AES_ENCRYPT, iv, iv_len, NULL, 0,
                             input, output, input_length, tag_buf, tag_len);

    gcm_zero_ctx( &ctx );

    return( ret );
}

int32_t aes_gcm_decrypt(uint8_t* output, const uint8_t* input, int32_t input_length, const uint8_t* key, const size_t key_len, const uint8_t * iv, const size_t iv_len){

    int32_t ret = 0;
    gcm_context_t ctx;

    size_t tag_len = 0;
    uint8_t * tag_buf = NULL;

    gcm_setkey( &ctx, key, key_len );

    ret = gcm_crypt_and_tag( &ctx, AES_DECRYPT, iv, iv_len, NULL, 0,
                             input, output, input_length, tag_buf, tag_len);

    gcm_zero_ctx( &ctx );

    return( ret );

}
