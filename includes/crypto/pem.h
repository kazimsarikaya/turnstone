/**
 * @file pem.h
 * @brief PEM (Privacy-Enhanced Mail) format handling.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___PEM_H
#define ___PEM_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t pem_encode(const char_t* header,
                  const uint8_t* der_data, size_t der_length,
                  char_t** out_pem, size_t* out_pem_length);

int8_t pem_decode(const char_t* header,
                  const char_t* pem_data, size_t pem_length,
                  uint8_t** out_der_data, size_t* out_der_length);

#ifdef __cplusplus
}
#endif

#endif /* ___PEM_H */
