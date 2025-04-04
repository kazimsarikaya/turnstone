/**
 * @file binarysearch.h
 * @brief binarysearch interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___BINARYSEARCH_H
/*! macro for avoiding multiple inclusion */
#define ___BINARYSEARCH_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief item comparator
 * @param[in] item1 first item
 * @param[in] item2 second item
 * @return -1 if item1<item2, 1 if item1>item2, 0 otherwise
 */
typedef int8_t (*binarysearch_comparator_f)(const void* item1, const void* item2);

/**
 * @brief binarsearch
 * @param[in] list list to search
 * @param[in] size list size
 * @param[in] item_size each item size in list
 * @param[in] key to search
 * @param[in] cmp binarsearch comparator @ref binarysearch_comparator_f
 * @return found item or null
 */
void* binarysearch(void* list, uint64_t size, uint64_t item_size, void* key, binarysearch_comparator_f cmp);

#ifdef __cplusplus
}
#endif

#endif

