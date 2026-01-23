/**
 * @file bigint.h
 * @brief Big Integer Library for Turnstone OS.
 *
 * This library provides arbitrary-precision arithmetic. All functions returning
 * int8_t follow the convention: 0 for success, non-zero for error.
 * * This work is licensed under TURNSTONE OS Public License.
 */

#ifndef ___BIGINT_H
#define ___BIGINT_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! @typedef bigint_t
 * @brief Opaque structure representing a big integer instance.
 */
typedef struct bigint_t bigint_t;

/*! @def BIGINT_CHECK_RESULT
 * @brief Attribute to enforce checking of function return values.
 */
#define BIGINT_CHECK_RESULT __attribute__((warn_unused_result))

/**
 * @brief Deallocates a bigint instance and its internal memory.
 * @param bigint Pointer to the bigint instance to destroy.
 */
void __attribute__((no_reorder)) bigint_destroy(bigint_t* bigint);

/**
 * @brief Allocates and initializes a new bigint instance.
 * @return Pointer to new bigint_t, or NULL on failure.
 */
BIGINT_CHECK_RESULT bigint_t* bigint_create(void);

/**
 * @brief Creates a bigint as 0.
 * @return Bigint with value 0. */
BIGINT_CHECK_RESULT bigint_t* bigint_zero(void);

/**
 * @brief Creates a bigint as 1.
 * @return Bigint with value 1. */
BIGINT_CHECK_RESULT bigint_t* bigint_one(void);

/**
 * @brief Creates a bigint as 2.
 * @return Bigint with value 2. */
BIGINT_CHECK_RESULT bigint_t* bigint_two(void);

/**
 * @brief Creates a deep copy of a bigint.
 * @param src Source bigint to clone.
 * @return New bigint instance on success, NULL on failure.
 */
BIGINT_CHECK_RESULT bigint_t* bigint_clone(const bigint_t* src);

/**
 * @brief Generates a random bigint of specified bit length.
 * @param bits Number of bits for the random number.
 * @return New random bigint instance on success, NULL on failure.
 */
BIGINT_CHECK_RESULT bigint_t* bigint_random(uint64_t bits);

/**
 * @brief Generates a random bigint within [min, max].
 * @param min Lower bound (inclusive).
 * @param max Upper bound (inclusive).
 * @return New random bigint instance on success, NULL on failure.
 */
BIGINT_CHECK_RESULT bigint_t* bigint_random_range(const bigint_t* min, const bigint_t* max);

/**
 * @brief Generates a random prime number of specified bit length.
 * @param bits Number of bits for the prime number.
 * @return New prime bigint instance on success, NULL on failure.
 */
BIGINT_CHECK_RESULT bigint_t* bigint_random_prime(uint64_t bits);

/**
 * @brief Sets bigint value to 0.
 * @param bigint The bigint instance to modify.
 * @return 0 on success, non-zero on error.
 */
BIGINT_CHECK_RESULT int8_t bigint_set_zero(bigint_t* bigint);

/**
 * @brief Sets bigint value from a string.
 * @param bigint The bigint instance to modify.
 * @param str The numeric string (decimal or hex).
 * @return 0 on success, non-zero on error.
 */
BIGINT_CHECK_RESULT int8_t bigint_set_str(bigint_t* bigint, const char_t* str);

/**
 * @brief Sets bigint value from a signed 64-bit integer.
 * @param bigint The bigint instance to modify.
 * @param value The 64-bit value to set.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_set_int64(bigint_t* bigint, int64_t value);

/**
 * @brief Sets bigint value from an unsigned 64-bit integer.
 * @param bigint The bigint instance to modify.
 * @param value The unsigned 64-bit value to set.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_set_uint64(bigint_t* bigint, uint64_t value);

/**
 * @brief Sets bigint value from another bigint (copy).
 * @param bigint The destination bigint.
 * @param src The source bigint.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_set_bigint(bigint_t* bigint, const bigint_t* src);

/**
 * @brief Sets specific bit at index to value.
 * @param bigint The bigint instance.
 * @param bit The index of the bit.
 * @param value TRUE for 1, FALSE for 0.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_set_bit(bigint_t* bigint, uint64_t bit, boolean_t value);

/**
 * @brief Gets value of bit at specific index.
 * @param bigint The bigint instance.
 * @param bit The index of the bit.
 * @param value Pointer to store the bit value.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_get_bit(const bigint_t* bigint, uint64_t bit, boolean_t* value);

/**
 * @brief Flips bit at specific index.
 * @param bigint The bigint instance.
 * @param bit The index of the bit to flip.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_flip_bit(bigint_t* bigint, uint64_t bit);

/**
 * @brief Clears bit at specific index.
 * @param bigint The bigint instance.
 * @param bit The index of the bit to clear.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_clear_bit(bigint_t* bigint, uint64_t bit);

/**
 * @brief Converts bigint to string.
 * @param bigint The bigint instance.
 * @return Heap-allocated string, or NULL on failure.
 */
