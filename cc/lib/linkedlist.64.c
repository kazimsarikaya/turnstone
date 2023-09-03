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

MODULE("turnstone.lib");

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
    linkedlist_item_internal_t*  middle;
    size_t                       middle_position;
    int8_t                       balance;
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
    size_t                      current_position;
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

int8_t linkedlist_narrow(linkedlist_internal_t* list, size_t s, const void* data, linkedlist_item_internal_t** head, linkedlist_item_internal_t** tail, size_t* position);

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
    linkedlist_internal_t* l = list;
    memory_heap_t* heap = l->heap;

    linkedlist_item_internal_t* li = l->head;

    while(li) {
        if(type == LINKEDLIST_DESTROY_WITH_DATA) {
            memory_free_ext(heap, (void*)li->data);
        }

        linkedlist_item_internal_t* n_li = li->next;
        memory_free_ext(heap, li);
        li = n_li;
    }

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
    linkedlist_internal_t* l = (linkedlist_internal_t*)list;

    if(l == NULL) {
        return -1ULL;
    }

    lock_acquire(l->lock);

    size_t result = 0;
    linkedlist_item_internal_t* item = memory_malloc_ext(l->heap, sizeof(linkedlist_item_internal_t), 0x0);

    if(item == NULL) {
        lock_release(l->lock);

        return -1ULL;
    }

    item->data = data;

    if(l->head == NULL) { // if head is null insert both head and tail and return
        l->head = item;
        l->tail = item;
        l->middle = item;
        l->balance = 0;
        l->item_count++;
        lock_release(l->lock);

        return 0;
    }

    if(where == LINKEDLIST_INSERT_AT_HEAD) {
        item->next = l->head;
        l->head->previous = item;
        l->head = item;
        result = 0;

        l->balance--;
        l->middle_position++;

        if(l->balance == -2) {
            l->middle = l->middle->previous;
            l->balance = 0;
            l->middle_position--;
        } else if (l->balance == 2) {
            l->middle = l->middle->next;
            l->balance = 0;
            l->middle_position++;
        }

    } else if(where == LINKEDLIST_INSERT_AT_TAIL) {
        item->previous = l->tail;
        l->tail->next = item;
        l->tail = item;
        result = l->item_count;

        l->balance++;

        if(l->balance == -2) {
            l->middle = l->middle->previous;
            l->middle_position--;
            l->balance = 0;
        } else if(l->balance == 2) {
            l->middle = l->middle->next;
            l->middle_position++;
            l->balance = 0;
        }

    } else if(where == LINKEDLIST_INSERT_AT_SORTED) {
        linkedlist_item_internal_t* cur = l->middle;
        linkedlist_item_internal_t* h = l->head;
        linkedlist_item_internal_t* t = l->tail;
        boolean_t before_middle = false;
        result = 0;

        int8_t c_res = -1;
        uint8_t insert_at_end = 0;

        if(l->comparator(item->data, cur->data) <= 0) {
            c_res = linkedlist_narrow(l, l->middle_position + 1, item->data, &h, &cur, &result);
            cur = h;
            before_middle = true;
        } else {
            result = l->middle_position;
            c_res = linkedlist_narrow(l, l->item_count - l->middle_position, item->data, &cur, &t, &result);
        }

        if(c_res == 1) {
            if(!cur->next) {
                insert_at_end = 1;
                cur = l->tail;
            }
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

        if(before_middle) {
            l->middle_position++;
            l->balance--;
        } else {
            l->balance++;
        }

        if(l->balance == -2) {
            l->middle = l->middle->previous;
            l->middle_position--;
            l->balance = 0;
        } else if (l->balance == 2) {
            l->middle = l->middle->next;
            l->middle_position++;
            l->balance = 0;
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

        if(result > l->middle_position) {
            l->balance++;
        } else {
            l->middle_position++;
            l->balance--;
        }

        if(l->balance == -2) {
            l->middle = l->middle->previous;
            l->balance = 0;
            l->middle_position--;
        } else if (l->balance == 2) {
            l->middle = l->middle->next;
            l->balance = 0;
            l->middle_position++;
        }
    } else {
        memory_free_ext(l->heap, item);
        lock_release(l->lock);

        return -1ULL;
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

    if(position >= l->item_count) {
        return NULL;
    }

    if(!position && l->item_count == 1) {
        where = LINKEDLIST_DELETE_AT_HEAD;
    }

    if(l->item_count == 0) {
        return NULL;
    }

    if(!l->head) {
        return NULL;
    }

    lock_acquire(l->lock);

    const void* result = NULL;

    if(where == LINKEDLIST_DELETE_AT_HEAD) {
        linkedlist_item_internal_t* item = l->head;
        l->head = l->head->next;

        if(l->head == NULL) {
            l->tail = NULL;
            l->middle = NULL;
        } else {
            l->head->previous = NULL;

            if(l->middle == item) {
                l->middle = l->middle->next;
                l->balance--;
            }
        }

        result = item->data;
        memory_free_ext(l->heap, item);
        l->item_count--;

        l->balance++;
        l->middle_position--;

        if(l->middle) {
            if(l->balance == -2) {
                l->middle = l->middle->previous;
                l->balance = 0;
                l->middle_position--;
            } else if (l->balance == 2) {
                l->middle = l->middle->next;
                l->balance = 0;
                l->middle_position++;
            }
        } else {
            l->balance = 0;
            l->middle_position = 0;
        }

    } else if(where == LINKEDLIST_DELETE_AT_TAIL) {
        linkedlist_item_internal_t* item = l->tail;
        l->tail = l->tail->previous;

        if(l->tail == NULL) {
            l->head = NULL;
            l->middle = NULL;
        } else {
            l->tail->next = NULL;

            if(l->middle == item) {
                l->middle = l->middle->previous;
                l->balance++;
            }
        }

        result = item->data;
        memory_free_ext(l->heap, item);
        l->item_count--;

        l->balance--;

        if(l->middle) {
            if(l->balance == -2) {
                l->middle = l->middle->previous;
                l->balance = 0;
                l->middle_position--;
            } else if (l->balance == 2) {
                l->middle = l->middle->next;
                l->balance = 0;
                l->middle_position++;
            }
        } else {
            l->balance = 0;
            l->middle_position = 0;
        }

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
        linkedlist_item_internal_t* cur = l->head;
        size_t index = 0;

        if(position >= l->middle_position) {
            cur = l->middle;
            index = l->middle_position;
        }

        while(index < position) {
            cur = cur->next;

            if(cur == NULL) {
                break;
            }

            index++;
        }

        if(!cur) {
            lock_release(l->lock);

            return NULL;
        }

        linkedlist_item_internal_t* previous = cur->previous;
        linkedlist_item_internal_t* next = cur->next;

        if(previous == NULL) {
            l->head = next;

            if(next != NULL) {
                next->previous = NULL;
            }
        } else {
            previous->next = next;
        }

        if(next == NULL) {
            l->tail = previous;

            if(previous != NULL) {
                previous->next = NULL;
            }
        } else {
            next->previous = previous;
        }

        if(l->middle_position == index) {
            if(previous) {
                l->middle = l->middle->previous;
                l->middle_position--;
                l->balance++;
            } else {
                l->middle = l->middle->next;
                l->balance--;
            }
        } else if(index < l->middle_position) {
            l->balance++;
            l->middle_position--;
        } else {
            l->balance--;
        }

        if(l->balance == -2) {
            l->middle = l->middle->previous;
            l->balance = 0;
            l->middle_position--;
        } else if (l->balance == 2) {
            l->middle = l->middle->next;
            l->balance = 0;
            l->middle_position++;
        }

        result = cur->data;
        l->item_count--;
        memory_free_ext(l->heap, cur);
    }

    lock_release(l->lock);

    return result;
}

int8_t linkedlist_narrow(linkedlist_internal_t* list, size_t s, const void* data, linkedlist_item_internal_t** head, linkedlist_item_internal_t** tail, size_t* position) {
    linkedlist_item_internal_t* h = *head;
    linkedlist_item_internal_t* t = *tail;

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

int8_t linkedlist_get_position(linkedlist_t list, const void* data, size_t* position) {
    linkedlist_internal_t* l = (linkedlist_internal_t*)list;

    if(position) {
        *position = 0;
    }

    if(l->type == LINKEDLIST_TYPE_SORTEDLIST && !l->equality_comparator) {
        linkedlist_item_internal_t* h = l->head;
        linkedlist_item_internal_t* t = l->tail;
        linkedlist_item_internal_t* m = l->middle;

        int8_t c_res = -1;

        if(l->comparator(data, m->data) < 0) {
            c_res = linkedlist_narrow(l, l->middle_position + 1, data, &h, &m, position);
        } else {
            if(position) {
                (*position) = l->middle_position;
            }
            c_res = linkedlist_narrow(l, l->item_count - l->middle_position, data, &m, &t, position);
        }

        return c_res;
    }

    int8_t res = -1;


    linkedlist_data_comparator_f cmp = l->comparator;

    if(l->equality_comparator) {
        cmp = l->equality_comparator;
    }

    if(!cmp) {
        cmp = linkedlist_default_data_comparator;
    }

    linkedlist_item_internal_t* li = l->middle;

    if(!li) {
        li = l->head;
    } else {
        if(cmp(data, li->data) == -1) {
            li = l->head;
        } else {
            if(position) {
                (*position) = l->middle_position;
            }
        }
    }

    while(li) {

        if(cmp(li->data, data) == 0) {
            res = 0;
            break;
        }

        if(position) {
            (*position)++;
        }

        li = li->next;
    }

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

    size_t rem = position;
    linkedlist_item_internal_t* li = l->head;
    boolean_t to_left = false;

    if(l->middle) {
        if(rem >= l->middle_position) {
            li = l->middle;
            rem -= l->middle_position;

            size_t rev_position = l->item_count - l->middle_position - rem - 1;

            if(rem > rev_position) {
                li = l->tail;
                rem = rev_position;
                to_left = true;
            }

        } else {
            size_t rev_position = l->middle_position - rem;

            if(rem > rev_position) {
                li = l->middle;
                rem = rev_position;
                to_left = true;
            }

        }
    } else if(l->item_count > position + 1) {
        size_t rev_position = l->item_count - rem - 1;

        if(rem > rev_position) {
            li = l->tail;
            rem = rev_position;
            to_left = true;
        }
    }

    if(to_left) {
        while(li) {
            if(rem == 0) {
                result = li->data;

                break;
            }

            rem--;
            li = li->previous;
        }

        return result;
    } else {
        while(li) {
            if(rem == 0) {
                result = li->data;

                break;
            }

            rem--;
            li = li->next;
        }

    }

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
    linkedlist_item_internal_t* li = ((linkedlist_iterator_internal_t*)iterator->metadata)->current;

    if(li == NULL) {
        return NULL;
    }

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

linkedlist_item_t linkedlist_insert_at_head_and_get_linkedlist_item(linkedlist_t list, const void* data) {
    if(!list) {
        return NULL;
    }

    linkedlist_insert_at_head(list, data);

    linkedlist_internal_t* l = list;

    return l->head;
}

boolean_t linkedlist_move_item_to_head(linkedlist_t list, linkedlist_item_t item) {
    if(!list || !item) {
        return false;
    }

    linkedlist_internal_t* l = list;
    linkedlist_item_internal_t* li = item;

    if(l->head == li) {
        return true;
    }

    if(li->previous) {
        li->previous->next = li->next;
    }

    if(li->next) {
        li->next->previous = li->previous;
    }

    if(l->tail == li) {
        l->tail = li->previous;

        if(!l->tail) {
            l->tail = l->head;
            l->tail->next = NULL;
        }
    }

    li->next = l->head;
    l->head->previous = li;
    li->previous = NULL;
    l->head = li;

    return true;
}

boolean_t linkedlist_delete_linkedlist_item(linkedlist_t list, linkedlist_item_t item) {
    if(!list || !item) {
        return false;
    }

    linkedlist_internal_t* l = list;
    linkedlist_item_internal_t* li = item;

    if(li->previous) {
        li->previous->next = li->next;
    } else {
        l->head = li->next;
        l->head->previous = NULL;
    }

    if(li->next) {
        li->next->previous = li->previous;
    } else {
        l->tail = li->previous;
        l->tail->next = NULL;
    }

    l->item_count--;

    memory_free(li);

    return true;

}
