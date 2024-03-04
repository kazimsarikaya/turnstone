/**
 * @file list_array.64.c
 * @brief array list types implementations
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

MODULE("turnstone.lib");


typedef struct list_item_t {
    const void* data; ///< the data inside list item
}list_item_t; ///<short hand for struct

typedef struct list_t {
    memory_heap_t*         heap; ///< the heap of the list
    list_type_t            type; ///< list type
    lock_t*                lock; ///< lock for the list
    list_data_comparator_f comparator; ///< if the list is sorted, this is comparator function for data
    list_data_comparator_f equality_comparator; ///< if the list is sorted, this is comparator function for data
    size_t                 item_count; ///< item count at the list, for fast access.
    indexer_t              indexer; ///< if the list is indexed, this is the indexer
    size_t                 capacity; ///< the capacity of the list
    size_t                 head; ///< the head of the list
    size_t                 tail; ///< the tail of the list
    list_item_t*           items; ///< the items of the list
}list_t; ///< short hand for struct


list_t* arraylist_create_with_type(memory_heap_t* heap, list_type_t type,
                                   list_data_comparator_f comparator, indexer_t indexer);
int8_t      arraylist_set_capacity(list_t* list, size_t capacity);
uint8_t     arraylist_destroy_with_type(list_t* list, list_destroy_type_t type, list_item_destroyer_callback_f destroyer);
size_t      arraylist_insert_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);
const void* arraylist_delete_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);
const void* arraylist_get_data_at_position(list_t* list, size_t position);
int8_t      arraylist_get_position(list_t* list, const void* data, size_t* position);
list_t*     arraylist_duplicate_list_with_heap(memory_heap_t* heap, list_t* list);
iterator_t* arraylist_iterator_create(list_t* list);


list_t* arraylist_create_with_type(memory_heap_t* heap, list_type_t type,
                                   list_data_comparator_f comparator, indexer_t indexer) {

    heap = memory_get_heap(heap); // get rid of the null heap, so heap is always stable.

    list_t* list = memory_malloc_ext(heap, sizeof(list_t), 0x0);

    if(list == NULL) {
        return NULL;
    }

    list->heap = heap;
    list->type = type;
    list->comparator = comparator;
    list->indexer = indexer;
    list->capacity = 128;
    list->head = list->capacity - 1;
    list->tail = list->capacity - 1;
    list->items = memory_malloc_ext(heap, sizeof(list_item_t) * list->capacity, 0x0);

    if(list->items == NULL) {
        memory_free_ext(heap, list);
        return NULL;
    }

    if(type & LIST_TYPE_SORTEDLIST) {
        if(comparator == NULL) {
            list->comparator = list_default_data_comparator;
        }
    }

    return list;
}

int8_t arraylist_set_capacity(list_t* list, size_t capacity) {
    if(list == NULL) {
        return -1;
    }

    if(capacity < list->capacity || capacity == 0) {
        return -1;
    }

    list_item_t* new_items = memory_malloc_ext(list->heap, sizeof(list_item_t) * capacity, 0x0);

    if(new_items == NULL) {
        return -1;
    }


    lock_acquire(list->lock);

    list_item_t* old_items = list->items; // save the old items

    size_t idx = list->head;

    for(size_t i = 0; i < list->item_count; i++) {
        new_items[i].data = list->items[idx].data;
        idx = (idx + 1) % list->capacity;
    }

    list->items = new_items;
    list->capacity = capacity;
    list->head = 0;
    list->tail = list->item_count;

    lock_release(list->lock);

    memory_free_ext(list->heap, old_items);

    return 0;
}

uint8_t arraylist_destroy_with_type(list_t* list, list_destroy_type_t type, list_item_destroyer_callback_f destroyer) {
    if(list == NULL) {
        return -1;
    }

    if(type & LIST_DESTROY_WITH_DATA) {
        for(size_t i = 0; i < list->capacity; i++) {
            if(list->items[i].data != NULL) {
                if(destroyer != NULL) {
                    destroyer(list->heap, (void*)list->items[i].data);
                } else {
                    memory_free_ext(list->heap, (void*)list->items[i].data);
                }
            }
        }
    }

    memory_free_ext(list->heap, list->items);
    memory_free_ext(list->heap, list);

    return 0;
}

const void* arraylist_get_data_at_position(list_t* list, size_t position) {
    if(list == NULL) {
        return NULL;
    }

    if(position >= list->item_count) {
        return NULL;
    }

    position = (list->head + position) % list->capacity;

    return list->items[position].data;
}

int8_t arraylist_get_position(list_t* list, const void* data, size_t* position) {
    if(list == NULL || data == NULL || position == NULL) {
        return -1;
    }

    size_t idx = list->head;

    for(size_t i = 0; i < list->item_count; i++) {
        if(list->comparator(data, list->items[idx].data) == 0) {
            *position = idx;
            return 0;
        }

        idx = (idx + 1) % list->capacity;
    }

    return -1;
}

size_t arraylist_insert_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position) {
    if(list == NULL) {
        return -1;
    }

    UNUSED(data);
    UNUSED(where);
    UNUSED(position);
    NOTIMPLEMENTEDLOG(KERNEL);

    return -1;
}

const void* arraylist_delete_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position) {
    if(list == NULL) {
        return NULL;
    }

    UNUSED(data);
    UNUSED(where);
    UNUSED(position);
    NOTIMPLEMENTEDLOG(KERNEL);

    return NULL;
}

list_t* arraylist_duplicate_list_with_heap(memory_heap_t* heap, list_t* list) {
    if(list == NULL) {
        return NULL;
    }

    UNUSED(heap);
    UNUSED(list);
    NOTIMPLEMENTEDLOG(KERNEL);

    return NULL;
}

iterator_t* arraylist_iterator_create(list_t* list) {
    if(list == NULL) {
        return NULL;
    }

    UNUSED(list);

    NOTIMPLEMENTEDLOG(KERNEL);

    return NULL;
}
