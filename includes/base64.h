/**
 * @file base64.h
 * @brief base64 encoder decoder headers
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___BASE64_H
#define ___BASE64_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encodes a byte array into a Base64 string.
 *
 * This function takes a byte array as input and encodes it into a Base64
 * string. The resulting string is dynamically allocated and should be freed
 * by the caller.
 *
 * @param in Pointer to the input byte array.
 * @param len Length of the input byte array.
 * @param add_newline If TRUE, a newline character is appended to the encoded string.
 * @param out Pointer to a pointer that will store the dynamically allocated
 *            Base64 encoded string. This pointer will be NULL on failure.
 * @return The length of the encoded string (excluding null terminator), or 0 on failure.
 */
size_t base64_encode(const uint8_t* in, size_t len, boolean_t add_newline, uint8_t** out);

/**
 * @brief Decodes a Base64 string into a byte array.
 *
 * This function takes a Base64 encoded string as input and decodes it into
 * a byte array. The resulting byte array is dynamically allocated and should
 * be freed by the caller.
 *
 * @param in Pointer to the input Base64 encoded string.
 * @param len Length of the input Base64 encoded string.
 * @param out Pointer to a pointer that will store the dynamically allocated
 *            decoded byte array. This pointer will be NULL on failure.
 * @return The length of the decoded byte array, or 0 on failure.
 */
size_t base64_decode(const uint8_t* in, size_t len, uint8_t** out);

#ifdef __cplusplus
}
#endif

#endif
