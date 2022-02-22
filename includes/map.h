/**
 * @file map.h
 * @brief map interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___MAP_H
#define ___MAP_H 0

#include <types.h>
#include <memory.h>

typedef void* map_t;

typedef uint64_t (* map_key_extractor_f)(const void* key);

uint64_t map_default_key_extractor(const void* key);
uint64_t map_string_key_extractor(const void* key);
uint64_t map_data_key_extractor(const void* key_size);

map_t map_new_with_heap_with_factor(memory_heap_t* heap, int64_t factor, map_key_extractor_f mke);
#define map_new_with_factor(f, mke) map_new_with_heap_with_factor(NULL, f, mke)
#define map_new(mke) map_new_with_heap_with_factor(NULL, 128, mke)

#define map_integer() map_new(&map_default_key_extractor)
#define map_string() map_new(&map_string_key_extractor)
#define map_data() map_new(&map_data_key_extractor)

void* map_insert(map_t map, void* key, void* data);
void* map_get_with_default(map_t map, void* key, void* def);
#define map_get(m, k) map_get_with_default(m, k, NULL)
#define map_exists(m, k) (map_get_with_default(m, k, NULL) != NULL)
void* map_delete(map_t map, void* key);
int8_t map_destroy(map_t map);


#endif
