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
#define itoh_with_buffer(buf, number) ito_base_with_buffer(buf, number, 16)

#endif
