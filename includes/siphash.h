/**
 * @file siphash.h
 * @brief sip hash interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___SIPHASH_H
/*! prevent duplicate header error macro */
#define ___SIPHASH_H 0

#include <types.h>

uint128_t siphash128(const void* data, uint64_t len, uint128_t seed);

#endif
