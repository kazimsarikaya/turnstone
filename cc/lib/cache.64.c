/**
 * @file cache.64.c
 * @brief cache interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <cache.h>
#include <cpu/sync.h>
#include <memory.h>
#include <logging.h>

MODULE("turnstone.lib");

typedef struct cache_item_t cache_item_t;

struct cache_item_t {
    const void*   key;
    const void*   item;
    uint64_t      size;
    cache_item_t* previous;
    cache_item_t* next;
};

struct cache_t {
    cache_config_t config;
    cache_item_t*  mru_list_head;
    cache_item_t*  mru_list_tail;
    hashmap_t*     mru_map;
    uint64_t       mru_size;
    cache_item_t*  lru_list_head;
    cache_item_t*  lru_list_tail;
    hashmap_t*     lru_map;
    uint64_t       lru_size;
    lock_t         lock;
};

static inline int8_t cache_insert_head(cache_t* cache, boolean_t mru, cache_item_t* ci) {
    if(!cache || !ci) {
        return -1;
    }

    if(mru) {
        if(cache->mru_list_head) {
            cache->mru_list_head->previous = ci;
        }

        ci->next = cache->mru_list_head;
        ci->previous = NULL;
        cache->mru_list_head = ci;

        if(!cache->mru_list_tail) {
            cache->mru_list_tail = ci;
        }
    } else {
        if(cache->lru_list_head) {
            cache->lru_list_head->previous = ci;
        }

        ci->next = cache->lru_list_head;
        ci->previous = NULL;
        cache->lru_list_head = ci;

        if(!cache->lru_list_tail) {
            cache->lru_list_tail = ci;
        }
    }

    return 0;
}

static inline int8_t cache_delete_item(cache_t* cache, boolean_t mru, cache_item_t* ci) {
    if(!cache || !ci) {
        return -1;
    }

    if(mru) {
        if(ci->previous) {
            ci->previous->next = ci->next;
        } else {
            cache->mru_list_head = ci->next;
        }

        if(ci->next) {
            ci->next->previous = ci->previous;
        } else {
            cache->mru_list_tail = ci->previous;
        }
    } else {
        if(ci->previous) {
            ci->previous->next = ci->next;
        } else {
            cache->lru_list_head = ci->next;
        }

        if(ci->next) {
            ci->next->previous = ci->previous;
        } else {
            cache->lru_list_tail = ci->previous;
        }
    }

    ci->previous = NULL;
    ci->next = NULL;

    return 0;
}

static inline int8_t cache_move_to_head(cache_t* cache, boolean_t mru, cache_item_t* ci) {
    if(!cache || !ci) {
        return -1;
    }

    cache_delete_item(cache, mru, ci);
    cache_insert_head(cache, mru, ci);

    return 0;
}

cache_t* cache_new (cache_config_t * config) {
    if(!config) {
        return NULL;
    }

    cache_t* cache = memory_malloc(sizeof(cache_t));

    if(!cache) {
        return NULL;
    }

    memory_memcopy(config, cache, sizeof(cache_config_t));

    cache->mru_map = hashmap_new_with_hkg_with_hkc(128, cache->config.key_generator, cache->config.key_comparator);
    cache->lru_map = hashmap_new_with_hkg_with_hkc(128, cache->config.key_generator, cache->config.key_comparator);

    if(!cache->mru_map || !cache->lru_map) {
        hashmap_destroy(cache->mru_map);
        hashmap_destroy(cache->lru_map);
        memory_free(cache);

        return NULL;
    }

    cache->lock = lock_create();

    return cache;
}

boolean_t cache_destroy (cache_t * cache) {
    if(!cache) {
        return true;
    }

    cache_item_t* ci;

    ci = cache->mru_list_head;

    while(ci) {
        cache_item_t* next = ci->next;

        cache->config.item_key_destroyer(ci->key, ci->item);

        memory_free(ci);

        ci = next;
    }

    hashmap_destroy(cache->mru_map);

    ci = cache->lru_list_head;

    while(ci) {
        cache_item_t* next = ci->next;

        cache->config.item_key_destroyer(ci->key, ci->item);

        memory_free(ci);

        ci = next;
    }

    hashmap_destroy(cache->lru_map);

    lock_destroy(cache->lock);

    memory_free(cache);

    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
boolean_t cache_put(cache_t * cache, const void* key, const void* item, uint64_t size) {
    if(!cache) {
        return false;
    }

    cache_item_t* ci = memory_malloc(sizeof(cache_item_t));

    if(!ci) {
        return false;
    }

    ci->item = item;
    ci->key = key;
    ci->size = size;

    cache_insert_head(cache, true, ci);

    hashmap_put(cache->mru_map, key, ci);

    cache->mru_size += size;

    if(cache->mru_size > cache->config.soft_limit) {
        cache_item_t* old_ci = cache->mru_list_tail;
        cache_delete_item(cache, true, old_ci);
        hashmap_delete(cache->mru_map, old_ci->key);
        cache->mru_size -= old_ci->size;

        cache_insert_head(cache, false, old_ci);

        hashmap_put(cache->lru_map, old_ci->key, old_ci);

        cache->lru_size += size;

        if(cache->mru_size + cache->lru_size > cache->config.hard_limit) {
            old_ci = cache->lru_list_tail;
            cache_delete_item(cache, false, old_ci);
            hashmap_delete(cache->lru_map, old_ci->key);
            cache->lru_size -= old_ci->size;

            cache->config.item_key_destroyer(old_ci->key, old_ci->item);

            memory_free((void*)old_ci);
        }
    }

    return true;
}
#pragma GCC diagnostic pop

const void* cache_get(cache_t* cache, const void* key) {
    if(!cache) {
        return NULL;
    }

    cache_item_t* ci = (cache_item_t*)hashmap_get(cache->mru_map, key);

    if(ci) {
        cache_move_to_head(cache, true, ci);

        return ci->item;
    }

    ci = (cache_item_t*)hashmap_get(cache->lru_map, key);

    if(ci) {
        cache_delete_item(cache, false, ci);
        cache->lru_size -= ci->size;

        cache_put(cache, ci->key, ci->item, ci->size);

        const void* item = ci->item;

        memory_free((void*)ci);

        return item;
    }

    return NULL;
}
