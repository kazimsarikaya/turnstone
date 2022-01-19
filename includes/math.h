/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___MATH_H
#define ___MATH_H 0

#include <utils.h>

#define EXP  ((float64_t)(2.71828182845904523536L))
#define LN2  ((float64_t)(0.69314718055994506418L))
#define LN10 ((float64_t)(2.30258509299404590109L))


/**
 * @brief power base with p
 * @param[in]  base the base
 * @param[in]  p the power
 * @return base^p with fast power algorithm
 */
float64_t math_power(float64_t base, float64_t p);

float64_t math_exp(float64_t number);
float64_t math_log(float64_t number);
#define math_log2(n) (math_log(n) / LN2)
#define math_log10(n) (math_log(n) / LN10)

float64_t math_antilog(float64_t power, float64_t exp);

float64_t math_root(float64_t number, float64_t root);

#endif
