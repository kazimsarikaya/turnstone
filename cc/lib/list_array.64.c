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

MODULE("turnstone.lib.list.array");


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
    indexer_t*             indexer; ///< if the list is indexed, this is the indexer
    size_t                 capacity; ///< the capacity of the list
    size_t                 head; ///< the head of the list
    size_t                 tail; ///< the tail of the list
    list_item_t*           items; ///< the items of the list
}list_t; ///< short hand for struct


list_t* arraylist_create_with_type(memory_heap_t* heap, list_type_t type,
                                   list_data_comparator_f comparator, indexer_t* indexer);
int8_t      arraylist_set_capacity(list_t* list, size_t capacity);
uint8_t     arraylist_destroy_with_type(list_t* list, list_destroy_type_t type, list_item_destroyer_callback_f destroyer);
size_t      arraylist_insert_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);
const void* arraylist_delete_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);
const void* arraylist_get_data_at_position(list_t* list, size_t position);
int8_t      arraylist_get_position(list_t* list, const void* data, size_t* position);
list_t*     arraylist_duplicate_list_with_heap(memory_heap_t* heap, list_t* list);
iterator_t* arraylist_iterator_create(list_t* list);


list_t* arraylist_create_with_type(memory_heap_t* heap, list_type_t type,
                                   list_data_comparator_f comparator, indexer_t* indexer) {

    heap = memory_get_heap(heap); // get rid of the null heap, so heap is always stable.

    list_t* list = memory_malloc_ext(heap, sizeof(list_t), 0x0);

    if(list == NULL) {
        return NULL;
    }

    list->heap = heap;
    list->type = type;
    list->comparator = comparator;

    if(list->comparator == NULL) {
        list->comparator = &list_default_data_comparator;
    }

    list->indexer = indexer;
    list->capacity = 128;
    list->head = list->capacity - 1;
    list->tail = list->capacity - 1;
    list->item_count = 0;

    list->lock = lock_create_with_heap(heap);

    if(list->lock == NULL) {
        memory_free_ext(heap, list);
        return NULL;
    }

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
    list->tail = list->item_count - 1;

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
    lock_destroy(list->lock);
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
            *position = i;
            return 0;
        }

        idx = (idx + 1) % list->capacity;
    }

    return -1;
}

size_t arraylist_insert_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position) {
    if (list == NULL) {
        return -1ULL;
    }

    lock_acquire(list->lock);

    if (list->item_count >= list->capacity) {
        lock_release(list->lock);
        return -1ULL;
    }

    size_t insert_pos = 0;
    size_t result = -1ULL;

    // 1. Determine the logical insertion position
    if (where == LIST_INSERT_AT_HEAD) {
        insert_pos = 0;
    } else if (where == LIST_INSERT_AT_TAIL) {
        insert_pos = list->item_count;
    } else if (where == LIST_INSERT_AT_SORTED) {
        insert_pos = 0;
        while (insert_pos < list->item_count) {
            if (list->comparator(data, arraylist_get_data_at_position(list, insert_pos)) < 0) {
                break;
            }
            insert_pos++;
        }
    } else if (where == LIST_INSERT_AT_POSITION) {
        if (position > list->item_count) {
            lock_release(list->lock);
            return -1ULL;
        }
        insert_pos = position;
    } else {
        lock_release(list->lock); // Unsupported mode (e.g. INDEXED)
        return -1ULL;
    }

    // 2. Handle Insert Logic
    if (list->item_count == 0) {
        list->head = 0;
        list->tail = 0;
        list->items[0].data = data;
        result = 0;
    } else if (insert_pos == 0) {
        // Simple Head Insert
        list->head = (list->head == 0) ? list->capacity - 1 : list->head - 1;
        list->items[list->head].data = data;
        result = 0;
    } else if (insert_pos == list->item_count) {
        // Simple Tail Insert
        list->tail = (list->tail + 1) % list->capacity;
        list->items[list->tail].data = data;
        result = list->item_count;
    } else {
        // Middle Insert: Choose most efficient shift
        size_t target_idx = (list->head + insert_pos) % list->capacity;

        if (insert_pos < list->item_count / 2) {
            // Shift Head-side backwards
            list->head = (list->head == 0) ? list->capacity - 1 : list->head - 1;
            for (size_t i = 0; i < insert_pos; i++) {
                size_t dest = (list->head + i) % list->capacity;
                size_t src = (list->head + i + 1) % list->capacity;
                list->items[dest].data = list->items[src].data;
            }
            // Correct target_idx after head move
            target_idx = (list->head + insert_pos) % list->capacity;
        } else {
            // Shift Tail-side forwards
            for (size_t i = list->item_count; i > insert_pos; i--) {
                size_t dest = (list->head + i) % list->capacity;
                size_t src = (list->head + i - 1) % list->capacity;
                list->items[dest].data = list->items[src].data;
            }
            list->tail = (list->tail + 1) % list->capacity;
        }
        list->items[target_idx].data = data;
        result = insert_pos;
    }

    if (result != -1ULL) {
        list->item_count++;
    }
    lock_release(list->lock);
    return result;
}

