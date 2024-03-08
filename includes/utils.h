/**
 * @file utils.h
 * @brief util functions and macros
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___UTILS_H
/*! prevent duplicate header error macro */
#define ___UTILS_H 0

#include <types.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/**
 * find min of two numbers
 * @param[in]  x first number
 * @param[in]  y first number
 * @return  minimum of x or y
 */
#define MIN(x, y)  (x < y ? x : y)

/**
 * find max of two numbers
 * @param[in]  x first number
 * @param[in]  y first number
 * @return  maximum of x or y
 */
#define MAX(x, y)  (x > y ? x : y)

#define ABS(x)  (x >= 0?x:-1 * x)

/**
 * @brief power base with p
 * @param[in]  base the base
 * @param[in]  p the power
 * @return base^p with fast power algorithm
 */
number_t power(number_t base, number_t p);

#define sizeof_field(s, m) (sizeof((((s*)0)->m)))
#define typeof_field(s, m) (typeof(((s*)0)->m))
#define offsetof_field(s, m) ( ((uint64_t)&((s*)0)->m) )

/**
 * @brief converts integer to string
 * @param[in]  buffer destination buffer
 * @param[in]  number to convert
 * @param[in]  base to convert
 * @return 0 if successed
 *
 * buffer should be enough to take data. it's malloc free
 */
int8_t ito_base_with_buffer(char_t* buffer, number_t number, number_t base);

#define itoa_with_buffer(buf, number) ito_base_with_buffer(buf, number, 10)

/**
 * @brief converts unsigned integer to string
 * @param[in]  buffer destination buffer
 * @param[in]  number unsigned number to convert
 * @param[in]  base to convert
 * @return 0 if successed
 *
 * buffer should be enough to take data. it's malloc free
 */
int8_t uto_base_with_buffer(char_t* buffer, unumber_t number, number_t base);

#define utoa_with_buffer(buf, number) uto_base_with_buffer(buf, number, 10)
#define utoh_with_buffer(buf, number) uto_base_with_buffer(buf, number, 16)

/**
 * @brief converts float to string
 * @param[in]  buffer destination buffer
 * @param[in]  number to convert
 * @param[in]  base to convert
 * @return 0 if successed
 *
 * buffer should be enough to take data. it's malloc free
 */
int8_t fto_base_with_buffer(char_t* buffer, float64_t number, number_t prec, number_t base);

#define ftoa_with_buffer(buf, number) fto_base_with_buffer(buf, number, 6, 10)
#define ftoh_with_buffer(buf, number) fto_base_with_buffer(buf, number, 6, 16)
#define ftoa_with_buffer_and_prec(buf, number, prec) fto_base_with_buffer(buf, number, prec, 10)
#define ftoh_with_buffer_and_prec(buf, number, prec) fto_base_with_buffer(buf, number, prec, 16)

static inline uint64_t byte_swap(uint64_t num, uint8_t bc) {
    if(bc == 1) {
        return num;
    }

    uint64_t res = num;
    __asm__ __volatile__ ("bswap %0\n" : "=r" (res) : "0" (res));

    return res >> ((8 - bc) * 8);
}

#define BYTE_SWAP16(n) byte_swap(n, 2)
#define BYTE_SWAP32(n) byte_swap(n, 4)
#define BYTE_SWAP64(n) byte_swap(n, 8)

#define DIGIT_TO_HEX(r) (((r) < 10)?(r) + 48:(r) + 55)

#define ROTLEFT(a, b, c) (((a) << (b)) | ((a) >> (c - (b))))
#define ROTRIGHT(a, b, c) (((a) >> (b)) | ((a) << (c - (b))))
#define ROTLEFT8(a, b)         ROTLEFT(a, b, 8)
#define ROTRIGHT8(a, b)        ROTRIGHT(a, b, 8)
#define ROTLEFT16(a, b)        ROTLEFT(a, b, 16)
#define ROTRIGHT16(a, b)       ROTRIGHT(a, b, 16)
#define ROTLEFT32(a, b)        ROTLEFT(a, b, 32)
#define ROTRIGHT32(a, b)       ROTRIGHT(a, b, 32)
#define ROTLEFT64(a, b)        ROTLEFT((uint64_t)a, (uint64_t)b, 64ULL)
#define ROTRIGHT64(a, b)       ROTRIGHT((uint64_t)a, (uint64_t)b, 64ULL)

uint8_t byte_count(const uint64_t num);

