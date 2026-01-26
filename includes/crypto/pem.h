/**
 * @file pem.h
 * @brief PEM (Privacy-Enhanced Mail) format handling.
 *
 * This module provides functions for encoding and decoding data into the
 * PEM (Privacy-Enhanced Mail) format, which is commonly used for storing
 * cryptographic keys and certificates. PEM encoding involves Base64 encoding
 * the DER (Distinguished Encoding Rules) data and wrapping it with specific
 * header and footer markers.
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

/**
 * @brief Encodes DER data into PEM format.
 *
 * This function takes raw DER (Distinguished Encoding Rules) encoded data
 * and converts it into the PEM (Privacy-Enhanced Mail) format. It adds
 * standard PEM headers and footers (e.g., "-----BEGIN TYPE-----" and
 * "-----END TYPE-----") and Base64 encodes the DER data, inserting newlines
 * for readability according to RFC 7468.
 *
 * @param header The header string for the PEM block (e.g., "CERTIFICATE", "PRIVATE KEY").
 *               This string will be used in the BEGIN and END markers.
 * @param der_data Pointer to the DER encoded data buffer.
 * @param der_length The length of the DER encoded data buffer in bytes.
 * @param out_pem Pointer to a `char_t*` where the newly allocated PEM encoded string will be stored.
 *                The caller is responsible for freeing this memory using `memory_free()`.
 * @param out_pem_length Pointer to a `size_t` where the length of the generated PEM string (including null terminator) will be stored.
 * @return 0 on success, -1 on failure (e.g., memory allocation error, invalid input).
 */
int8_t pem_encode(const char_t* header,
                  const uint8_t* der_data, size_t der_length,
                  char_t** out_pem, size_t* out_pem_length);

/**
 * @brief Decodes PEM formatted data into DER format.
 *
 * This function takes a PEM formatted string and extracts the DER (Distinguished
 * Encoding Rules) encoded data. It verifies the PEM headers and footers,
 * decodes the Base64 encoded content, and returns the raw DER data.
 *
 * @param header The expected header string for the PEM block (e.g., "CERTIFICATE", "PRIVATE KEY").
 *               This string must match the header used in the PEM markers.
 * @param pem_data Pointer to the PEM encoded data string.
 * @param pem_length The length of the PEM encoded data string.
 * @param out_der_data Pointer to a `uint8_t*` where the newly allocated DER encoded data buffer will be stored.
 *                     The caller is responsible for freeing this memory using `memory_free()`.
 * @param out_der_length Pointer to a `size_t` where the length of the DER encoded data buffer will be stored.
 * @return 0 on success, -1 on failure (e.g., invalid PEM format, header mismatch, memory allocation error).
 */
int8_t pem_decode(const char_t* header,
                  const char_t* pem_data, size_t pem_length,
                  uint8_t** out_der_data, size_t* out_der_length);

#ifdef __cplusplus
}
#endif

#endif /* ___PEM_H */
