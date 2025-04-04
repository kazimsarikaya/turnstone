/**
 * @file bloomfilter.h
 * @brief bloom filter interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 **/

#ifndef ___BLOOMFILTER_H
/*! prevent duplicate header error macro */
#define ___BLOOMFILTER_H 0

#include <types.h>
#include <data.h>

#ifdef __cplusplus
extern "C" {
#endif

///! bloomfilter_t type
typedef struct bloomfilter_t bloomfilter_t;

/**
 * @brief creates new bloomfilter
 * @param[in] entry_count how many entries
 * @param[in] error error rate for false positive
 * @return bloom filter
 *
 */
bloomfilter_t* bloomfilter_new(uint64_t entry_count, float64_t error);

/**
 * @brief destroy's bloom filter
 * @param[in] bf bloom filter
 * @return true if succeed.
 */
boolean_t bloomfilter_destroy(bloomfilter_t* bf);

/**
 * @brief check given data in bloom filter
 * @param[in] bf bloom filter
 * @param[in] data given data
 * @return true if found
 */
boolean_t bloomfilter_check(bloomfilter_t* bf, data_t* data);

/**
 * @brief add given data to bloom filter
 * @param[in] bf bloom filter
 * @param[in] data given data
 * @return true if added
 */
boolean_t bloomfilter_add(bloomfilter_t* bf, data_t* data);

/**
 * @brief serialize given bloom filter
 * @param[in] bf bloom filter to serialize
 * @return serialized data
 */
data_t* bloomfilter_serialize(bloomfilter_t* bf);

/**
 * @brief deserialize given bloom filter
 * @param[in] data data that holds serialized bloom filter
 * @return bloom filter
 */
bloomfilter_t* bloomfilter_deserialize(data_t* data);

#ifdef __cplusplus
}
#endif

#endif

