/**
 * @file linkedlist.64.c
 * @brief linked list types implementations
 */
#include <types.h>
#include <linkedlist.h>
#include <indexer.h>

/**
 * @struct linkedlist_item_internal
 * @brief linked list internal  type.
 *
 * double linked list item
 */
typedef struct linkedlist_item_internal {
	void* data; ///< the data inside list item
	struct linkedlist_item_internal* next; ///< next list item
	struct linkedlist_item_internal* previous; ///< previous list item
}linkedlist_item_internal_t; ///<short hand for struct

/**
 * @struct linkedlist_internal_t
 * @brief linked list internal interface
 */
typedef struct {
	memory_heap_t* heap; ///< the heap of the list
	linkedlist_type_t type; ///< list type
	linkedlist_data_comparator_f comparator; ///< if the list is sorted, this is comparator function for data
	indexer_t indexer; ///< if the list is indexed, this is the indexer
	size_t item_count; ///< item count at the list, for fast access.
	linkedlist_item_internal_t* head; ///< head of the list
	linkedlist_item_internal_t* tail; ///< tail of the list
}linkedlist_internal_t; ///< short hand for struct

/**
 * @struct linkedlist_iterator_internal_t
 * @brief iterator struct
 */
typedef struct {
	linkedlist_internal_t* list; ///< owner list
	linkedlist_item_internal_t* current; ///< current item at the iterator
	uint8_t current_deleted; ///< if current item is deleted it is to be 1.
} linkedlist_iterator_internal_t; ///<short hand for struct

/**
 * @brief linked list default data comparator
 * @param  data1 item 1
 * @param  data2 item 2
 * @return   -1 data1<data2, 0 data1=data2, 1 data1>data2
 *
 * assumes data1 and data2 is size_t pointer
 */
int8_t linkedlist_default_data_comparator(const void* data1, const void* data2){
	size_t* t_i = (size_t*)data1;
	size_t* t_j = (size_t*)data2;
	if(*t_i < *t_j) {
		return -1;
	}
	if(*t_i > *t_j) {
		return 1;
	}
	return 0;
}

linkedlist_t linkedlist_create_with_type(memory_heap_t* heap, linkedlist_type_t type,
                                         linkedlist_data_comparator_f comparator, indexer_t indexer){
	linkedlist_internal_t* list;
	list = memory_malloc_ext(heap, sizeof(linkedlist_internal_t), 0x0);
	list->heap = heap;
	list->type = type;
	list->comparator = comparator;
	if(list->comparator == NULL) {
		list->comparator = &linkedlist_default_data_comparator;
	}
	list->indexer = indexer;
	list->item_count = 0;
	list->head = NULL;
	list->tail = NULL;
	return list;
}

linkedlist_data_comparator_f linkedlist_set_comparator(linkedlist_t list, linkedlist_data_comparator_f comparator){
	linkedlist_internal_t* l = (linkedlist_internal_t*)list;
	linkedlist_data_comparator_f old = l->comparator;
	l->comparator = comparator;
	return old;
}

size_t linkedlist_size(linkedlist_t list){
	linkedlist_internal_t* l = (linkedlist_internal_t*)list;
	return l->item_count;
}

uint8_t linkedlist_destroy_with_type(linkedlist_t list, linkedlist_destroy_type_t type){
	// TODO: check errors
	memory_heap_t* heap = ((linkedlist_internal_t*)list)->heap;
	linkedlist_iterator_t iter = linkedlist_iterator_create(list);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		void* data = linkedlist_iterator_delete_item(iter);
		if(type == LINKEDLIST_DESTROY_WITH_DATA) {
			memory_free_ext(heap, data);
		}
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);
	return memory_free_ext(heap, list);
}

void* linkedlist_get_data_from_listitem(linkedlist_item_t list_item) {
	linkedlist_item_internal_t* li = (linkedlist_item_internal_t*)list_item;
	return li->data;
}