/**
 * @brief test bit value of given data at bitloc
 * @param[in] data bit array
 * @param[in] bitloc bit location at data
 * @return bit value
 *
 **/
static inline boolean_t bit_test32(uint32_t* data, uint8_t bitloc) {
    boolean_t res = false;
    asm volatile ("bt %%ebx,(%%rax)" : "=@ccc" (res) : "a" (data), "b" (bitloc));
    return res;
}

/**
 * @brief clears bit value of given data at bitloc
 * @param[in] data bit array
 * @param[in] bitloc bit location at data
 * @return old value
 *
 **/
static inline boolean_t bit_clear32(volatile uint32_t* data, uint8_t bitloc) {
    boolean_t res = false;
    asm volatile ("btr %%ebx,(%%rax)" : "=@ccc" (res) : "a" (data), "b" (bitloc));
    return res;
}

/**
 * @brief sets bit value of given data at bitloc
 * @param[in] data bit array
 * @param[in] bitloc bit location at data
 * @return old value
 *
 **/
static inline boolean_t bit_set32(volatile uint32_t* data, uint8_t bitloc) {
    boolean_t res = false;
    asm volatile ("bts %%ebx,(%%rax)" : "=@ccc" (res) : "a" (data), "b" (bitloc));
    return res;
}

/**
 * @brief test bit value of given data at bitloc
 * @param[in] data bit array
 * @param[in] bitloc bit location at data
 * @return bit value
 *
 **/
static inline boolean_t bit_test(uint64_t* data, uint8_t bitloc) {
    boolean_t res = false;
    asm volatile ("bt %%rbx,(%%rax)" : "=@ccc" (res) : "a" (data), "b" (bitloc));
    return res;
}

/**
 * @brief clears bit value of given data at bitloc
 * @param[in] data bit array
 * @param[in] bitloc bit location at data
 * @return old value
 *
 **/
static inline boolean_t bit_clear(uint64_t* data, uint8_t bitloc) {
    boolean_t res = false;
    asm volatile ("btr %%rbx,(%%rax)" : "=@ccc" (res) : "a" (data), "b" (bitloc));
    return res;
}

/**
 * @brief sets bit value of given data at bitloc
 * @param[in] data bit array
 * @param[in] bitloc bit location at data
 * @return old value
 *
 **/
static inline boolean_t bit_set(uint64_t* data, uint8_t bitloc) {
    boolean_t res = false;
    asm volatile ("bts %%rbx,(%%rax)" : "=@ccc" (res) : "a" (data), "b" (bitloc));
    return res;
}

/**
 * @brief changes bit value of given data at bitloc
 * @param[in] data bit array
 * @param[in] bitloc bit location at data
 * @return old value
 *
 **/
static inline boolean_t bit_change(uint64_t* data, uint8_t bitloc) {
    boolean_t res = false;
    asm volatile ("btc %%rbx,(%%rax)" : "=@ccc" (res) : "a" (data), "b" (bitloc));
    return res;
}

/**
 * @brief gets most significant bit location
 * @param[in] num number to get
 * @return bit location
 *
 **/
static inline uint64_t bit_most_significant(uint64_t num) {
    uint64_t res = 0;
    boolean_t zf = false;
    asm volatile ("bsrq %2, %0" : "=r" (res), "=@ccz" (zf) : "r" (num));

    if(zf) {
        res = 0;
    }

    return res;
}


uint64_t __attribute__((noinline, optimize("O0"))) read_memio(uint64_t va, uint8_t size);
void __attribute__((noinline, optimize("O0")))     write_memio(uint64_t va, uint64_t val, uint8_t size);

static inline uint64_t reverse_bits(uint64_t bits, uint8_t bit_count) {
    int16_t result = 0;

    for (uint8_t i = 0; i < bit_count; i++) {
        result <<= 1;
        result |= bits & 1;
        bits >>= 1;
    }

    return result;
}

boolean_t isalpha(char_t c);
boolean_t isdigit(char_t c);
boolean_t isalnum(char_t c);
boolean_t isxdigit(char_t c);
boolean_t islower(char_t c);
boolean_t isupper(char_t c);
boolean_t isspace(char_t c);
boolean_t ispunct(char_t c);
boolean_t isprint(char_t c);
boolean_t isgraph(char_t c);
boolean_t iscntrl(char_t c);
boolean_t isblank(char_t c);
boolean_t isascii(char_t c);
boolean_t isalnumw(char_t c);

const char_t* randstr(uint32_t len);

#endif
