/**
 * @file utils.h
 * @brief util functions and macros
 */
#ifndef ___UTILS_H
/*! prevent duplicate header error macro */
#define ___UTILS_H 0

#include <types.h>

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

/**
 * @brief power base with p
 * @param[in]  base the base
 * @param[in]  p the power
 * @return base^p with fast power algorithm
 */
number_t power(number_t base, number_t p);

#define sizeof_field(s, m) (sizeof((((s*)0)->m)))
#define typeof_field(s, m) typeof(((s*)0)->m)

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

#endif
