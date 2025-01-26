/**
 * @file cache.h
 * @brief cache interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___CACHE_H
#define ___CACHE_H 0

#include <types.h>
#include <hashmap.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cache_policy_t {
    CACHE_POLICY_COUNT,
    CACHE_POLICY_SIZE,
} cache_policy_t;

typedef boolean_t (*cache_item_key_destroyer_f)(const void* key, const void* item);

typedef struct cache_config_t {
    cache_policy_t             policy;
    uint64_t                   hard_limit;
    uint64_t                   soft_limit;
    hashmap_key_generator_f    key_generator;
    hashmap_key_comparator_f   key_comparator;
    cache_item_key_destroyer_f item_key_destroyer;
} cache_config_t;

typedef struct cache_t cache_t;

cache_t*  cache_new(cache_config_t* config);
boolean_t cache_destroy(cache_t* cache);
boolean_t cache_put(cache_t* cache, const void* key, const void* item, uint64_t size);
#define cache_put_by_count(c, k, i) cache_put(c, k, i, 1)
#define cache_put_item_as_key(c, i, s) cache_put(c, i, i, s)
const void* cache_get(cache_t* cache, const void* key);

#ifdef __cplusplus
}
#endif

#endif
