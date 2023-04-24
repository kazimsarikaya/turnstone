/**
 * @file cache.64.c
 * @brief cache interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <cache.h>
#include <linkedlist.h>
#include <cpu/sync.h>
#include <memory.h>

struct cache_t {
    cache_config_t config;
    linkedlist_t   mru_list;
    hashmap_t*     mru_map;
    uint64_t       mru_size;
    linkedlist_t   lru_list;
    hashmap_t*     lru_map;
    uint64_t       lru_size;
    lock_t         lock;
};

typedef struct cache_item_t {
    const void* key;
    const void* item;
    uint64_t    size;
} cache_item_t;

cache_t* cache_new (cache_config_t * config) {
    if(!config) {
        return NULL;
    }

    cache_t* cache = memory_malloc(sizeof(cache_t));

    if(!cache) {
        return NULL;
    }

    memory_memcopy(config, cache, sizeof(cache_config_t));

    cache->mru_list = linkedlist_create_list();
    cache->mru_map = hashmap_new_with_hkg_with_hkc(128, cache->config.key_generator, cache->config.key_comparator);

    cache->lru_list = linkedlist_create_list();
    cache->lru_map = hashmap_new_with_hkg_with_hkc(128, cache->config.key_generator, cache->config.key_comparator);


    if(!cache->mru_list || !cache->mru_map || !cache->lru_list || !cache->lru_map) {
        linkedlist_destroy(cache->mru_list);
        hashmap_destroy(cache->mru_map);
        linkedlist_destroy(cache->mru_list);
        hashmap_destroy(cache->mru_map);
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


    iterator_t* iter = linkedlist_iterator_create(cache->mru_list);

    while(iter->end_of_iterator(iter) != 0) {
        const cache_item_t* ci = iter->get_item(iter);

        cache->config.item_key_destroyer(ci->key, ci->item);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    linkedlist_destroy_with_data(cache->mru_list);
    hashmap_destroy(cache->mru_map);

    iter = linkedlist_iterator_create(cache->lru_list);

    while(iter->end_of_iterator(iter) != 0) {
        const cache_item_t* ci = iter->get_item(iter);

        cache->config.item_key_destroyer(ci->key, ci->item);


        iter = iter->next(iter);
    }

    iter->destroy(iter);

    linkedlist_destroy_with_data(cache->lru_list);
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

    linkedlist_item_t* li = linkedlist_insert_at_head_and_get_linkedlist_item(cache->mru_list, ci);

    hashmap_put(cache->mru_map, key, li);

    cache->mru_size += size;

    if(cache->mru_size > cache->config.soft_limit) {
        const cache_item_t* old_ci = linkedlist_delete_at_tail(cache->mru_list);
        hashmap_delete(cache->mru_map, old_ci->key);
        cache->mru_size -= old_ci->size;

        li = linkedlist_insert_at_head_and_get_linkedlist_item(cache->lru_list, old_ci);

        hashmap_put(cache->lru_map, old_ci->key, li);

        cache->lru_size += size;

        if(cache->mru_size + cache->lru_size > cache->config.hard_limit) {
            old_ci = linkedlist_delete_at_tail(cache->lru_list);
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

    linkedlist_item_t li = (linkedlist_item_t)hashmap_get(cache->mru_map, key);

    if(li) {
        linkedlist_move_item_to_head(cache->mru_list, li);

        const cache_item_t* ci = linkedlist_get_data_from_listitem(li);

        return ci->item;
    }

    li = (linkedlist_item_t)hashmap_get(cache->lru_map, key);

    if(li) {
        const cache_item_t* ci = linkedlist_get_data_from_listitem(li);

        linkedlist_delete_linkedlist_item(cache->lru_list, li);
        cache->lru_size -= ci->size;

        cache_put(cache, ci->key, ci->item, ci->size);

        const void* item = ci->item;

        memory_free((void*)ci);

        return item;
    }

    return NULL;
}
