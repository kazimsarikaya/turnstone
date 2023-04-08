/**
 * @file binarsearch.h
 * @brief binarysearch interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___BINARYSEARCH_H
#define ___BINARYSEARCH_H 0

#include <types.h>

/**
 * @brief item comparator
 * @param[in] item1 first item
 * @param[in] item2 second item
 * @return -1 if item1<item2, 1 if item1>item2, 0 otherwise
 */
typedef int8_t (*binarsearch_comparator_f)(const void* item1, const void* item2);

/**
 * @brief binarsearch
 * @param[in] list list to search
 * @param[in] size list size
 * @param[in] item_size each item size in list
 * @param[in] key to search
 * @param[in] cmp binarsearch comparator \ref binarsearch_comparator_f
 * @return found item or null
 */
void* binarsearch(void* list, uint64_t size, uint64_t item_size, void* key, binarsearch_comparator_f cmp);

#endif

