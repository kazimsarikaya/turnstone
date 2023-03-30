/**
 * @file linkedlist.64.c
 * @brief linked list types implementations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <types.h>
#include <linkedlist.h>
#include <indexer.h>
#include <cpu/sync.h>
#include <strings.h>
#include <video.h>

/**
 * @struct linkedlist_item_internal
 * @brief linked list internal  type.
 *
 * double linked list item
 */
typedef struct linkedlist_item_internal_t {
    const void*                        data;       ///< the data inside list item
    struct linkedlist_item_internal_t* next; ///< next list item
    struct linkedlist_item_internal_t* previous; ///< previous list item
}linkedlist_item_internal_t; ///<short hand for struct

/**
 * @struct linkedlist_internal_t
 * @brief linked list internal interface
 */
typedef struct linkedlist_internal_t {
    memory_heap_t*               heap; ///< the heap of the list
    linkedlist_type_t            type; ///< list type
    linkedlist_data_comparator_f comparator; ///< if the list is sorted, this is comparator function for data
    linkedlist_data_comparator_f equality_comparator; ///< if the list is sorted, this is comparator function for data
    indexer_t                    indexer; ///< if the list is indexed, this is the indexer
    size_t                       item_count; ///< item count at the list, for fast access.
    linkedlist_item_internal_t*  head; ///< head of the list
    linkedlist_item_internal_t*  tail; ///< tail of the list
    lock_t                       lock;
}linkedlist_internal_t; ///< short hand for struct

/**
 * @struct linkedlist_iterator_internal_t
 * @brief iterator struct
 */
