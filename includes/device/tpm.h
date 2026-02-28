/**
 * @file tpm.h
 * @brief TPM (Trusted Platform Module) related functions and definitions.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___TPM_H
#define ___TPM_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tpm2_device_t tpm2_device_t;

typedef enum tpm_alg_t : uint16_t {
    TPM_ALG_ERROR     = 0x0000,
    TPM_ALG_RSA       = 0x0001,
    TPM_ALG_SHA1      = 0x0004,
    TPM_ALG_HMAC      = 0x0005,
    TPM_ALG_AES       = 0x0006,
    TPM_ALG_MGF1      = 0x0007,
    TPM_ALG_KEYEDHASH = 0x0008,
    TPM_ALG_XOR       = 0x000A,
    TPM_ALG_SHA256    = 0x000B,
    TPM_ALG_SHA384    = 0x000C,
    TPM_ALG_SHA512    = 0x000D,
    TPM_ALG_NULL      = 0x0010,
    TPM_ALG_ECDSA     = 0x0018,
    TPM_ALG_ECDH      = 0x0019,
    TPM_ALG_ECDAA     = 0x001A,
    TPM_ALG_SM2       = 0x001B,
    TPM_ALG_ECSCHNORR = 0x001C,
    TPM_ALG_KDF2      = 0x0021,
    TPM_ALG_ECC       = 0x0023,
    TPM_ALG_SYMCIPHER = 0x0025,
    TPM_ALG_SHA3_256  = 0x0027,
    TPM_ALG_SHA3_384  = 0x0028,
    TPM_ALG_SHA3_512  = 0x0029,
    TPM_ALG_CTR       = 0x0040,
    TPM_ALG_OFB       = 0x0041,
    TPM_ALG_CBC       = 0x0042,
    TPM_ALG_CFB       = 0x0043,
    TPM_ALG_ECB       = 0x0044,
} tpm_alg_t;

typedef enum tpm_ecc_curve_t : uint16_t {
    TPM_ECC_NONE      = 0x0000,
    TPM_ECC_NIST_P192 = 0x0001,
    TPM_ECC_NIST_P224 = 0x0002,
    TPM_ECC_NIST_P256 = 0x0003,
    TPM_ECC_NIST_P384 = 0x0004,
    TPM_ECC_NIST_P521 = 0x0005,
    TPM_ECC_BN_P256   = 0x0010,
    TPM_ECC_BN_P638   = 0x0011,
    TPM_ECC_SM2_P256  = 0x0020,
} tpm_ecc_curve_t;

int8_t tpm2_init(void);

int8_t tpm2_flush_context(uint32_t handle);

int8_t tpm2_start_auth_session(uint32_t* session_handle);
int8_t tpm2_get_random(uint8_t* buffer, uint16_t requested_sz);

int8_t tpm2_find_ecc_key_with_point(tpm_ecc_curve_t curve_id, const uint8_t* point, uint32_t* handle);
int8_t tpm2_sign_ecc_with_hash(uint32_t handle, tpm_alg_t digest_alg, const uint8_t* digest, uint8_t** sig, size_t* sig_len);

int8_t tpm2_load_key(uint32_t parent_handle,
                     const uint8_t* private_key, size_t private_len,
                     const uint8_t* public_key, size_t public_len,
                     uint32_t* loaded_handle);

#ifdef __cplusplus
}
#endif

#endif
