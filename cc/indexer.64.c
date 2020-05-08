#include <types.h>
#include <indexer.h>
#include <memory.h>
#include <linkedlist.h>
#include <iterator.h>

typedef struct {
	index_t* idx;
	indexer_key_creator_f key_creator;
}indexer_idx_kc_internal_t;

typedef struct {
	memory_heap_t* heap;
	linkedlist_t indexes;
} indexer_internal_t;

indexer_t indexer_create_with_heap(memory_heap_t* heap){
	indexer_internal_t* l_idxer = memory_malloc_ext(heap, sizeof(indexer_internal_t), 0x0);
	l_idxer->heap = heap;
	l_idxer->indexes = linkedlist_create_list_with_heap(heap);
	return l_idxer;
}

int8_t indexer_destroy(indexer_t idxer){
	indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;
	linkedlist_destroy(l_idxer->indexes);
	memory_free_ext(l_idxer->heap, idxer);
	return 0;
}

int8_t indexer_register_index(indexer_t idxer, index_t* idx, indexer_key_creator_f key_creator){
	indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;
	indexer_idx_kc_internal_t* pair = memory_malloc_ext(l_idxer->heap, sizeof(indexer_idx_kc_internal_t), 0x0);
	pair->idx = idx;
	pair->key_creator = key_creator;
	linkedlist_list_insert(l_idxer->indexes, pair);
	return 0;
}

int8_t indexer_index(indexer_t idxer, void* key, void* data){
	indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;
	iterator_t* iter = linkedlist_iterator_create(l_idxer->indexes);
	while(iter->end_of_iterator(iter) != 0) {
		indexer_idx_kc_internal_t* pair = iter->get_item(iter);
		void* r_key = pair->key_creator(key);
		if(r_key != NULL) {
			pair->idx->insert(pair->idx, r_key, data);
		}
		iter = iter->next(iter);
	}
	iter->destroy(iter);
	return 0;
}

void* indexer_delete(indexer_t idxer, void* key){
	indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;
	void* data = NULL;
	iterator_t* iter = linkedlist_iterator_create(l_idxer->indexes);
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

int8_t indexer_search(indexer_t idxer, void* key, void** result, index_key_search_criteria_t criteria){
	indexer_internal_t* l_idxer = (indexer_internal_t*)idxer;
	int8_t res = -1;
	iterator_t* iter = linkedlist_iterator_create(l_idxer->indexes);
	while(iter->end_of_iterator(iter) != 0) {
		indexer_idx_kc_internal_t* pair = iter->get_item(iter);
		void* r_key = pair->key_creator(key);
		if(r_key != NULL) {
			if(pair->idx->search(pair->idx, r_key, result, criteria) == 0) {
				res = 0;
				break;
			}
		}
		iter = iter->next(iter);
	}
	iter->destroy(iter);
	return res;
}
