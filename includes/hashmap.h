/**
 * @file hashmap.h
 * @brief hashmap interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___HASHMAP_H
#define ___HASHMAP_H 0

#include <types.h>
#include <iterator.h>

typedef struct hashmap_t hashmap_t;

typedef uint64_t (*hashmap_key_generator_f)(const void* item);
typedef int8_t   (*hashmap_key_comparator_f)(const void* item1, const void* item2);

hashmap_t* hashmap_new_with_hkg_with_hkc(uint64_t capacity, hashmap_key_generator_f hkg, hashmap_key_comparator_f hkc);
#define hashmap_new_with_hkg(c, hkg) hashmap_new_with_hkg_with_hkc(c, hkg, NULL)
#define hashmap_new_with_hkc(c, hkc) hashmap_new_with_hkg_with_hkc(c, NULL, hkc);
#define hashmap_new(c) hashmap_new_with_hkg_with_hkc(c, NULL, NULL)
boolean_t   hashmap_destroy(hashmap_t* hm);
const void* hashmap_put(hashmap_t* hm, const void* key, const void* item);
const void* hashmap_get(hashmap_t* hm, const void* key);
const void* hashmap_get_key(hashmap_t* hm, const void* key);
#define hashmap_exists(hm, k) (hashmap_get_key(hm, k) != NULL)
boolean_t   hashmap_delete(hashmap_t* hm, const void* key);
uint64_t    hashmap_size(hashmap_t* hm);
iterator_t* hashmap_iterator_create(hashmap_t* hm);

#endif
