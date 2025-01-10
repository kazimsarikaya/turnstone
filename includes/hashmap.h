/**
 * @file hashmap.h
 * @brief hashmap interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___HASHMAP_H
/*! macro for avoiding multiple inclusion */
#define ___HASHMAP_H 0

#include <types.h>
#include <memory.h>
#include <iterator.h>

/**
 * @typedef hashmap_t
 * @brief opaque hashmap type
 */
typedef struct hashmap_t hashmap_t; ///< opaque hashmap type

/**
 * @typedef hashmap_key_generator_f
 * @param[in] item item to generate key
 * @return key generated from item
 * @brief hashmap key generator function type
 */
typedef uint64_t (*hashmap_key_generator_f)(const void* item);

/**
 * @typedef hashmap_key_comparator_f
 * @param[in] item1 item to compare
 * @param[in] item2 item to compare
 * @return 0 if item1 == item2, -1 if item1 < item2, 1 if item1 > item2
 * @brief hashmap key comparator function type
 */
typedef int8_t (*hashmap_key_comparator_f)(const void* item1, const void* item2);

/**
 * @brief create hashmap with key generator and key comparator
 * @param[in] heap memory heap
 * @param[in] capacity capacity of hashmap
 * @param[in] hkg key generator function
 * @param[in] hkc key comparator function
 * @return hashmap
 */
hashmap_t* hashmap_new_with_hkg_with_hkc(memory_heap_t* heap, uint64_t capacity, hashmap_key_generator_f hkg, hashmap_key_comparator_f hkc);

/**
 * @brief create hashmap with key generator, uses default key comparator
 * @param[in] c capacity of hashmap
 * @param[in] hkg key generator function
 * @return hashmap
 */
#define hashmap_new_with_hkg(c, hkg) hashmap_new_with_hkg_with_hkc(NULL, c, hkg, NULL)

/**
 * @brief create hashmap with key comparator, uses default key generator
 * @param[in] c capacity of hashmap
 * @param[in] hkc key comparator function
 * @return hashmap
 */
#define hashmap_new_with_hkc(c, hkc) hashmap_new_with_hkg_with_hkc(NULL, c, NULL, hkc)

/**
 * @brief create hashmap with key generator and key comparator, uses default memory heap
 * @param[in] heap memory heap
 * @param[in] c capacity of hashmap
 * @return hashmap
 */
#define hashmap_new_with_heap(heap, c) hashmap_new_with_hkg_with_hkc(heap, c, NULL, NULL)

/**
 * @brief create hashmap, uses default key generator and key comparator
 * @param[in] c capacity of hashmap
 * @return hashmap
 */
#define hashmap_new(c) hashmap_new_with_heap(NULL, c)

/**
 * @brief create hashmap with string key, uses string key generator and key comparator
 * @param[in] heap memory heap
 * @param[in] capacity capacity of hashmap
 * @return hashmap
 */
hashmap_t* hashmap_string_with_heap(memory_heap_t* heap, uint64_t capacity);

/**
 * @brief create hashmap with string key, uses string key generator and key comparator
 * @param[in] c capacity of hashmap
 * @return hashmap
 */
#define hashmap_string(c) hashmap_string_with_heap(NULL, c)

/**
 * @brief create hashmap with integer key, uses integer key generator and key comparator
 * @param[in] c capacity of hashmap
 * @return hashmap
 */
#define hashmap_integer(c) hashmap_new(c)

/**
 * @brief create hashmap with integer key, uses integer key generator and key comparator
 * @param[in] heap memory heap
 * @param[in] c capacity of hashmap
 * @return hashmap
 */
#define hashmap_integer_with_heap(heap, c) hashmap_new_with_heap(heap, c)

/**
 * @brief destroy hashmap
 * @param[in] hm hashmap to destroy
 * @return true if hashmap is destroyed successfully, false otherwise
 */
boolean_t hashmap_destroy(hashmap_t* hm);

/**
 * @brief put item to hashmap
 * @param[in] hm hashmap to put item
 * @param[in] key key of item
 * @param[in] item item to put
 * @return old item if same key exists and put successfully, NULL otherwise
 */
const void* hashmap_put(hashmap_t* hm, const void* key, const void* item);

/**
 * @brief get item from hashmap
 * @param[in] hm hashmap to get item
 * @param[in] key key of item
 * @return item if same key exists, NULL otherwise
 */
const void* hashmap_get(hashmap_t* hm, const void* key);

/**
 * @brief get key from hashmap
 * @param[in] hm hashmap to get key
 * @param[in] key key of item
 * @return real key if input key logically exists, NULL otherwise
 */
const void* hashmap_get_key(hashmap_t* hm, const void* key);

/**
 * @brief check existence of key in hashmap
 * @param[in] hm hashmap to check existence
 * @param[in] key key of item
 * @return true if logically same key exists, false otherwise
 */
boolean_t hashmap_exists(hashmap_t* hm, const void* key);

/**
 * @brief delete item from hashmap
 * @param[in] hm hashmap to delete item
 * @param[in] key key of item
 * @return true if logically same key exists and deleted successfully, false otherwise
 */
boolean_t hashmap_delete(hashmap_t* hm, const void* key);

/**
 * @brief get size of hashmap
 * @param[in] hm hashmap to get size
 * @return size of hashmap
 */
uint64_t hashmap_size(hashmap_t* hm);

/**
 * @brief create iterator of hashmap
 * @param[in] hm hashmap to create iterator
 * @return iterator of hashmap
 */
iterator_t* hashmap_iterator_create(hashmap_t* hm);

#endif
