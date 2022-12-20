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

size_t base64_encode(const uint8_t* in, size_t len, boolean_t add_newline, uint8_t** out);
size_t base64_decode(const uint8_t* in, size_t len, uint8_t** out);

#endif
