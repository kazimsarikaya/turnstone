/**
 * @file list_linked.64.c
 * @brief linked list types implementations
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

/**
 * @struct linkedlist_item_internal
 * @brief linked list internal  type.
 *
 * double linked list item
 */
typedef struct list_item_t {
    const void*         data; ///< the data inside list item
    struct list_item_t* next; ///< next list item
    struct list_item_t* previous; ///< previous list item
}list_item_t; ///<short hand for struct

/**
 * @struct list_t
 * @brief linked list internal interface
 */
typedef struct list_t {
    memory_heap_t*         heap; ///< the heap of the list
    list_type_t            type; ///< list type
    lock_t*                lock; ///< lock for the list
    list_data_comparator_f comparator; ///< if the list is sorted, this is comparator function for data
    list_data_comparator_f equality_comparator; ///< if the list is sorted, this is comparator function for data
    size_t                 item_count; ///< item count at the list, for fast access.
    indexer_t              indexer; ///< if the list is indexed, this is the indexer
    list_item_t*           head; ///< head of the list
    list_item_t*           tail; ///< tail of the list
    list_item_t*           middle; ///< middle of the list
    size_t                 middle_position; ///< middle position of the list
    int8_t                 balance; ///< balance of the list
}list_t; ///< short hand for struct

/**
 * @struct linkedlist_iterator_internal_t
 * @brief iterator struct
 */
typedef struct linkedlist_iterator_internal_t {
    list_t*      list; ///< owner list
    list_item_t* current; ///< current item at the iterator
    uint8_t      current_deleted; ///< if current item is deleted it is to be 1.
    size_t       current_position;
} linkedlist_iterator_internal_t; ///<short hand for struct


list_t* linkedlist_create_with_type(memory_heap_t* heap, list_type_t type,
                                    list_data_comparator_f comparator, indexer_t indexer);
uint8_t     linkedlist_destroy_with_type(list_t* list, list_destroy_type_t type, list_item_destroyer_callback_f destroyer);
size_t      linkedlist_insert_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);
const void* linkedlist_delete_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);
list_t*     linkedlist_duplicate_list_with_heap(memory_heap_t* heap, list_t* list);
iterator_t* linkedlist_iterator_create(list_t* list);
const void* linkedlist_get_data_at_position(list_t* list, size_t position);
int8_t      linkedlist_get_position(list_t* list, const void* data, size_t* position);

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

int8_t linkedlist_narrow(list_t* list, size_t s, const void* data, list_item_t** head, list_item_t** tail, size_t* position);

/**
 * @brief deletes current item.
 * @param[in]  iterator the iterator
 * @return deleted item
 *
 * current item is deleted and the value of item is returned. also list metadata is updated.
 */
const void* linkedlist_iterator_delete_item(iterator_t* iterator);

list_t* linkedlist_create_with_type(memory_heap_t* heap, list_type_t type,
                                    list_data_comparator_f comparator, indexer_t indexer){
    heap = memory_get_heap(heap); // get rid of null pointer, so heap is always stable.

    list_t* list;

    list = memory_malloc_ext(heap, sizeof(list_t), 0x0);

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
    list->item_count = 0;
    list->head = NULL;
    list->tail = NULL;
    list->lock = lock_create_with_heap(list->heap);

    return list;
}