const void* arraylist_delete_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position) {
    if (list == NULL || list->item_count == 0) {
        return NULL;
    }

    lock_acquire(list->lock);

    size_t delete_pos = -1ULL;

    // 1. Find logical deletion position
    if (where == LIST_DELETE_AT_HEAD) {
        delete_pos = 0;
    } else if (where == LIST_DELETE_AT_TAIL) {
        delete_pos = list->item_count - 1;
    } else if (where == LIST_DELETE_AT_POSITION) {
        if (position >= list->item_count) {
            lock_release(list->lock); return NULL;
        }
        delete_pos = position;
    } else if (where == LIST_DELETE_AT_FINDBY) {
        for (size_t i = 0; i < list->item_count; i++) {
            size_t idx = (list->head + i) % list->capacity;
            if (list->comparator(data, list->items[idx].data) == 0) {
                delete_pos = i;
                break;
            }
        }
    }

    if (delete_pos == -1ULL) {
        lock_release(list->lock); return NULL;
    }

    // 2. Extract data
    size_t item_idx = (list->head + delete_pos) % list->capacity;
    const void* deleted_data = list->items[item_idx].data;

    // 3. Shift elements
    if (delete_pos < list->item_count / 2) {
        // Shift elements between Head and delete_pos forward
        for (size_t i = delete_pos; i > 0; i--) {
            size_t dest = (list->head + i) % list->capacity;
            size_t src = (list->head + i - 1) % list->capacity;
            list->items[dest].data = list->items[src].data;
        }
        list->items[list->head].data = NULL;
        list->head = (list->head + 1) % list->capacity;
    } else {
        // Shift elements between delete_pos and Tail backward
        for (size_t i = delete_pos; i < list->item_count - 1; i++) {
            size_t dest = (list->head + i) % list->capacity;
            size_t src = (list->head + i + 1) % list->capacity;
            list->items[dest].data = list->items[src].data;
        }
        list->items[list->tail].data = NULL;
        list->tail = (list->tail == 0) ? list->capacity - 1 : list->tail - 1;
    }

    list->item_count--;
    lock_release(list->lock);
    return deleted_data;
}

list_t* arraylist_duplicate_list_with_heap(memory_heap_t* heap, list_t* list) {
    if(list == NULL) {
        return NULL;
    }

    // we cannot duplicate indexed lists, so we just return NULL.
    if(list->indexer != NULL) {
        NOTIMPLEMENTEDLOG(KERNEL);
        return NULL;
    }

    list_t* new_list = memory_malloc_ext(heap, sizeof(list_t), 0x0);

    if(new_list == NULL) {
        return NULL;
    }

    new_list->heap = memory_get_heap(heap); // get rid of the null heap, so heap is always stable.
    new_list->type = list->type;
    new_list->comparator = list->comparator;
    new_list->equality_comparator = list->equality_comparator;
    new_list->item_count = list->item_count;
    new_list->capacity = list->capacity;
    new_list->head = list->head;
    new_list->tail = list->tail;

    new_list->lock = lock_create_with_heap(heap);

    if(new_list->lock == NULL) {
        memory_free_ext(heap, new_list);
        return NULL;
    }

    new_list->items = memory_malloc_ext(heap, sizeof(list_item_t) * new_list->capacity, 0x0);

    if(new_list->items == NULL) {
        lock_destroy(new_list->lock);
        memory_free_ext(heap, new_list);
        return NULL;
    }

    size_t idx = list->head;

    for(size_t i = 0; i < list->item_count; i++) {
        new_list->items[i].data = list->items[idx].data;
        idx = (idx + 1) % list->capacity;
    }

    // If the original list was not empty, the new list's items are copied starting from index 0.
    // We need to adjust head and tail to reflect this linear arrangement in the new array.
    if (list->item_count > 0) {
        new_list->head = 0;
        new_list->tail = list->item_count - 1; // Tail points to the last element's index
    } else {
        // If the original list was empty, initialize head and tail to indicate an empty state.
        new_list->head = new_list->capacity - 1;
        new_list->tail = new_list->capacity - 1;
    }


    return new_list;
}

typedef struct arraylist_iterator_internal_t {
    list_t* list; ///< the list to iterate
    size_t  current; ///< the current position in the list (logical index)
    size_t  current_deleted; ///< the current deleted position in the list
} arraylist_iterator_internal_t;

static int8_t arraylist_iterator_end_of_list(iterator_t* iterator) {
    if(iterator == NULL) {
        return 0;
    }

    arraylist_iterator_internal_t* iter = (arraylist_iterator_internal_t*)iterator->metadata;

    if(iter == NULL) {
        return 0;
    }

    // if current is equal to item_count, we are at the end of the list.
    if(iter->current >= iter->list->item_count) {
        return 0; // end of list
    }

    return 1; // not end of list
}

static const void* arraylist_iterator_get_item(iterator_t* iterator) {
    if(iterator == NULL) {
        return NULL;
    }

    arraylist_iterator_internal_t* iter = (arraylist_iterator_internal_t*)iterator->metadata;

    if(iter == NULL || iter->list == NULL) {
        return NULL;
    }

    if(iter->current >= iter->list->item_count) {
        return NULL; // out of bounds
    }

    size_t idx = (iter->list->head + iter->current) % iter->list->capacity;

    return iter->list->items[idx].data;
}

