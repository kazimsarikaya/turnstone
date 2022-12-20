/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <indexer.h>
#include <memory.h>
#include <linkedlist.h>
#include <iterator.h>
#include <strings.h>

typedef struct {
    char_t*               idx_name;
    index_t*              idx;
    indexer_key_creator_f key_creator;
}indexer_idx_kc_internal_t;

typedef struct {
    memory_heap_t* heap;
    linkedlist_t   indexes;
} indexer_internal_t;

indexer_t indexer_create_with_heap(memory_heap_t* heap){
    indexer_internal_t* l_idxer = memory_malloc_ext(heap, sizeof(indexer_internal_t), 0x0);

    if(l_idxer == NULL) {
        return NULL;
    }

    l_idxer->heap = heap;
    l_idxer->indexes = linkedlist_create_list_with_heap(heap);

    if(l_idxer->indexes == NULL) {
        linkedlist_destroy(l_idxer->indexes);
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

    linkedlist_destroy(l_idxer->indexes);
    memory_free_ext(l_idxer->heap, idxer);

    return 0;
}

int8_t indexer_register_index(indexer_t idxer, char_t* idx_name, index_t* idx, indexer_key_creator_f key_creator){
    indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;

    if(l_idxer == NULL) {
        return -1;
    }

    indexer_idx_kc_internal_t* pair = memory_malloc_ext(l_idxer->heap, sizeof(indexer_idx_kc_internal_t), 0x0);

    if(pair == NULL) {
        return -1;
    }

    pair->idx_name = idx_name;
    pair->idx = idx;
    pair->key_creator = key_creator;
    linkedlist_list_insert(l_idxer->indexes, pair);
    return 0;
}

int8_t indexer_index(indexer_t idxer, void* key, void* data){
    indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;

    if(l_idxer == NULL) {
        return 0;
    }

    iterator_t* iter = linkedlist_iterator_create(l_idxer->indexes);

    if(iter == NULL) {
        return -1;
    }

    while(iter->end_of_iterator(iter) != 0) {
        indexer_idx_kc_internal_t* pair = iter->get_item(iter);
        void* r_key = pair->key_creator(key);

        if(r_key != NULL) {
            pair->idx->insert(pair->idx, r_key, data, NULL);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}

void* indexer_delete(indexer_t idxer, void* key){
    indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;

    if(l_idxer == NULL) {
        return NULL;
    }

    void* data = NULL;
    iterator_t* iter = linkedlist_iterator_create(l_idxer->indexes);

    if(iter == NULL) {
        return NULL;
    }

    while(iter->end_of_iterator(iter) != 0) {
        indexer_idx_kc_internal_t* pair = iter->get_item(iter);
        void* r_key = pair->key_creator(key);

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

iterator_t* indexer_search(indexer_t idxer, char_t* idx_name, void* key1, void* key2, index_key_search_criteria_t criteria){
    indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;

    if(l_idxer == NULL) {
        return NULL;
    }

    indexer_idx_kc_internal_t* idx_pair = NULL;

    iterator_t* iter = linkedlist_iterator_create(l_idxer->indexes);

    if(iter == NULL) {
        return NULL;
    }

    while(iter->end_of_iterator(iter) != 0) {
        indexer_idx_kc_internal_t* pair = iter->get_item(iter);

        if(strcmp(pair->idx_name, idx_name) == 0) {
            idx_pair = pair;
            break;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    if(idx_pair) {
        void* r_key1 = idx_pair->key_creator(key1);
        void* r_key2 = idx_pair->key_creator(key2);

        return idx_pair->idx->search(idx_pair->idx, r_key1, r_key2, criteria);
    }

    return NULL;
}
