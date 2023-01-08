/**
 * @file crc.h
 * @brief crc headers.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___CRC_H
/*! prevent duplicate header error macro */
#define ___CRC_H 0

#include <types.h>

/*! crc32 seed*/
#define CRC32_SEED  0xffffffff

/**
 * @brief initialize crc32 tables for fast calculation
 */
void crc32_init_table();

/**
 * @brief calculates crc32 sum
 * @param[in] p input data
 * @param[in] bytelength input length
 * @param[in] init @ref CRC32_SEED or previous sum
 * @return pre crc32 sum, for finishing it should be xor'ed with @ref CRC32_SEED
 */
uint32_t crc32_sum(uint8_t* p, uint32_t bytelength, uint32_t init);

#endif
