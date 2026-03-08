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

#ifdef __cplusplus
extern "C" {
#endif

/*! crc32 seed*/
#define CRC32_SEED  0xffffffff

/*! crc16 seed*/
#define CRC16_SEED 0xffff

/**
 * @brief initialize crc tables for fast calculation
 */
void crc_init(void);

/**
 * @brief calculates crc32 sum
 * @param[in] data input data
 * @param[in] size input length
 * @param[in] init @ref CRC32_SEED or previous sum
 * @return pre crc32 sum, for finishing it should be xor'ed with @ref CRC32_SEED
 */
uint32_t crc32_sum(const void* data, uint32_t size, uint32_t init);

/**
 * @brief calculates crc32c sum
 * @param[in] data input data
 * @param[in] size input length
 * @param[in] init @ref CRC32_SEED or previous sum
 * @return pre crc32 sum, for finishing it should be xor'ed with @ref CRC32_SEED
 */
uint32_t crc32c_sum(const void* data, uint64_t size, uint32_t init);

/**
 * @brief finalize crc32 sum
 * @param[in] crc pre crc32 sum
 * @return crc32 sum
 */
static inline uint32_t crc32_finalize(uint32_t crc) {
    return crc ^ CRC32_SEED;
}

/**
 * @brief calculates crc16 sum
 * @param[in] data input data
 * @param[in] size input length
 * @param[in] init @ref CRC16_SEED or previous sum
 * @return pre crc16 sum, for finishing it should be xor'ed with @ref CRC16_SEED
 */
uint16_t crc16_sum(const void* data, uint64_t size, uint16_t init);

/**
 * @brief finalize crc16 sum
 * @param[in] crc pre crc16 sum
 * @return crc16 sum
 */
static inline uint16_t crc16_finalize(uint16_t crc) {
    return crc ^ CRC16_SEED;
}

/*! adler32 seed*/
#define ADLER32_SEED 1

/**
 * @brief calculates adler32 sum
 * @param[in] data input data
 * @param[in] size input length
 * @param[in] init seed/previous sum
 * @return crc32 sum
 */
uint32_t adler32_sum(const void* data, uint64_t size, uint32_t init);

#ifdef __cplusplus
}
#endif

#endif
