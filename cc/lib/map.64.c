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

typedef struct map_internal_t {
    memory_heap_t*      heap;
    lock_t              lock;
    map_key_extractor_f mke;
    index_t*            store;
} map_internal_t;

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

map_t map_new_with_heap_with_factor(memory_heap_t* heap, int64_t factor, map_key_extractor_f mke) {
    map_internal_t * mi = memory_malloc_ext(heap, sizeof(map_internal_t), 0);

    if(mi == NULL) {
        return NULL;
    }

    mi->heap = heap;
    mi->lock = lock_create_with_heap(mi->heap);

    if(mi->lock == NULL) {
        memory_free(mi);

        return NULL;
    }

    mi->mke = mke;
    mi->store = bplustree_create_index_with_heap_and_unique(mi->heap, factor, &map_key_comparator, true);

    if(mi->store == NULL) {
        lock_destroy(mi->lock);
        memory_free(mi);

        return NULL;
    }

    return (map_t)mi;
}

void* map_insert(map_t map, const void* key, const void* data) {
    map_internal_t* mi = (map_internal_t*)map;

    lock_acquire(mi->lock);


    uint64_t ckey = mi->mke(key);

    void* old_data = NULL;

    mi->store->insert(mi->store, (void*)ckey, data, &old_data);

    lock_release(mi->lock);

    return old_data;
}

const void* map_get_with_default(map_t map, const void* key, void* def) {
    map_internal_t* mi = (map_internal_t*)map;

    uint64_t ckey = mi->mke(key);

    const void* res = mi->store->find(mi->store, (void*)ckey);

    if(res == NULL) {
        res = def;
    }

    return res;
}

const void* map_delete(map_t map, const void* key) {
    map_internal_t* mi = (map_internal_t*)map;

    uint64_t ckey = mi->mke(key);

    void* old_data = NULL;

    lock_acquire(mi->lock);

    mi->store->delete(mi->store, (void*)ckey, &old_data);

    lock_release(mi->lock);

    return old_data;
}

int8_t map_destroy(map_t map) {
    map_internal_t* mi = (map_internal_t*)map;


    lock_destroy(mi->lock);
    bplustree_destroy_index(mi->store);
    memory_free_ext(mi->heap, mi);

    return 0;
}

iterator_t* map_create_iterator(map_t map) {
    if(!map) {
        return NULL;
    }

    map_internal_t* mi = (map_internal_t*)map;

    return mi->store->create_iterator(mi->store);
}

uint64_t map_size(map_t map) {
    if(!map) {
        return 0;
    }

    map_internal_t* mi = (map_internal_t*)map;

    return mi->store->size(mi->store);
}
