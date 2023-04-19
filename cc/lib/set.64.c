/**
 * @file set.64.c
 * @brief set interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <set.h>
#include <indexer.h>
#include <bplustree.h>
#include <cpu/sync.h>

struct set_t {
    index_t* index;
    lock_t   lock;
};

set_t* set_create(set_comparator_f cmp) {
    set_t* s = memory_malloc(sizeof(set_t));

    if(!s) {
        return NULL;
    }

    s->index = bplustree_create_index_with_unique(128, cmp, true);

    if(!s->index) {
        memory_free(s);

        return NULL;
    }

    s->lock = lock_create();

    return s;
}


boolean_t set_append(set_t* s, void* value) {
    if(!s) {
        return false;
    }

    lock_acquire(s->lock);

    iterator_t* iter = s->index->search(s->index, value, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

    if(!iter) {
        lock_release(s->lock);

        return false;
    }

    if(iter->end_of_iterator(iter) != 0) {
        iter->destroy(iter);
        lock_release(s->lock);

        return false;
    }

    iter->destroy(iter);

    s->index->insert(s->index, value, value, NULL);

    lock_release(s->lock);

    return true;
}

boolean_t set_exists(set_t* s, void* value) {
    if(!s) {
        return false;
    }

    lock_acquire(s->lock);

    iterator_t* iter = s->index->search(s->index, value, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

    if(!iter) {
        lock_release(s->lock);

        return false;
    }

    if(iter->end_of_iterator(iter) == 0) {
        iter->destroy(iter);
        lock_release(s->lock);

        return false;
    }

    lock_release(s->lock);

    return true;
}


boolean_t set_remove(set_t* s, void* value) {
    if(!s) {
        return false;
    }

    lock_acquire(s->lock);

    s->index->delete(s->index, value, NULL);

    lock_release(s->lock);

    return true;
}

boolean_t set_destroy_with_callback(set_t* s, set_destroy_callback_f cb) {
    if(!s) {
        return true;
    }

    boolean_t error = false;

    if(cb) {
        iterator_t* iter = set_create_iterator(s);

        if(iter) {
            while(iter->end_of_iterator(iter) != 0) {
                void* item = (void*)iter->get_item(iter);

                error |= !cb(item);
            }

            iter->destroy(iter);
        } else {
            error = true;
        }
    }

    bplustree_destroy_index(s->index);
    lock_destroy(s->lock);

    memory_free(s);

    return !error;
}

iterator_t* set_create_iterator(set_t* s) {
    if(!s) {
        return NULL;
    }

    return s->index->create_iterator(s->index);
}

