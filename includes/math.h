/**
 * @file math.h
 * @brief Math header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___MATH_H
#define ___MATH_H 0

#include <utils.h>

#define EXP    ((float64_t)(2.71828182845904523536L))
#define LN2    ((float64_t)(0.69314718055994506418L))
#define LN10   ((float64_t)(2.30258509299404590109L))
#define PI     ((float64_t)(3.14159265358979323846L))
#define PI_2   ((float64_t)(1.57079632679489661923L))
#define PI_1_2 ((float64_t)(0.318309886183790671538L))
#define PI_2_2 ((float64_t)(0.636619772367581343076L))

/**
 * @brief ceil of given float number
 * @param[in] num given float number
 * @return ceiled value
 *
 */
int64_t math_ceil(float64_t num);

/**
 * @brief floor of given float number
 * @param[in] num given float number
 * @return floored value
 *
 */
int64_t math_floor(float64_t num);


/**
 * @brief power base with p
 * @param[in]  base the base
 * @param[in]  p the power
 * @return base^p with fast power algorithm
 */
float64_t math_power(float64_t base, float64_t p);

/**
 * @brief calculates exp^number
 * @param[in] number number to calculate
 * @return exp^number
 */
float64_t math_exp(float64_t number);

/**
 * @brief calculates log given number at base exp
 * @param[in] number number to calculate
 * @return log value at base exp
 */
float64_t math_log(float64_t number);

///! log2 of n
#define math_log2(n) (math_log(n) / LN2)
///! log10 of n
#define math_log10(n) (math_log(n) / LN10)

/**
 * @brief calculates antilog value
 * @param[in] power given number
 * @param[in] base  log base
 * @return antilog value base^power
 */
float64_t math_antilog(float64_t power, float64_t base);

/**
 * @brief calculates root of given number at root
 * @param[in] number given number
 * @param[in] root root to calculate
 * @return root result
 */
float64_t math_root(float64_t number, float64_t root);

/**
 * @brief calculates sin of x
 * @param[in] number number for calculation
 * @return sin(number)
 */
float64_t math_sin(float64_t number);

#endif