char_t* bigint_to_str(const bigint_t* bigint);

/**
 * @brief Gets bit length of bigint.
 * @param bigint The bigint instance.
 * @return Total number of bits.
 */
uint64_t bigint_bit_length(const bigint_t* bigint);

/**
 * @brief result = -a
 * @param result The bigint to store the result.
 * @param a The source bigint.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_neg(bigint_t* result, const bigint_t* a);

/**
 * @brief result = a + b
 * @param result The destination bigint.
 * @param a First operand.
 * @param b Second operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_add(bigint_t* result, const bigint_t* a, const bigint_t* b);

/**
 * @brief result = a - b
 * @param result The destination bigint.
 * @param a First operand.
 * @param b Second operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_sub(bigint_t* result, const bigint_t* a, const bigint_t* b);

/**
 * @brief result = a * b
 * @param result The destination bigint.
 * @param a First operand.
 * @param b Second operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_mul(bigint_t* result, const bigint_t* a, const bigint_t* b);

/**
 * @brief result = a / b
 * @param result The destination bigint.
 * @param a Dividend.
 * @param b Divisor.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_div(bigint_t* result, const bigint_t* a, const bigint_t* b);

/**
 * @brief result = a / b, remainder = a % b
 * @param result Destination for quotient.
 * @param remainder Destination for remainder.
 * @param a Dividend.
 * @param b Divisor.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_div_with_remainder(bigint_t* result, bigint_t* remainder, const bigint_t* a, const bigint_t* b);

/**
 * @brief Unsigned division: result = a / b, remainder = a % b
 * @param result Destination for quotient.
 * @param remainder Destination for remainder.
 * @param a Dividend.
 * @param b Divisor.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_div_unsigned(bigint_t* result, bigint_t* remainder, const bigint_t* a, const bigint_t* b);

/**
 * @brief result = a % b
 * @param result Destination for result.
 * @param a Dividend.
 * @param b Divisor.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_mod(bigint_t* result, const bigint_t* a, const bigint_t* b);

/**
 * @brief result = a ^ b
 * @param result Destination for result.
 * @param a Base.
 * @param b Exponent.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_pow(bigint_t* result, const bigint_t* a, const bigint_t* b);

/**
 * @brief result = gcd(a, b)
 * @param result Destination for result.
 * @param a First operand.
 * @param b Second operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_gcd(bigint_t* result, const bigint_t* a, const bigint_t* b);

/**
 * @brief result = floor(sqrt(a))
 * @param result Destination for result.
 * @param a Operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_isqrt(bigint_t* result, const bigint_t* a);

/**
 * @brief result = a^-1 mod n
 * @param result Destination for result.
 * @param a Base.
 * @param n Modulus.
 * @return 0 on success, non-zero if inverse does not exist.
 */
BIGINT_CHECK_RESULT int8_t bigint_mod_inv(bigint_t* result, const bigint_t* a, const bigint_t* n);

/**
 * @brief a = a + b (uint64)
 * @param a Destination and first operand.
 * @param b 64-bit operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_add_uint64(bigint_t* a, uint64_t b);

/**
 * @brief a = a - b (uint64)
 * @param a Destination and first operand.
 * @param b 64-bit operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_sub_uint64(bigint_t* a, uint64_t b);

/**
 * @brief a = a * b (uint64)
 * @param a Destination and first operand.
 * @param b 64-bit operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_mul_uint64(bigint_t* a, uint64_t b);

/**
 * @brief Calculates a % m (uint64)
 * @param a Dividend.
 * @param m 64-bit divisor.
 * @return Remainder as uint64_t.
 */
uint64_t bigint_mod_uint64(const bigint_t* a, uint64_t m);

