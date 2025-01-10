/**
 * @file list.64.c
 * @brief generic linked list implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <types.h>
#include <list.h>
#include <indexer.h>
#include <cpu/sync.h>
#include <strings.h>
#include <logging.h>

MODULE("turnstone.lib.list");

typedef struct list_item_t {
    const void* data; ///< the data inside list item
}list_item_t; ///<short hand for struct


/**
 * @struct list_t
 * @brief list internal interface
 */
typedef struct list_t {
    memory_heap_t*         heap; ///< the heap of the list
    list_type_t            type; ///< list type
    lock_t*                lock; ///< lock for the list
    list_data_comparator_f comparator; ///< if the list is sorted, this is comparator function for data
    list_data_comparator_f equality_comparator; ///< if the list is sorted, this is comparator function for data
    size_t                 item_count; ///< item count at the list, for fast access.
    indexer_t              indexer; ///< if the list is indexed, this is the indexer
} list_t;

/**
 * @brief linked list default data comparator
 * @param  data1 item 1
 * @param  data2 item 2
 * @return   -1 data1<data2, 0 data1=data2, 1 data1>data2
 *
 * assumes data1 and data2 is size_t pointer
 */
int8_t list_default_data_comparator (const void* data1, const void* data2){
    size_t* t_i = (size_t*) data1;
    size_t* t_j = (size_t*) data2;
    if(*t_i < *t_j) {
        return -1;
    }
    if(*t_i > *t_j) {
        return 1;
    }
    return 0;
}

int8_t list_string_comprator (const void* data1, const void* data2) {
    return strcmp((char_t*) data1, (char_t*) data2);
}


memory_heap_t* list_get_heap(list_t* list) {
    if(list == NULL) {
        return NULL;
    }

    return list->heap;
}

list_data_comparator_f list_set_comparator(list_t* list, list_data_comparator_f comparator){
    if(list == NULL) {
        return NULL;
    }

    list_data_comparator_f old = list->comparator;
    list->comparator = comparator;
    return old;
}

int8_t list_set_equality_comparator(list_t* list, list_data_comparator_f comparator){
    if(list == NULL) {
        return -1;
    }

    list->equality_comparator = comparator;

    return 0;
}

size_t list_size(const list_t* list){
    if(list == NULL) {
        return 0;
    }

    return list->item_count;
}

int8_t list_merge(list_t* self, list_t* list){
    if(self == NULL || list == NULL) {
        return -1;
    }

    if(self->type != list->type) {
        return -1;
    }

    if(self->type == LIST_TYPE_INDEXEDLIST) {
        return -1;
    }

    for(size_t i = 0; i < list->item_count; i++) {
        const void* item = list_get_data_at_position(list, i);

        if(item == NULL) {
            return -1;
        }

        if(list_insert_at(self, item, LIST_INSERT_AT_TAIL, 0) == -1ULL) {
            return -1;
        }
    }

    return 0;
}

const void* list_get_data_from_listitem(list_item_t* item) {
    if(item == NULL) {
        return NULL;
    }

    return item->data;
}

list_t* linkedlist_create_with_type(memory_heap_t* heap, list_type_t type,
                                    list_data_comparator_f comparator, indexer_t indexer);
list_t* arraylist_create_with_type(memory_heap_t* heap, list_type_t type,
                                   list_data_comparator_f comparator, indexer_t indexer);

list_t* list_create_with_type(memory_heap_t* heap, list_type_t type,
                              list_data_comparator_f comparator, indexer_t indexer) {
    heap = memory_get_heap(heap);

    if(type & LIST_TYPE_LINKED) {
        return linkedlist_create_with_type(heap, type, comparator, indexer);
    } else if(type & LIST_TYPE_ARRAY) {
        return arraylist_create_with_type(heap, type, comparator, indexer);
    } else { // default is linked list
        return linkedlist_create_with_type(heap, type, comparator, indexer);
    }
}

uint8_t linkedlist_destroy_with_type(list_t* list, list_destroy_type_t type, list_item_destroyer_callback_f destroyer);
uint8_t arraylist_destroy_with_type(list_t* list, list_destroy_type_t type, list_item_destroyer_callback_f destroyer);

