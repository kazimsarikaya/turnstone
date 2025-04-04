/**
 * @file map.64.c
 * @brief map implementation with b+ tree
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <map.h>
#include <bplustree.h>
#include <data.h>
#include <strings.h>
#include <xxhash.h>
#include <cpu/sync.h>

MODULE("turnstone.lib");

typedef struct map_t {
    memory_heap_t*      heap;
    lock_t*             lock;
    map_key_extractor_f mke;
    index_t*            store;
} map_t;

int8_t map_key_comparator(const void* data1, const void* data2);

uint64_t map_default_key_extractor(const void* key) {
    return (uint64_t)key;
}

uint64_t map_string_key_extractor(const void* key) {
    const char_t* str = key;

    return xxhash64_hash(str, strlen(str));
}

uint64_t map_data_key_extractor(const void* key_size) {
    data_t* d = (data_t*)key_size;

    return xxhash64_hash(d->value, d->length);
}

int8_t map_key_comparator(const void* data1, const void* data2){
    uint64_t t_i = (uint64_t)data1;
    uint64_t t_j = (uint64_t)data2;

    if(t_i < t_j) {
        return -1;
    }

    if(t_i > t_j) {
        return 1;
    }

    return 0;
}

map_t* map_new_with_heap_with_factor(memory_heap_t* heap, int64_t factor, map_key_extractor_f mke) {
    heap = memory_get_heap(heap);

    map_t * map = memory_malloc_ext(heap, sizeof(map_t), 0);

    if(map == NULL) {
        return NULL;
    }

    map->heap = heap;
    map->lock = lock_create_with_heap(map->heap);

    if(map->lock == NULL) {
        memory_free(map);

        return NULL;
    }

    map->mke = mke;
    map->store = bplustree_create_index_with_heap_and_unique(map->heap, factor, &map_key_comparator, true);

    if(map->store == NULL) {
        lock_destroy(map->lock);
        memory_free(map);

        return NULL;
    }

    return map;
}

void* map_insert(map_t* map, const void* key, const void* data) {
    lock_acquire(map->lock);


    uint64_t ckey = map->mke(key);

    void* old_data = NULL;

    map->store->insert(map->store, (void*)ckey, data, &old_data);

    lock_release(map->lock);

    return old_data;
}

const void* map_get_with_default(map_t* map, const void* key, void* def) {
    uint64_t ckey = map->mke(key);

    const void* res = map->store->find(map->store, (void*)ckey);

    if(res == NULL) {
        res = def;
    }

    return res;
}

const void* map_delete(map_t* map, const void* key) {
    uint64_t ckey = map->mke(key);

    void* old_data = NULL;

    lock_acquire(map->lock);

    map->store->delete(map->store, (void*)ckey, &old_data);

    lock_release(map->lock);

    return old_data;
}

int8_t map_destroy(map_t* map) {

    lock_destroy(map->lock);
    bplustree_destroy_index(map->store);
    memory_free_ext(map->heap, map);

    return 0;
}

iterator_t* map_create_iterator(map_t* map) {
    if(!map) {
        return NULL;
    }

    return map->store->create_iterator(map->store);
}

uint64_t map_size(map_t* map) {
    if(!map) {
        return 0;
    }

    return map->store->size(map->store);
}
