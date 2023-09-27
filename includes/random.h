/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___RANDOM_H
#define ___RANDOM_H 0

#include <types.h>

/**
 * @brief initialize random generator
 * @param[in] seed random generator seed
 */
void srand(uint64_t seed);

/**
 * @brief generates random
 * @return 32 bit random number
 */
uint32_t rand(void);

/**
 * @brief generates random
 * @return 64 bit random number
 */
uint64_t rand64(void);

#endif