size_t linkedlist_insert_at(linkedlist_t list, void* data, linkedlist_insert_delete_at_t where, size_t position){
	if(data == NULL) {
		return NULL;
	}
	linkedlist_internal_t* l = (linkedlist_internal_t*)list;
	size_t result = 0;
	linkedlist_item_internal_t* item = memory_malloc_ext(l->heap, sizeof(linkedlist_item_internal_t), 0x0);
	item->data = data;

	if(l->head == NULL) { // if head is null insert both head and tail and return
		l->head = item;
		l->tail = item;
		l->item_count++;
		return 0;
	}

	if(where == LINKEDLIST_INSERT_AT_HEAD || where == LINKEDLIST_INSERT_AT_ANYWHERE) {
		item->next = l->head;
		l->head->previous = item;
		l->head = item;
		result = 0;
	} else if(where == LINKEDLIST_INSERT_AT_TAIL) {
		item->previous = l->tail;
		l->tail->next = item;
		l->tail = item;
		result = l->item_count;
	} else if(where == LINKEDLIST_INSERT_AT_SORTED) {
		linkedlist_item_internal_t* cur = l->head;
		uint8_t insert_at_end = 0;
		result = 0;
		while(l->comparator(item->data, cur->data) > 0) {
			if(cur->next != NULL) {
				cur = cur->next;
			} else {
				insert_at_end = 1;
				break;
			}
			result++;
		}
		if(insert_at_end == 0) {
			if(cur->previous != NULL) {
				cur->previous->next = item;
			} else {
				l->head = item;
			}
			item->next = cur;
			item->previous = cur->previous;
			cur->previous = item;
		} else {
			l->tail = item;
			cur->next = item;
			item->previous = cur;
			result = l->item_count;
		}
	} else if(where == LINKEDLIST_INSERT_AT_INDEXED) {
		item->next = l->head;
		l->head->previous = item;
		l->head = item;
		indexer_index(l->indexer, data, item);
		result = 0;
	}else if(where == LINKEDLIST_INSERT_AT_POSITION) {
		linkedlist_item_internal_t* cur = l->head;
		size_t index = 0;
		uint8_t insert_at_end = 0;
		while(index < position) {
			cur = cur->next;
			if(cur == NULL) {
				insert_at_end = 1;
				break;
			}
			index++;
		}
		if(insert_at_end == 1) {
			item->previous = l->tail;
			l->tail->next = item;
			l->tail = item;
			result = l->item_count;
		} else {
			item->next = cur;
			item->previous = cur->previous;
			cur->previous = item;
			if(item->previous != NULL) {
				item->previous->next = item;
			} else {
				l->head = item;
			}
			result = position;
		}
	}
	l->item_count++;
	return result;
}

void* linkedlist_delete_at(linkedlist_t list, void* data, linkedlist_insert_delete_at_t where, size_t position){
	if(data == NULL && where == LINKEDLIST_DELETE_AT_FINDBY) {
		return NULL;
	}
	linkedlist_internal_t* l = (linkedlist_internal_t*)list;
	if(l->item_count == 0) {
		return NULL;
	}
	void* result = NULL;
	if(where == LINKEDLIST_DELETE_AT_HEAD) {
		linkedlist_item_internal_t* item = l->head;
		l->head = l->head->next;
		if(l->head == NULL) {
			l->tail = NULL;
		} else {
			l->head->previous = NULL;
		}
		result = item->data;
		memory_free_ext(l->heap, item);
		l->item_count--;
	} else if(where == LINKEDLIST_DELETE_AT_TAIL) {
		linkedlist_item_internal_t* item = l->tail;
		l->tail = l->tail->previous;
		if(l->tail == NULL) {
			l->head = NULL;
		} else {
			l->tail->next = NULL;
		}
		result = item->data;
		memory_free_ext(l->heap, item);
		l->item_count--;
	} else if(where == LINKEDLIST_DELETE_AT_FINDBY) {
		if(l->type == LINKEDLIST_TYPE_INDEXEDLIST) {
			linkedlist_item_internal_t* item = (linkedlist_item_internal_t*)indexer_search(l->indexer, data);
			linkedlist_item_internal_t* previous = item->previous;
			linkedlist_item_internal_t* next = item->next;
			if(previous == NULL) {
				l->head = next;
				if(l->head != NULL) {
					l->head->previous = NULL;
				} else {
					l->tail = NULL;
				}
			} else {
				previous->next = next;
			}
			if(next == NULL) {
				l->tail = previous;
				if(l->tail != NULL) {
					l->tail->next = NULL;
				} else {
					l->head = NULL;
				}
			} else {
				next->previous = previous;
			}
			// TODO: check error.
			indexer_delete(l->indexer, item);
			result = item->data;
			memory_free_ext(l->heap, item);
			l->item_count--;
		} else {
			linkedlist_iterator_t iter = linkedlist_iterator_create(l);
			while(linkedlist_iterator_end_of_list(iter) != 0) {
				if(l->comparator(linkedlist_iterator_get_item(iter), data) == 0) {
					result = linkedlist_iterator_delete_item(iter);
					break;
				}
				iter = linkedlist_iterator_next(iter);
			}
			linkedlist_iterator_destroy(iter);
		}
	} else if(where == LINKEDLIST_DELETE_AT_POSITION) {
		linkedlist_iterator_t iter = linkedlist_iterator_create(l);
		while(linkedlist_iterator_end_of_list(iter) != 0) {
			if(position == 0) {
				result = linkedlist_iterator_delete_item(iter);
				break;
			}
			position--;
			iter = linkedlist_iterator_next(iter);
		}
		linkedlist_iterator_destroy(iter);
		if(position > 0) {
			result = NULL;
		}
	}
	return result;
}

