/**
 * @file zpack.h
 * @brief z77 compression algorithm header

 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___ZPACK_H
/*! prevent duplicate header error macro */
#define ___ZPACK_H 0

#include <compression.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief packs data at input buffer to output buffer with z77 algorithm
 * @param[in] in input buffer
 * @param[in] out output buffer
 * @return size of output buffer
 */
int8_t zpack_pack(buffer_t* in, buffer_t* out);

/**
 * @brief unpacks data at input buffer to output buffer with z77 algorithm
 * @param[in] in input buffer
 * @param[in] out output buffer
 * @return size of output buffer
 */
int8_t zpack_unpack(buffer_t* in, buffer_t* out);

#ifdef __cplusplus
}
#endif

#endif
