/**
 * @file utils.h
 * @brief util functions and macros
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___VARINT_H
/*! prevent duplicate header error macro */
#define ___VARINT_H 0

#include <types.h>

uint8_t* varint_encode(uint64_t num, int8_t* size);
uint64_t varint_decode(uint8_t* data, int8_t* size);

#endif