/**
 * @brief result = (a + b) mod m
 * @param result Destination.
 * @param a First operand.
 * @param b Second operand.
 * @param m Modulus.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_add_mod(bigint_t* result, const bigint_t* a, const bigint_t* b, const bigint_t* m);

/**
 * @brief result = (a - b) mod m
 * @param result Destination.
 * @param a First operand.
 * @param b Second operand.
 * @param m Modulus.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_sub_mod(bigint_t* result, const bigint_t* a, const bigint_t* b, const bigint_t* m);

/**
 * @brief result = (a * b) mod m
 * @param result Destination.
 * @param a First operand.
 * @param b Second operand.
 * @param m Modulus.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_mul_mod(bigint_t* result, const bigint_t* a, const bigint_t* b, const bigint_t* m);

/**
 * @brief result = (a ^ b) mod m
 * @param result Destination.
 * @param a Base.
 * @param b Exponent.
 * @param m Modulus.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_pow_mod(bigint_t* result, const bigint_t* a, const bigint_t* b, const bigint_t* m);

/**
 * @brief Bitwise AND: result = a & b
 * @param result Destination.
 * @param a First operand.
 * @param b Second operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_and(bigint_t* result, const bigint_t* a, const bigint_t* b);

/**
 * @brief Bitwise OR: result = a | b
 * @param result Destination.
 * @param a First operand.
 * @param b Second operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_or(bigint_t* result, const bigint_t* a, const bigint_t* b);

/**
 * @brief Bitwise XOR: result = a ^ b
 * @param result Destination.
 * @param a First operand.
 * @param b Second operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_xor(bigint_t* result, const bigint_t* a, const bigint_t* b);

/**
 * @brief Bitwise NOT: result = ~a
 * @param result Destination.
 * @param a Operand.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_not(bigint_t* result, const bigint_t* a);

/**
 * @brief Left shift: result = a << shift
 * @param result Destination.
 * @param a Operand.
 * @param shift Number of bits to shift.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_shl(bigint_t* result, const bigint_t* a, int64_t shift);

/**
 * @brief Right shift: result = a >> shift
 * @param result Destination.
 * @param a Operand.
 * @param shift Number of bits to shift.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_shr(bigint_t* result, const bigint_t* a, int64_t shift);

/**
 * @brief Left shift by one bit.
 * @param a Operand to be shifted in place.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_shl_one(bigint_t* a);

/**
 * @brief Right shift by one bit.
 * @param a Operand to be shifted in place.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_shr_one(bigint_t* a);

/**
 * @brief Compares two bigints.
 * @param a First bigint.
 * @param b Second bigint.
 * @return -1 if a < b, 0 if a == b, 1 if a > b.
 */
int8_t bigint_cmp(const bigint_t* a, const bigint_t* b);

/**
 * @brief Checks if bigint is zero.
 * @param a The bigint instance.
 * @return TRUE if value is 0.
 */
boolean_t bigint_is_zero(const bigint_t* a);

/**
 * @brief Checks if bigint is negative.
 * @param a The bigint instance.
 * @return TRUE if value < 0.
 */
boolean_t bigint_is_negative(const bigint_t* a);

/**
 * @brief Checks if bigint is odd.
 * @param a The bigint instance.
 * @return TRUE if value is odd.
 */
boolean_t bigint_is_odd(const bigint_t* a);

/**
 * @brief Checks if bigint is even.
 * @param a The bigint instance.
 * @return TRUE if value is even.
 */
boolean_t bigint_is_even(const bigint_t* a);

/**
 * @brief Checks if bigint value equals int64_t.
 * @param a The bigint instance.
 * @param value The value to compare against.
 * @return TRUE if equal.
 */
boolean_t bigint_is_int64(const bigint_t* a, int64_t value);

/**
 * @brief Checks if bigint value equals uint64_t.
 * @param a The bigint instance.
 * @param value The value to compare against.
 * @return TRUE if equal.
 */
boolean_t bigint_is_uint64(const bigint_t* a, uint64_t value);

/**
 * @brief Checks if bigint is prime.
 * @param a The bigint instance.
 * @return TRUE if value is prime.
 */
boolean_t bigint_is_prime(const bigint_t* a);

/**
 * @brief Exports bigint to big-endian bytes.
 * @param a The bigint instance.
 * @param buf Buffer to store bytes.
 * @param len Length of the buffer.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_to_bytes(const bigint_t* a, uint8_t* buf, uint64_t len);

/**
 * @brief Imports bigint from big-endian bytes.
 * @param a The bigint instance.
 * @param buf Buffer containing bytes.
 * @param len Length of the buffer.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_from_bytes(bigint_t* a, const uint8_t* buf, uint64_t len);

/**
 * @brief Exports bigint to little-endian bytes.
 * @param a The bigint instance.
 * @param buf Buffer to store bytes.
 * @param len Length of the buffer.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_to_bytes_le(const bigint_t* a, uint8_t* buf, uint64_t len);

/**
 * @brief Imports bigint from little-endian bytes.
 * @param a The bigint instance.
 * @param buf Buffer containing bytes.
 * @param len Length of the buffer.
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_from_bytes_le(bigint_t* a, const uint8_t* buf, uint64_t len);

/**
 * @brief Constant-time conditional swap.
 * Swaps a and b if swap is 1, does nothing if 0.
 * @param a First bigint.
 * @param b Second bigint.
 * @param swap Control flag (0 or 1).
 * @return 0 on success.
 */
BIGINT_CHECK_RESULT int8_t bigint_cswap(bigint_t* a, bigint_t* b, uint8_t swap);

#ifdef __cplusplus
}
#endif

#endif /* ___BIGINT_H */
