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

int8_t tpm2_init(void);

int8_t tpm2_get_random(uint8_t* buffer, uint16_t requested_sz);

#ifdef __cplusplus
}
#endif

#endif
