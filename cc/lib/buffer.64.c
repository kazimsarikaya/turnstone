/**
 * @file buffer.64.c
 * @brief buffer implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <buffer.h>
#include <cpu/sync.h>


typedef struct buffer_internal_s {
    memory_heap_t* heap;
    lock_t         lock;
    uint64_t       capacity;
    uint64_t       length;
    uint8_t*       data;
}buffer_internal_t;

buffer_t buffer_new_with_capacity(memory_heap_t* heap, uint64_t capacity) {
    buffer_internal_t* bi = memory_malloc_ext(heap, sizeof(buffer_internal_t), 0);

    bi->heap = heap;
    bi->lock = lock_create_with_heap(bi->heap);
    bi->capacity = capacity;
    bi->data = memory_malloc_ext(bi->heap, bi->capacity, 0);

    return (buffer_t)bi;
}

uint64_t buffer_get_length(buffer_t buffer) {
    if(!buffer) {
        return 0;
    }

    return ((buffer_internal_t*)buffer)->length;
};

uint64_t buffer_get_capacity(buffer_t buffer) {
    if(!buffer) {
        return 0;
    }

    return ((buffer_internal_t*)buffer)->capacity;
};

buffer_t buffer_append_byte(buffer_t buffer, uint8_t data) {
    uint8_t tmp[] = {data};
    return buffer_append_bytes(buffer, tmp, 1);
}

buffer_t buffer_append_bytes(buffer_t buffer, uint8_t* data, uint64_t length) {
    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    lock_acquire(bi->lock);

    uint64_t new_cap = bi->capacity;

    while(bi->length + length >= new_cap) {
        new_cap *= 2;
    }

    if(bi->capacity != new_cap) {
        bi->capacity = new_cap;
        uint8_t* tmp_data = memory_malloc_ext(bi->heap, bi->capacity, 0);

        if(tmp_data == NULL) {
            lock_release(bi->lock);

            return NULL;
        }

        memory_memcopy(bi->data, tmp_data, bi->length);
        memory_free(bi->data);
        bi->data = tmp_data;
    }

    memory_memcopy(data, bi->data + bi->length, length);
    bi->length += length;

    lock_release(bi->lock);

    return (buffer_t)bi;
}

buffer_t buffer_append_buffer(buffer_t buffer, buffer_t appenden) {
    buffer_internal_t* bi = (buffer_internal_t*)appenden;

    return buffer_append_bytes(buffer, bi->data, bi->length);
}

uint8_t* buffer_get_bytes(buffer_t buffer, uint64_t* length) {
    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    uint8_t* res = memory_malloc_ext(bi->heap, bi->length, 0);
    memory_memcopy(bi->data, res, bi->length);

    if(length) {
        *length = bi->length;
    }

    return res;
}

int8_t buffer_destroy(buffer_t buffer) {
    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    lock_destroy(bi->lock);
    memory_free_ext(bi->heap, bi->data);
    memory_free_ext(bi->heap, bi);

    return 0;
}
