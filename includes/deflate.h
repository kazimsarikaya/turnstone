/**
 * @file deflate.h
 * @brief deflate compression algorithm
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */



#ifndef ___DEFLATE_H
/*! prevent duplicate header error macro */
#define ___DEFLATE_H 0

#include <compression.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t deflate_deflate(buffer_t* in, buffer_t* out);
int8_t deflate_inflate(buffer_t* in, buffer_t* out);

#ifdef __cplusplus
}
#endif

#endif