uint8_t linkedlist_destroy_with_type(list_t* list, list_destroy_type_t type, list_item_destroyer_callback_f destroyer){
    if(list == NULL) {
        return 0;
    }

    // TODO: check errors
    memory_heap_t* heap = list->heap;

    list_item_t* item = list->head;

    while(item) {
        if(type == LIST_DESTROY_WITH_DATA) {
            if(destroyer) {
                destroyer(heap, (void*)item->data);
            } else {
                memory_free_ext(heap, (void*)item->data);
            }
        }

        list_item_t* n_li = item->next;
        memory_free_ext(heap, item);
        item = n_li;
    }

    lock_destroy(((list_t*)list)->lock);

    return memory_free_ext(heap, list);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
size_t linkedlist_insert_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position){
    list_t* l = (list_t*)list;

    if(l == NULL) {
        return -1ULL;
    }

    lock_acquire(list->lock);

    size_t result = 0;
    list_item_t* item = memory_malloc_ext(list->heap, sizeof(list_item_t), 0x0);

    if(item == NULL) {
        lock_release(list->lock);

        return -1ULL;
    }

    item->data = data;

    if(list->head == NULL) { // if head is null insert both head and tail and return
        list->head = item;
        list->tail = item;
        list->middle = item;
        list->balance = 0;
        list->item_count++;
        lock_release(list->lock);

        return 0;
    }

    if(where == LIST_INSERT_AT_HEAD) {
        item->next = list->head;
        list->head->previous = item;
        list->head = item;
        result = 0;

        list->balance--;
        list->middle_position++;

        if(list->balance == -2) {
            list->middle = list->middle->previous;
            list->balance = 0;
            list->middle_position--;
        } else if (list->balance == 2) {
            list->middle = list->middle->next;
            list->balance = 0;
            list->middle_position++;
        }

    } else if(where == LIST_INSERT_AT_TAIL) {
        item->previous = list->tail;
        list->tail->next = item;
        list->tail = item;
        result = list->item_count;

        list->balance++;

        if(list->balance == -2) {
            list->middle = list->middle->previous;
            list->middle_position--;
            list->balance = 0;
        } else if(list->balance == 2) {
            list->middle = list->middle->next;
            list->middle_position++;
            list->balance = 0;
        }

    } else if(where == LIST_INSERT_AT_SORTED) {
        list_item_t* cur = list->middle;
        list_item_t* h = list->head;
        list_item_t* t = list->tail;
        boolean_t before_middle = false;
        result = 0;

        int8_t c_res = -1;
        uint8_t insert_at_end = 0;

        if(list->comparator(item->data, cur->data) <= 0) {
            c_res = linkedlist_narrow(l, list->middle_position + 1, item->data, &h, &cur, &result);
            cur = h;
            before_middle = true;
        } else {
            result = list->middle_position;
            c_res = linkedlist_narrow(l, list->item_count - list->middle_position, item->data, &cur, &t, &result);
        }

        if(c_res == 1) {
            if(!cur->next) {
                insert_at_end = 1;
                cur = list->tail;
            }
        }

        if(insert_at_end == 0) {
            if(cur->previous != NULL) {
                cur->previous->next = item;
            } else {
                list->head = item;
            }

            item->next = cur;
            item->previous = cur->previous;
            cur->previous = item;

        } else {
            list->tail = item;
            cur->next = item;
            item->previous = cur;
            result = list->item_count;
        }

        if(before_middle) {
            list->middle_position++;
            list->balance--;
        } else {
            list->balance++;
        }

        if(list->balance == -2) {
            list->middle = list->middle->previous;
            list->middle_position--;
            list->balance = 0;
        } else if (list->balance == 2) {
            list->middle = list->middle->next;
            list->middle_position++;
            list->balance = 0;
        }

    } else if(where == LIST_INSERT_AT_INDEXED) {
        item->next = list->head;
        list->head->previous = item;
        list->head = item;
        indexer_index(list->indexer, data, item);
        result = 0;

    }else if(where == LIST_INSERT_AT_POSITION) {
        list_item_t* cur = list->head;
        size_t index = 0;

        while(index < position) {
            cur = cur->next;

            if(cur == NULL) {
                break;
            }

            index++;
        }

        if(cur == NULL) {
            item->previous = list->tail;
            list->tail->next = item;
            list->tail = item;
            result = list->item_count;

        } else {
            list_item_t* old_prev = cur->previous;
            cur->previous = item;
            item->next = cur;
            item->previous = old_prev;

            if(old_prev != NULL) {
                old_prev->next = item;
            } else {
                list->head = item;
            }

            result = index;
        }

        if(result > list->middle_position) {
            list->balance++;
        } else {
            list->middle_position++;
            list->balance--;
        }

        if(list->balance == -2) {
            list->middle = list->middle->previous;
            list->balance = 0;
            list->middle_position--;
        } else if (list->balance == 2) {
            list->middle = list->middle->next;
            list->balance = 0;
            list->middle_position++;
        }
    } else {
        memory_free_ext(list->heap, item);
        lock_release(list->lock);

        return -1ULL;
    }

    list->item_count++;
    lock_release(list->lock);

    return result;
}
#pragma GCC diagnostic pop

