/**
 * @file aes-gcm.h
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___AES_GCM_HEADER_H
#define ___AES_GCM_HEADER_H

#include <gcm.h>

int32_t aes_gcm_encrypt(uint8_t* output, const uint8_t* input, int32_t input_length, const uint8_t* key, const size_t key_len, const uint8_t* iv, const size_t iv_len);

int32_t aes_gcm_decrypt(uint8_t* output, const uint8_t* input, int32_t input_length, const uint8_t* key, const size_t key_len, const uint8_t* iv, const size_t iv_len);

#endif