int8_t linkedlist_get_position(linkedlist_t list, void* data, size_t* position){
	if(data == NULL) {
		return -1;
	}
	linkedlist_internal_t* l = (linkedlist_internal_t*)list;
	*position = 0;
	int8_t res = -1;
	linkedlist_iterator_t iter = linkedlist_iterator_create(l);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		if(l->comparator(linkedlist_iterator_get_item(iter), data) == 0) {
			res = 0;
			break;
		}
		(*position)++;
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);
	return res;
}

void* linkedlist_get_data_at_position(linkedlist_t list, size_t position){
	void* result = NULL;
	linkedlist_internal_t* l = (linkedlist_internal_t*)list;
	if(position >= l->item_count) {
		return result;
	}
	linkedlist_iterator_t iter = linkedlist_iterator_create(l);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		if(position == 0) {
			result = linkedlist_iterator_get_item(iter);
			break;
		}
		position--;
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);
	return result;
}

linkedlist_iterator_t linkedlist_iterator_create(linkedlist_t list) {
	linkedlist_internal_t* l = (linkedlist_internal_t*)list;
	linkedlist_iterator_internal_t* iter = memory_malloc_ext(l->heap, sizeof(linkedlist_iterator_internal_t), 0x0);
	iter->list = l;
	iter->current = l->head;
	return iter;
}

uint8_t linkedlist_iterator_destroy(linkedlist_iterator_t iterator) {
	linkedlist_iterator_internal_t* iter = (linkedlist_iterator_internal_t*)iterator;
	memory_heap_t* heap = iter->list->heap;
	return memory_free_ext(heap, iter);
}

linkedlist_iterator_t linkedlist_iterator_next(linkedlist_iterator_t iterator) {
	linkedlist_iterator_internal_t* iter = (linkedlist_iterator_internal_t*)iterator;
	if(iter->current != NULL) {
		if(iter->current_deleted == 1) {
			iter->current_deleted = 0;
		} else {
			iter->current = iter->current->next;
		}
	}
	return iter;
}

uint8_t linkedlist_iterator_end_of_list(linkedlist_iterator_t iterator) {
	linkedlist_iterator_internal_t* iter = (linkedlist_iterator_internal_t*)iterator;
	return iter->current == NULL ? 0 : 1;
}

void* linkedlist_iterator_get_item(linkedlist_iterator_t iterator) {
	linkedlist_item_internal_t* li = ((linkedlist_iterator_internal_t*)iterator)->current;
	return li->data;
}

void* linkedlist_iterator_delete_item(linkedlist_iterator_t iterator){
	linkedlist_iterator_internal_t* iter = (linkedlist_iterator_internal_t*)iterator;
	if(iter->current == NULL) {
		return NULL;
	}
	void* data = iter->current->data;
	linkedlist_item_internal_t* current = iter->current;
	linkedlist_item_internal_t* previous = iter->current->previous;
	linkedlist_item_internal_t* next = iter->current->next;
	if(previous == NULL) {
		iter->list->head = next;
		if(next != NULL) {
			next->previous = NULL;
		}
	} else {
		previous->next = next;
	}
	if(next == NULL) {
		iter->list->tail = previous;
		if(previous != NULL) {
			previous->next = NULL;
		}
	} else {
		next->previous = previous;
	}
	iter->current = next;
	iter->current_deleted = 1;
	iter->list->item_count--;
	memory_free_ext(iter->list->heap, current);
	if(iter->list->type == LINKEDLIST_TYPE_INDEXEDLIST) {
		// TODO: check error
		indexer_delete(iter->list->indexer, data);
	}
	return data;
}
