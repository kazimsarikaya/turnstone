/**
 * @file indexer.64.c
 * @brief Indexer implementation for several data types.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <indexer.h>
#include <memory.h>
#include <list.h>
#include <iterator.h>
#include <strings.h>

MODULE("turnstone.lib");

typedef struct indexer_idx_kc_internal_t {
    uint64_t              idx_id;
    index_t*              idx;
    indexer_key_creator_f key_creator;
    void*                 keyarg;
}indexer_idx_kc_internal_t;

typedef struct indexer_internal_t {
    memory_heap_t* heap;
    list_t*        indexes;
} indexer_internal_t;

indexer_t indexer_create_with_heap(memory_heap_t* heap){
    indexer_internal_t* l_idxer = memory_malloc_ext(heap, sizeof(indexer_internal_t), 0x0);

    if(l_idxer == NULL) {
        return NULL;
    }

    l_idxer->heap = heap;
    l_idxer->indexes = list_create_list_with_heap(heap);

    if(l_idxer->indexes == NULL) {
        list_destroy(l_idxer->indexes);
        memory_free_ext(heap, l_idxer);

        return NULL;
    }

    return l_idxer;
}

int8_t indexer_destroy(indexer_t idxer){
    indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;

    if(l_idxer == NULL) {
        return 0;
    }

    list_destroy(l_idxer->indexes);
    memory_free_ext(l_idxer->heap, idxer);

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t indexer_register_index(indexer_t idxer, uint64_t idx_id, index_t* idx, indexer_key_creator_f key_creator, void* keyarg){
    indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;

    if(l_idxer == NULL) {
        return -1;
    }

    indexer_idx_kc_internal_t* pair = memory_malloc_ext(l_idxer->heap, sizeof(indexer_idx_kc_internal_t), 0x0);

    if(pair == NULL) {
        return -1;
    }

    pair->idx_id = idx_id;
    pair->idx = idx;
    pair->key_creator = key_creator;
    pair->keyarg = keyarg;
    list_list_insert(l_idxer->indexes, pair);
    return 0;
}
#pragma GCC diagnostic pop

int8_t indexer_index(indexer_t idxer, const void* key, const void* data){
    indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;

    if(l_idxer == NULL) {
        return 0;
    }

    iterator_t* iter = list_iterator_create(l_idxer->indexes);

    if(iter == NULL) {
        return -1;
    }

    while(iter->end_of_iterator(iter) != 0) {
        const indexer_idx_kc_internal_t* pair = iter->get_item(iter);
        void* r_key = pair->key_creator(key, pair->keyarg);

        if(r_key != NULL) {
            pair->idx->insert(pair->idx, r_key, data, NULL);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}

const void* indexer_delete(indexer_t idxer, const void* key){
    indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;

    if(l_idxer == NULL) {
        return NULL;
    }

    void* data = NULL;
    iterator_t* iter = list_iterator_create(l_idxer->indexes);

    if(iter == NULL) {
        return NULL;
    }

    while(iter->end_of_iterator(iter) != 0) {
        const indexer_idx_kc_internal_t* pair = iter->get_item(iter);
        void* r_key = pair->key_creator(key, pair->keyarg);

        if(r_key != NULL) {
            void* tmp_data;
            pair->idx->delete(pair->idx, r_key, &tmp_data);

            if(tmp_data != NULL && data != NULL) {
                data = tmp_data;
            }
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return data;
}

iterator_t* indexer_search(indexer_t idxer, uint64_t idx_id, const void* key1, const void* key2, const index_key_search_criteria_t criteria){
    indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;

    if(l_idxer == NULL) {
        return NULL;
    }

    const indexer_idx_kc_internal_t* idx_pair = NULL;

    iterator_t* iter = list_iterator_create(l_idxer->indexes);

    if(iter == NULL) {
        return NULL;
    }

    while(iter->end_of_iterator(iter) != 0) {
        const indexer_idx_kc_internal_t* pair = iter->get_item(iter);

        if(pair->idx_id == idx_id) {
            idx_pair = pair;
            break;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    if(idx_pair) {
        void* r_key1 = idx_pair->key_creator(key1, idx_pair->keyarg);
        void* r_key2 = idx_pair->key_creator(key2, idx_pair->keyarg);

        return idx_pair->idx->search(idx_pair->idx, r_key1, r_key2, criteria);
    }

    return NULL;
}