const void* linkedlist_delete_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position){
    if(data == NULL && where == LIST_DELETE_AT_FINDBY) {
        return NULL;
    }

    list_t* l = (list_t*)list;

    if(l == NULL) {
        return NULL;
    }

    if(position >= list->item_count) {
        return NULL;
    }

    if(!position && list->item_count == 1) {
        where = LIST_DELETE_AT_HEAD;
    }

    if(list->item_count == 0) {
        return NULL;
    }

    if(!list->head) {
        return NULL;
    }

    lock_acquire(list->lock);

    const void* result = NULL;

    if(where == LIST_DELETE_AT_HEAD) {
        list_item_t* item = list->head;
        list->head = list->head->next;

        if(list->head == NULL) {
            list->tail = NULL;
            list->middle = NULL;
        } else {
            list->head->previous = NULL;

            if(list->middle == item) {
                list->middle = list->middle->next;
                list->balance--;
            }
        }

        result = item->data;
        memory_free_ext(list->heap, item);
        list->item_count--;

        list->balance++;
        list->middle_position--;

        if(list->middle) {
            if(list->balance == -2) {
                list->middle = list->middle->previous;
                list->balance = 0;
                list->middle_position--;
            } else if (list->balance == 2) {
                list->middle = list->middle->next;
                list->balance = 0;
                list->middle_position++;
            }
        } else {
            list->balance = 0;
            list->middle_position = 0;
        }

    } else if(where == LIST_DELETE_AT_TAIL) {
        list_item_t* item = list->tail;
        list->tail = list->tail->previous;

        if(list->tail == NULL) {
            list->head = NULL;
            list->middle = NULL;
        } else {
            list->tail->next = NULL;

            if(list->middle == item) {
                list->middle = list->middle->previous;
                list->balance++;
            }
        }

        result = item->data;
        memory_free_ext(list->heap, item);
        list->item_count--;

        list->balance--;

        if(list->middle) {
            if(list->balance == -2) {
                list->middle = list->middle->previous;
                list->balance = 0;
                list->middle_position--;
            } else if (list->balance == 2) {
                list->middle = list->middle->next;
                list->balance = 0;
                list->middle_position++;
            }
        } else {
            list->balance = 0;
            list->middle_position = 0;
        }

    } else if(where == LIST_DELETE_AT_FINDBY) {
        if(list->type == LIST_TYPE_INDEXEDLIST) {

            iterator_t* pk_iter = indexer_search(list->indexer, 0, data, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

            if(pk_iter->end_of_iterator(pk_iter) != 0) {
                result = pk_iter->get_item(pk_iter);
            }

            pk_iter->destroy(pk_iter);

            if(result == NULL) {
                return NULL;
            }

            list_item_t* item = (list_item_t*)result;
            list_item_t* previous = item->previous;
            list_item_t* next = item->next;

            if(previous == NULL) {
                list->head = next;

                if(list->head != NULL) {
                    list->head->previous = NULL;
                } else {
                    list->tail = NULL;
                }

            } else {
                previous->next = next;
            }

            if(next == NULL) {
                list->tail = previous;

                if(list->tail != NULL) {
                    list->tail->next = NULL;
                } else {
                    list->head = NULL;
                }

            } else {
                next->previous = previous;
            }

            // TODO: check error.
            indexer_delete(list->indexer, item);
            result = item->data;
            memory_free_ext(list->heap, item);
            list->item_count--;

        } else {
            iterator_t* iter = linkedlist_iterator_create(l);

            if(iter == NULL) {
                lock_release(list->lock);

                return NULL;
            }

            list_data_comparator_f cmp = list->comparator;

            if(list->equality_comparator) {
                cmp = list->equality_comparator;
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
    } else if(where == LIST_DELETE_AT_POSITION) {
        list_item_t* cur = list->head;
        size_t index = 0;

        if(position >= list->middle_position) {
            cur = list->middle;
            index = list->middle_position;
        }

        while(index < position) {
            cur = cur->next;

            if(cur == NULL) {
                break;
            }

            index++;
        }

        if(!cur) {
            lock_release(list->lock);

            return NULL;
        }

        list_item_t* previous = cur->previous;
        list_item_t* next = cur->next;

        if(previous == NULL) {
            list->head = next;

            if(next != NULL) {
                next->previous = NULL;
            }
        } else {
            previous->next = next;
        }

        if(next == NULL) {
            list->tail = previous;

            if(previous != NULL) {
                previous->next = NULL;
            }
        } else {
            next->previous = previous;
        }

        if(list->middle_position == index) {
            if(previous) {
                list->middle = list->middle->previous;
                list->middle_position--;
                list->balance++;
            } else {
                list->middle = list->middle->next;
                list->balance--;
            }
        } else if(index < list->middle_position) {
            list->balance++;
            list->middle_position--;
        } else {
            list->balance--;
        }

        if(list->balance == -2) {
            list->middle = list->middle->previous;
            list->balance = 0;
            list->middle_position--;
        } else if (list->balance == 2) {
            list->middle = list->middle->next;
            list->balance = 0;
            list->middle_position++;
        }

        result = cur->data;
        list->item_count--;
        memory_free_ext(list->heap, cur);
    }

    lock_release(list->lock);

    return result;
}

int8_t linkedlist_narrow(list_t* list, size_t s, const void* data, list_item_t** head, list_item_t** tail, size_t* position) {
    list_item_t* h = *head;
    list_item_t* t = *tail;

    if(!h || !t) {
        return -1;
    }

    if(h == t) {
        return list->comparator(data, h->data);
    }

    int8_t c_res = -1;

    size_t t_pos = 0;

    if(position) {
        t_pos = *position;
    }

    while(h != t && s) {
        int8_t c_res_h = list->comparator(data, h->data);

        if(c_res_h == 0) {
            c_res = 0;

            break;
        }

        int8_t c_res_t = list->comparator(data, t->data);

        if(c_res_t == 0) {
            t_pos += s - 1;
            c_res = 0;
            h = t;

            (*head) = h;

            break;
        }

        if(s == 2) {
            break;
        }

        if(s == 3) {
            if(c_res_h == 1) {
                h = h->next;
                t_pos += 1;

                (*head) = h;
            }

            break;
        }

        size_t q_s = s >> 2;
        size_t iter_c = q_s;

        while(iter_c) {
            h = h->next;
            t = t->previous;
            iter_c--;
        }

        if(list->comparator(data, h->data) <= 0) {
            t = h;
            h = *head;

            s = q_s + 1;
        } else if(list->comparator(data, t->data) >= 0) {
            h = t;
            t = *tail;

            t_pos += s - (q_s + 1);

            s = q_s + 1;
        } else {
            s -= 2 * q_s;

            t_pos += q_s;
        }

        *head = h;
        *tail = t;
    }

    (*head) = h;

    if(c_res == 0) {
        while(h->previous) {
            if(list->comparator(data, h->previous->data) == 0) {
                h = h->previous;
                (*head) = h;

                t_pos--;
            } else {
                if(position) {
                    *position = t_pos;
                }

                return 0;
            }
        }
    }

    c_res = list->comparator(data, h->data);

    if(c_res == -1) {
        if(position) {
            *position = t_pos;
        }

        return -1;
    }

    if(c_res == 0) {
        while(h->previous) {
            if(list->comparator(data, h->previous->data) == 0) {
                h = h->previous;
                (*head) = h;

                t_pos--;
            } else {
                if(position) {
                    *position = t_pos;
                }

                return 0;
            }
        }
    }

    while(c_res == 1) {
        if(!h->next) {
            return 1;
        }

        h = h->next;
        (*head) = h;

        t_pos++;

        c_res = list->comparator(data, h->data);
    }

    if(position) {
        *position = t_pos;
    }

    return c_res;
}

int8_t linkedlist_get_position(list_t* list, const void* data, size_t* position) {
    if(position) {
        *position = 0;
    }

    if(list->item_count == 0) {
        return -1;
    }

    if(list->type == LIST_TYPE_SORTEDLIST && !list->equality_comparator) {
        list_item_t* h = list->head;
        list_item_t* t = list->tail;
        list_item_t* m = list->middle;

        int8_t c_res = -1;

        if(list->comparator(data, m->data) < 0) {
            c_res = linkedlist_narrow(list, list->middle_position + 1, data, &h, &m, position);
        } else {
            if(position) {
                (*position) = list->middle_position;
            }
            c_res = linkedlist_narrow(list, list->item_count - list->middle_position, data, &m, &t, position);
        }

        return c_res;
    }

    int8_t res = -1;


    list_data_comparator_f cmp = list->comparator;

    if(list->equality_comparator) {
        cmp = list->equality_comparator;
    }

    if(!cmp) {
        cmp = list_default_data_comparator;
    }

    list_item_t* item = list->middle;

    if(!item) {
        item = list->head;
    } else {
        if(cmp(data, item->data) == -1) {
            item = list->head;
        } else {
            if(position) {
                (*position) = list->middle_position;
            }
        }
    }

    while(item) {

        if(cmp(item->data, data) == 0) {
            res = 0;
            break;
        }

        if(position) {
            (*position)++;
        }

        item = item->next;
    }

    return res;
}

const void* linkedlist_get_data_at_position(list_t* list, size_t position){
    const void* result = NULL;

    if(list == NULL) {
        return result;
    }

    if(position >= list->item_count) {
        return result;
    }

    size_t rem = position;
    list_item_t* item = list->head;
    boolean_t to_left = false;

    if(list->middle) {
        if(rem >= list->middle_position) {
            item = list->middle;
            rem -= list->middle_position;

            size_t rev_position = list->item_count - list->middle_position - rem - 1;

            if(rem > rev_position) {
                item = list->tail;
                rem = rev_position;
                to_left = true;
            }

        } else {
            size_t rev_position = list->middle_position - rem;

            if(rem > rev_position) {
                item = list->middle;
                rem = rev_position;
                to_left = true;
            }

        }
    } else if(list->item_count > position + 1) {
        size_t rev_position = list->item_count - rem - 1;

        if(rem > rev_position) {
            item = list->tail;
            rem = rev_position;
            to_left = true;
        }
    }

    if(to_left) {
        while(item) {
            if(rem == 0) {
                result = item->data;

                break;
            }

            rem--;
            item = item->previous;
        }

        return result;
    } else {
        while(item) {
            if(rem == 0) {
                result = item->data;

                break;
            }

            rem--;
            item = item->next;
        }

    }

    return result;
}

iterator_t* linkedlist_iterator_create(list_t* list) {
    if(list == NULL) {
        return NULL;
    }

    lock_acquire(list->lock);

    iterator_t* iterator = memory_malloc_ext(list->heap, sizeof(iterator_t), 0x0);

    if(iterator == NULL) {
        lock_release(list->lock);

        return NULL;
    }

    linkedlist_iterator_internal_t* iter = memory_malloc_ext(list->heap, sizeof(linkedlist_iterator_internal_t), 0x0);

    if(iter == NULL) {
        memory_free_ext(list->heap, iterator);
        lock_release(list->lock);

        return NULL;
    }

    iter->list = list;
    iter->current = list->head;
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
            iter->current_position++;
        }
    }

    return iterator;
}

int8_t linkedlist_iterator_end_of_list(iterator_t* iterator) {
    linkedlist_iterator_internal_t* iter = (linkedlist_iterator_internal_t*)iterator->metadata;
    return iter->current == NULL ? 0 : 1;
}

const void* linkedlist_iterator_get_item(iterator_t* iterator) {
    list_item_t* item = ((linkedlist_iterator_internal_t*)iterator->metadata)->current;
    return item->data;
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
    list_item_t* current = iter->current;
    list_item_t* previous = iter->current->previous;
    list_item_t* next = iter->current->next;

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

    if(iter->list->middle_position == iter->current_position) {
        if(previous) {
            iter->list->middle = previous;
            iter->list->middle_position--;
            iter->list->balance++;
        } else {
            iter->list->middle = next;
            iter->list->balance--;
        }
    }else if(iter->current_position > iter->list->middle_position) {
        iter->list->balance--;
    } else {
        iter->list->balance++;
        iter->list->middle_position--;
    }

    if(iter->list->balance == -2) {
        iter->list->middle = iter->list->middle->previous;
        iter->list->balance = 0;
        iter->list->middle_position--;
    } else if (iter->list->balance == 2) {
        iter->list->middle = iter->list->middle->next;
        iter->list->balance = 0;
        iter->list->middle_position++;
    }

    iter->current = next;
    iter->current_deleted = 1;
    iter->list->item_count--;
    memory_free_ext(iter->list->heap, current);

    if(iter->list->type == LIST_TYPE_INDEXEDLIST) {
        // TODO: check error
        indexer_delete(iter->list->indexer, data);
    }

    return data;
}

list_t* linkedlist_duplicate_list_with_heap(memory_heap_t* heap, list_t* list) {

    if(list == NULL) {
        return NULL;
    }

    list_t* new_list;
    list_t* source_list = (list_t*)list;
    if(heap == NULL) {
        heap = source_list->heap;
    }
    new_list = memory_malloc_ext(heap, sizeof(list_t), 0x0);

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
        linkedlist_insert_at(new_list, item, LIST_INSERT_AT_TAIL, 0);
        iter = iter->next(iter);
    }
    iter->destroy(iter);
    return new_list;
}

