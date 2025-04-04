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

/**
 * @brief initialize crc32 tables for fast calculation
 */
void crc32_init_table(void);

/**
 * @brief calculates crc32 sum
 * @param[in] p input data
 * @param[in] bytelength input length
 * @param[in] init @ref CRC32_SEED or previous sum
 * @return pre crc32 sum, for finishing it should be xor'ed with @ref CRC32_SEED
 */
uint32_t crc32_sum(const void* p, uint32_t bytelength, uint32_t init);

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
