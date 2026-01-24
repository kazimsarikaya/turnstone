/**
 * @file varint.h
 * @brief Variable length integer encoding/decoding header.
 *
 * This header defines functions for encoding and decoding variable-length integers.
 * Variable-length integers are a common technique for representing integers
 * in a way that uses fewer bytes for smaller numbers, making data more compact.
 * This implementation is based on the standard varint encoding scheme.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___VARINT_H
/*! prevent duplicate header error macro */
#define ___VARINT_H 0

#include <types.h> // Assuming types.h defines uint8_t, uint64_t, int8_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encodes a 64-bit unsigned integer into a variable-length byte array.
 *
 * This function takes a 64-bit unsigned integer and converts it into its
 * variable-length representation. The encoded data is written to the buffer
 * pointed to by `num`. The actual size of the encoded data is returned via
 * the `size` parameter.
 *
 * @param num The 64-bit unsigned integer to encode.
 * @param size A pointer to an 8-bit signed integer where the size of the
 *             encoded data (in bytes) will be stored. This parameter can be NULL
 *             if the size is not needed.
 * @return A pointer to the first byte of the encoded variable-length integer.
 *         The caller is responsible for managing the memory of the returned buffer.
 *         It is recommended to allocate sufficient space beforehand, or to use
 *         a dynamically growing buffer if the maximum size is unknown.
 */
uint8_t* varint_encode(uint64_t num, int8_t* size);

/**
 * @brief Decodes a variable-length byte array into a 64-bit unsigned integer.
 *
 * This function takes a byte array containing a variable-length encoded integer
 * and decodes it back into a 64-bit unsigned integer. The number of bytes
 * consumed from the input data is returned via the `size` parameter.
 *
 * @param data A pointer to the byte array containing the variable-length encoded integer.
 * @param size A pointer to an 8-bit signed integer where the number of bytes
 *             consumed from `data` during decoding will be stored. This parameter
 *             can be NULL if the consumed size is not needed.
 * @return The decoded 64-bit unsigned integer.
 */
uint64_t varint_decode(uint8_t* data, int8_t* size);

#ifdef __cplusplus
}
#endif

#endif
