/**
 * @file future.64.c
 * @brief Future implementation for 64-bit systems.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <future.h>

MODULE("turnstone.lib");

typedef struct future_t {
    memory_heap_t* heap;
    lock_t*        lock;
    void*          data;
} future_t;

future_t* future_create_with_heap_and_data(memory_heap_t* heap, lock_t* lock, void* data) {
    future_t* fi = memory_malloc_ext(heap, sizeof(future_t), 0);

    if(fi == NULL) {
        return NULL;
    }

    fi->heap = heap;
    fi->lock = lock;
    fi->data = data;

    return fi;
}

void* future_get_data_and_destroy(future_t* future) {
    future_t* fi = (future_t*)future;

    if(fi == NULL) {
        return NULL;
    }

    lock_acquire(fi->lock);

    void* data = fi->data;

    lock_destroy(fi->lock);

    memory_free_ext(fi->heap, fi);

    return data;
}