uint8_t list_destroy_with_type(list_t* list, list_destroy_type_t type, list_item_destroyer_callback_f destroyer) {
    if(list == NULL) {
        return -1;
    }

    if(list->type & LIST_TYPE_LINKED) {
        return linkedlist_destroy_with_type(list, type, destroyer);
    } else if(list->type & LIST_TYPE_ARRAY) {
        return arraylist_destroy_with_type(list, type, destroyer);
    }

    // default is linked list
    return linkedlist_destroy_with_type(list, type, destroyer);
}

size_t linkedlist_insert_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);
size_t arraylist_insert_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);

size_t list_insert_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position) {
    if(list == NULL) {
        return -1ULL;
    }

    if(list->type & LIST_TYPE_LINKED) {
        return linkedlist_insert_at(list, data, where, position);
    } else if(list->type & LIST_TYPE_ARRAY) {
        return arraylist_insert_at(list, data, where, position);
    }

    // default is linked list
    return linkedlist_insert_at(list, data, where, position);
}

const void* linkedlist_delete_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);
const void* arraylist_delete_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);

const void* list_delete_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position) {
    if(list == NULL) {
        return NULL;
    }

    if(list->type & LIST_TYPE_LINKED) {
        return linkedlist_delete_at(list, data, where, position);
    } else if(list->type & LIST_TYPE_ARRAY) {
        return arraylist_delete_at(list, data, where, position);
    }

    // default is linked list
    return linkedlist_delete_at(list, data, where, position);
}


const void* linkedlist_get_data_at_position(list_t* list, size_t position);
const void* arraylist_get_data_at_position(list_t* list, size_t position);

const void* list_get_data_at_position(list_t* list, size_t position) {
    if(list == NULL) {
        return NULL;
    }

    if(list->type & LIST_TYPE_LINKED) {
        return linkedlist_get_data_at_position(list, position);
    } else if(list->type & LIST_TYPE_ARRAY) {
        return arraylist_get_data_at_position(list, position);
    }

    // default is linked list
    return linkedlist_get_data_at_position(list, position);
}


int8_t linkedlist_get_position(list_t* list, const void* data, size_t* position);
int8_t arraylist_get_position(list_t* list, const void* data, size_t* position);

int8_t list_get_position(list_t* list, const void* data, size_t* position) {
    if(list == NULL) {
        return -1;
    }

    if(list->type & LIST_TYPE_LINKED) {
        return linkedlist_get_position(list, data, position);
    } else if(list->type & LIST_TYPE_ARRAY) {
        return arraylist_get_position(list, data, position);
    }

    // default is linked list
    return linkedlist_get_position(list, data, position);
}


iterator_t* linkedlist_iterator_create(list_t* list);
iterator_t* arraylist_iterator_create(list_t* list);

iterator_t* list_iterator_create(list_t* list) {
    if(list == NULL) {
        return NULL;
    }

    if(list->type & LIST_TYPE_LINKED) {
        return linkedlist_iterator_create(list);
    } else if(list->type & LIST_TYPE_ARRAY) {
        return arraylist_iterator_create(list);
    }

    // default is linked list
    return linkedlist_iterator_create(list);
}

int8_t arraylist_set_capacity(list_t* list, size_t capacity);

int8_t list_set_capacity(list_t* list, size_t capacity) {
    if(list == NULL) {
        return -1;
    }

    if(list->type & LIST_TYPE_ARRAY) {
        return arraylist_set_capacity(list, capacity);
    }

    return -2;
}

list_t* linkedlist_duplicate_list_with_heap(memory_heap_t* heap, list_t* list);
list_t* arraylist_duplicate_list_with_heap(memory_heap_t* heap, list_t* list);

list_t* list_duplicate_list_with_heap(memory_heap_t* heap, list_t* list) {
    if(list == NULL) {
        return NULL;
    }

    if(list->type & LIST_TYPE_LINKED) {
        return linkedlist_duplicate_list_with_heap(heap, list);
    } else if(list->type & LIST_TYPE_ARRAY) {
        return arraylist_duplicate_list_with_heap(heap, list);
    }

    // default is linked list
    return linkedlist_duplicate_list_with_heap(heap, list);
}