static iterator_t* arraylist_iterator_next(iterator_t* iterator) {
    if(iterator == NULL) {
        return NULL;
    }

    arraylist_iterator_internal_t* iter = (arraylist_iterator_internal_t*)iterator->metadata;

    if(iter == NULL || iter->list == NULL) {
        return NULL;
    }

    if(iter->current >= iter->list->item_count) {
        return iterator; // end of list, do not increment
    }

    if(iter->current_deleted == 1) {
        iter->current_deleted = 0; // reset the deleted flag
    } else {
        iter->current++; // increment the current position
    }

    return iterator;
}

static const void* arraylist_iterator_delete_item(iterator_t* iterator) {
    if(iterator == NULL) {
        return NULL;
    }

    arraylist_iterator_internal_t* iter = (arraylist_iterator_internal_t*)iterator->metadata;

    if(iter == NULL || iter->list == NULL) {
        return NULL;
    }

    if(iter->current >= iter->list->item_count) {
        return NULL; // out of bounds
    }

    // Acquire lock before modifying the list
    lock_acquire(iter->list->lock);

    size_t idx_to_delete = (iter->list->head + iter->current) % iter->list->capacity;

    // We cannot use arraylist_delete_at because it shifts elements.
    // Then we lost iterator's current position.
    // Instead, we directly delete the item at the calculated index.
    // and shift elements accordingly.
    const void* deleted_data = iter->list->items[idx_to_delete].data;
    // Shift elements to fill the gap left by the deleted item
    if (iter->current < iter->list->item_count / 2) {
        // Shift elements between Head and current forward
        for (size_t i = iter->current; i > 0; i--) {
            size_t dest = (iter->list->head + i) % iter->list->capacity;
            size_t src = (iter->list->head + i - 1) % iter->list->capacity;
            iter->list->items[dest].data = iter->list->items[src].data;
        }
        iter->list->items[iter->list->head].data = NULL;
        iter->list->head = (iter->list->head + 1) % iter->list->capacity;
    } else {
        // Shift elements between current and Tail backward
        for (size_t i = iter->current; i < iter->list->item_count - 1; i++) {
            size_t dest = (iter->list->head + i) % iter->list->capacity;
            size_t src = (iter->list->head + i + 1) % iter->list->capacity;
            iter->list->items[dest].data = iter->list->items[src].data;
        }
        iter->list->items[iter->list->tail].data = NULL;
        iter->list->tail = (iter->list->tail == 0) ? iter->list->capacity - 1 : iter->list->tail - 1;
    }
    iter->list->item_count--;

    iter->current_deleted = 1; // set deleted flag

    // Release lock after modification
    lock_release(iter->list->lock);

    return deleted_data;
}

static int8_t arraylist_iterator_destroy(iterator_t* iterator) {
    if(iterator == NULL) {
        return -1;
    }

    arraylist_iterator_internal_t* iter = (arraylist_iterator_internal_t*)iterator->metadata;

    if(iter == NULL) {
        return -1;
    }

    memory_heap_t* heap = NULL;

    if(iter->list != NULL) {
        heap = iter->list->heap;
        // Release the lock if it was acquired by the iterator creation
        // Note: The lock is acquired in arraylist_iterator_create and should be released here.
        // However, the current implementation of arraylist_iterator_create doesn't explicitly
        // store the lock state to know if it needs releasing. Assuming it's always acquired.
        lock_release(iter->list->lock);
    }

    memory_free_ext(heap, iter);
    memory_free_ext(heap, iterator);

    return 0;
}

iterator_t* arraylist_iterator_create(list_t* list) {
    if(list == NULL) {
        return NULL;
    }

    // Acquire lock to ensure list consistency during iterator creation
    lock_acquire(list->lock);

    iterator_t* iterator = memory_malloc_ext(list->heap, sizeof(iterator_t), 0x0);

    if(iterator == NULL) {
        lock_release(list->lock); // Release lock if allocation fails
        return NULL;
    }

    arraylist_iterator_internal_t* iter = memory_malloc_ext(list->heap, sizeof(arraylist_iterator_internal_t), 0x0);

    if(iter == NULL) {
        memory_free_ext(list->heap, iterator);
        lock_release(list->lock); // Release lock if allocation fails
        return NULL;
    }

    iter->list = list;
    // Initialize current to 0, representing the first logical element.
    // The actual index in the circular buffer is calculated using head.
    iter->current = 0;
    iter->current_deleted = 0; // Initialize deleted flag

    iterator->metadata = iter;
    iterator->destroy = &arraylist_iterator_destroy;
    iterator->next = &arraylist_iterator_next;
    iterator->end_of_iterator = &arraylist_iterator_end_of_list;
    iterator->get_item = &arraylist_iterator_get_item;
    iterator->delete_item = &arraylist_iterator_delete_item;
    iterator->get_extra_data = NULL; // Not used in this implementation

    return iterator;
}
