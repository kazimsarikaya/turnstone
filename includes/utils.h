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

#endif