typedef struct linkedlist_iterator_internal_t {
    linkedlist_internal_t*      list; ///< owner list
    linkedlist_item_internal_t* current; ///< current item at the iterator
    uint8_t                     current_deleted; ///< if current item is deleted it is to be 1.
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

/**
 * @brief destroys the iterator
 * @param[in]  iterator to destroy.
 * @return  0 if succeed.
 */
int8_t linkedlist_iterator_destroy(iterator_t* iterator);

/**
 * @brief iterates next item of list.
 * @param[in]  iterator the iterator
 * @return          itself
 */
iterator_t* linkedlist_iterator_next(iterator_t* iterator);

/**
 * @brief checks if iterator is at end of the list
 * @param[in]  iterator the iterator
 * @return  0 if the iterator is at the end of list.
 */
int8_t linkedlist_iterator_end_of_list(iterator_t* iterator);

/**
 * @brief returns data at current item
 * @param[in]  iterator the iterator
 * @return data
 */
const void* linkedlist_iterator_get_item(iterator_t* iterator);

int8_t linkedlist_string_comprator(const void* data1, const void* data2) {
    return strcmp((char_t*)data1, (char_t*)data2);
}

/**
 * @brief deletes current item.
 * @param[in]  iterator the iterator
 * @return deleted item
 *
 * current item is deleted and the value of item is returned. also list metadata is updated.
 */
const void* linkedlist_iterator_delete_item(iterator_t* iterator);

linkedlist_t linkedlist_create_with_type(memory_heap_t* heap, linkedlist_type_t type,
                                         linkedlist_data_comparator_f comparator, indexer_t indexer){
    linkedlist_internal_t* list;

    list = memory_malloc_ext(heap, sizeof(linkedlist_internal_t), 0x0);

    if(list == NULL) {
        return NULL;
    }

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
    list->lock = lock_create_with_heap(list->heap);

    return list;
}

memory_heap_t* linkedlist_get_heap(linkedlist_t list) {
    if(list == NULL) {
        return NULL;
    }

    linkedlist_internal_t* l = (linkedlist_internal_t*)list;

    return l->heap;
}

linkedlist_data_comparator_f linkedlist_set_comparator(linkedlist_t list, linkedlist_data_comparator_f comparator){
    linkedlist_internal_t* l = (linkedlist_internal_t*)list;
    linkedlist_data_comparator_f old = l->comparator;
    l->comparator = comparator;
    return old;
}

size_t linkedlist_size(linkedlist_t list){
    linkedlist_internal_t* l = (linkedlist_internal_t*)list;

    if(l == NULL) {
        return 0;
    }

    return l->item_count;
}

uint8_t linkedlist_destroy_with_type(linkedlist_t list, linkedlist_destroy_type_t type){
    if(list == NULL) {
        return 0;
    }

    // TODO: check errors
    memory_heap_t* heap = ((linkedlist_internal_t*)list)->heap;
    iterator_t* iter = linkedlist_iterator_create(list);

    if(iter == NULL) {
        return -1;
    }

    while(iter->end_of_iterator(iter) != 0) {
        void* data = (void*)iter->delete_item(iter);
        if(type == LINKEDLIST_DESTROY_WITH_DATA) {
            memory_free_ext(heap, data);
        }
        iter = iter->next(iter);
    }
    iter->destroy(iter);

    lock_destroy(((linkedlist_internal_t*)list)->lock);

    return memory_free_ext(heap, list);
}

const void* linkedlist_get_data_from_listitem(linkedlist_item_t list_item) {
    linkedlist_item_internal_t* li = (linkedlist_item_internal_t*)list_item;

    if(li == NULL) {
        return NULL;
    }

    return li->data;
}

int8_t linkedlist_set_equality_comparator(linkedlist_t list, linkedlist_data_comparator_f comparator){

    linkedlist_internal_t* l = (linkedlist_internal_t*)list;

    if(l == NULL) {
        return -1;
    }

    l->equality_comparator = comparator;

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
size_t linkedlist_insert_at(linkedlist_t list, const void* data, linkedlist_insert_delete_at_t where, size_t position){
    if(data == NULL) {
        return NULL;
    }

    linkedlist_internal_t* l = (linkedlist_internal_t*)list;

    if(l == NULL) {
        return 0;
    }

    lock_acquire(l->lock);

    size_t result = 0;
    linkedlist_item_internal_t* item = memory_malloc_ext(l->heap, sizeof(linkedlist_item_internal_t), 0x0);

    if(item == NULL) {
        lock_release(l->lock);

        return 0;
    }

    item->data = data;

    if(l->head == NULL) { // if head is null insert both head and tail and return
        l->head = item;
        l->tail = item;
        l->item_count++;
        lock_release(l->lock);

        return 0;
    }

    if(where == LINKEDLIST_INSERT_AT_HEAD) {
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

        while(index < position) {
            cur = cur->next;

            if(cur == NULL) {
                break;
            }

            index++;
        }

        if(cur == NULL) {
            item->previous = l->tail;
            l->tail->next = item;
            l->tail = item;
            result = l->item_count;

        } else {
            linkedlist_item_internal_t* old_prev = cur->previous;
            cur->previous = item;
            item->next = cur;
            item->previous = old_prev;

            if(old_prev != NULL) {
                old_prev->next = item;
            } else {
                l->head = item;
            }

            result = index;
        }
    } else {
        memory_free_ext(l->heap, item);
        lock_release(l->lock);

        return 0;
    }

    l->item_count++;
    lock_release(l->lock);

    return result;
}
#pragma GCC diagnostic pop

const void* linkedlist_delete_at(linkedlist_t list, const void* data, linkedlist_insert_delete_at_t where, size_t position){
    if(data == NULL && where == LINKEDLIST_DELETE_AT_FINDBY) {
        return NULL;
    }

    linkedlist_internal_t* l = (linkedlist_internal_t*)list;

    if(l == NULL) {
        return NULL;
    }

    lock_acquire(l->lock);

    if(l->item_count == 0) {
        return NULL;
    }

    const void* result = NULL;

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

            iterator_t* pk_iter = indexer_search(l->indexer, 0, data, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

            if(pk_iter->end_of_iterator(pk_iter) != 0) {
                result = pk_iter->get_item(pk_iter);
            }

            pk_iter->destroy(pk_iter);

            if(result == NULL) {
                return NULL;
            }

            linkedlist_item_internal_t* item = (linkedlist_item_internal_t*)result;
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
            iterator_t* iter = linkedlist_iterator_create(l);

            if(iter == NULL) {
                lock_release(l->lock);

                return NULL;
            }

            linkedlist_data_comparator_f cmp = l->comparator;

            if(l->equality_comparator) {
                cmp = l->equality_comparator;
            }

            while(iter->end_of_iterator(iter) != 0) {
                if(cmp(iter->get_item(iter), data) == 0) {
                    result = iter->delete_item(iter);
                    break;
                }

                iter = iter->next(iter);
            }

            iter->destroy(iter);
        }
    } else if(where == LINKEDLIST_DELETE_AT_POSITION) {
        iterator_t* iter = linkedlist_iterator_create(l);

        if(iter == NULL) {
            lock_release(l->lock);

            return NULL;
        }

        while(iter->end_of_iterator(iter) != 0) {
            if(position == 0) {
                result = iter->delete_item(iter);
                break;
            }

            position--;
            iter = iter->next(iter);
        }

        iter->destroy(iter);

        if(position > 0) {
            result = NULL;
        }
    }

    lock_release(l->lock);

    return result;
}

int8_t linkedlist_get_position(linkedlist_t list, const void* data, size_t* position){
    if(data == NULL) {
        return -1;
    }

    linkedlist_internal_t* l = (linkedlist_internal_t*)list;

    if(position) {
        *position = 0;
    }

    int8_t res = -1;


    linkedlist_data_comparator_f cmp = l->comparator;

    if(l->equality_comparator) {
        cmp = l->equality_comparator;
    }

    iterator_t* iter = linkedlist_iterator_create(l);

    if(iter == NULL) {
        return -1;
    }

    while(iter->end_of_iterator(iter) != 0) {


        if(cmp(iter->get_item(iter), data) == 0) {
            res = 0;
            break;
        }

        if(position) {
            (*position)++;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return res;
}

const void* linkedlist_get_data_at_position(linkedlist_t list, size_t position){
    const void* result = NULL;
    linkedlist_internal_t* l = (linkedlist_internal_t*)list;

    if(l == NULL) {
        return result;
    }

    if(position >= l->item_count) {
        return result;
    }

    iterator_t* iter = linkedlist_iterator_create(l);

    if(iter == NULL) {
        return NULL;
    }

    while(iter->end_of_iterator(iter) != 0) {
        if(position == 0) {
            result = iter->get_item(iter);

            break;
        }

        position--;
        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return result;
}

iterator_t* linkedlist_iterator_create(linkedlist_t list) {
    if(list == NULL) {
        return NULL;
    }

    linkedlist_internal_t* l = (linkedlist_internal_t*)list;

    lock_acquire(l->lock);

    iterator_t* iterator = memory_malloc_ext(l->heap, sizeof(iterator_t), 0x0);

    if(iterator == NULL) {
        lock_release(l->lock);

        return NULL;
    }

    linkedlist_iterator_internal_t* iter = memory_malloc_ext(l->heap, sizeof(linkedlist_iterator_internal_t), 0x0);

    if(iter == NULL) {
        memory_free_ext(l->heap, iterator);
        lock_release(l->lock);

        return NULL;
    }

    iter->list = l;
    iter->current = l->head;
    iterator->metadata = iter;
    iterator->destroy = &linkedlist_iterator_destroy;
    iterator->next = &linkedlist_iterator_next;
    iterator->end_of_iterator = &linkedlist_iterator_end_of_list;
    iterator->get_item = &linkedlist_iterator_get_item;
    iterator->delete_item = &linkedlist_iterator_delete_item;
    iterator->get_extra_data = NULL;

    return iterator;
}

int8_t linkedlist_iterator_destroy(iterator_t* iterator) {
    if(iterator == NULL) {
        return -1;
    }

    linkedlist_iterator_internal_t* iter = (linkedlist_iterator_internal_t*)iterator->metadata;

    if(iter == NULL) {
        return -1;
    }

    memory_heap_t* heap = NULL;

    if(iter->list != NULL) {
        heap = iter->list->heap;
        lock_release(iter->list->lock);
    }

    memory_free_ext(heap, iter);
    memory_free_ext(heap, iterator);

    return 0;
}

iterator_t* linkedlist_iterator_next(iterator_t* iterator) {
    if(iterator == NULL) {
        return NULL;
    }

    linkedlist_iterator_internal_t* iter = (linkedlist_iterator_internal_t*)iterator->metadata;

    if(iter == NULL) {
        return NULL;
    }

    if(iter->current != NULL) {
        if(iter->current_deleted == 1) {
            iter->current_deleted = 0;
        } else {
            iter->current = iter->current->next;
        }
    }

    return iterator;
}

int8_t linkedlist_iterator_end_of_list(iterator_t* iterator) {
    linkedlist_iterator_internal_t* iter = (linkedlist_iterator_internal_t*)iterator->metadata;
    return iter->current == NULL ? 0 : 1;
}

const void* linkedlist_iterator_get_item(iterator_t* iterator) {
    linkedlist_item_internal_t* li = ((linkedlist_iterator_internal_t*)iterator->metadata)->current;
    return li->data;
}

const void* linkedlist_iterator_delete_item(iterator_t* iterator){

    if(iterator == NULL) {
        return NULL;
    }

    linkedlist_iterator_internal_t* iter = (linkedlist_iterator_internal_t*)iterator->metadata;

    if(iter->current == NULL) {
        return NULL;
    }

    const void* data = iter->current->data;
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

linkedlist_t linkedlist_duplicate_list_with_heap(memory_heap_t* heap, linkedlist_t list) {

    if(list == NULL) {
        return NULL;
    }

    linkedlist_internal_t* new_list;
    linkedlist_internal_t* source_list = (linkedlist_internal_t*)list;
    if(heap == NULL) {
        heap = source_list->heap;
    }
    new_list = memory_malloc_ext(heap, sizeof(linkedlist_internal_t), 0x0);

    if(new_list == NULL) {
        return NULL;
    }

    new_list->heap = heap;
    new_list->type = source_list->type;
    new_list->comparator = source_list->comparator;
    new_list->indexer = source_list->indexer;

    iterator_t* iter = linkedlist_iterator_create(list);

    if(iter == NULL) {
        memory_free_ext(heap, new_list);

        return NULL;
    }

    while(iter->end_of_iterator(iter) != 0) {
        const void* item = iter->get_item(iter);
        linkedlist_insert_at(new_list, item, LINKEDLIST_INSERT_AT_TAIL, 0);
        iter = iter->next(iter);
    }
    iter->destroy(iter);
    return new_list;
}