#if 0
list_item_t* linkedlist_insert_at_head_and_get_linkedlist_item(list_t* list, const void* data) {
    if(!list) {
        return NULL;
    }

    linkedlist_insert_at_head(list, data);

    return list->head;
}

boolean_t linkedlist_move_item_to_head(list_t* list, list_item_t* item) {
    if(!list || !item) {
        return false;
    }

    if(list->head == item) {
        return true;
    }

    if(item->previous) {
        item->previous->next = item->next;
    }

    if(item->next) {
        item->next->previous = item->previous;
    }

    if(list->tail == item) {
        list->tail = item->previous;

        if(!list->tail) {
            list->tail = list->head;
            list->tail->next = NULL;
        }
    }

    item->next = list->head;
    list->head->previous = item;
    item->previous = NULL;
    list->head = item;

    return true;
}

boolean_t linkedlist_delete_linkedlist_item(list_t* list, list_item_t* item) {
    if(!list || !item) {
        return false;
    }

    if(item->previous) {
        item->previous->next = item->next;
    } else {
        list->head = item->next;
        list->head->previous = NULL;
    }

    if(item->next) {
        item->next->previous = item->previous;
    } else {
        list->tail = item->previous;
        list->tail->next = NULL;
    }

    list->item_count--;

    memory_free(item);

    return true;

}
#endif
