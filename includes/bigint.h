/**
 * @file bigint.h
 * @brief Big Integer Library
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___BIGINT_H
#define ___BIGINT_H

#include <types.h>

typedef struct bigint_t bigint_t;


bigint_t* bigint_create(void);
void      bigint_destroy(bigint_t* bigint);

int8_t bigint_set_str(bigint_t* bigint, const char_t* str);
int8_t bigint_set_int64(bigint_t* bigint, int64_t value);
int8_t bigint_set_uint64(bigint_t* bigint, uint64_t value);
int8_t bigint_set_bigint(bigint_t* bigint, const bigint_t* src);

int8_t bigint_set_bit(bigint_t* bigint, uint64_t bit, boolean_t value);
int8_t bigint_get_bit(const bigint_t* bigint, uint64_t bit, boolean_t* value);
int8_t bigint_flip_bit(bigint_t* bigint, uint64_t bit);
int8_t bigint_clear_bit(bigint_t* bigint, uint64_t bit);

const char_t* bigint_to_str(const bigint_t* bigint);

uint64_t bigint_bit_length(const bigint_t* bigint);

int8_t bigint_neg(bigint_t* result, const bigint_t* a);
int8_t bigint_add(bigint_t* result, const bigint_t* a, const bigint_t* b);
int8_t bigint_sub(bigint_t* result, const bigint_t* a, const bigint_t* b);
int8_t bigint_mul(bigint_t* result, const bigint_t* a, const bigint_t* b);
int8_t bigint_div(bigint_t* result, const bigint_t* a, const bigint_t* b);
int8_t bigint_div_unsigned(bigint_t* result, const bigint_t* a, const bigint_t* b);
int8_t bigint_mod(bigint_t* result, const bigint_t* a, const bigint_t* b);
int8_t bigint_pow(bigint_t* result, const bigint_t* a, const bigint_t* b);
int8_t bigint_gcd(bigint_t* result, const bigint_t* a, const bigint_t* b);

int8_t bigint_and(bigint_t* result, const bigint_t* a, const bigint_t* b);
int8_t bigint_or(bigint_t* result, const bigint_t* a, const bigint_t* b);
int8_t bigint_xor(bigint_t* result, const bigint_t* a, const bigint_t* b);
int8_t bigint_not(bigint_t* result, const bigint_t* a);

int8_t bigint_shl(bigint_t* result, const bigint_t* a, int64_t shift);
int8_t bigint_shr(bigint_t* result, const bigint_t* a, int64_t shift);

int8_t bigint_cmp(const bigint_t* a, const bigint_t* b);

boolean_t bigint_is_zero(const bigint_t* a);
boolean_t bigint_is_negative(const bigint_t* a);
boolean_t bigint_is_odd(const bigint_t* a);
boolean_t bigint_is_prime(const bigint_t* a);

#endif
