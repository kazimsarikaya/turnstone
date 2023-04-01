/**
 * @file buffer.64.c
 * @brief buffer implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <buffer.h>
#include <cpu/sync.h>


typedef struct buffer_internal_t {
    memory_heap_t* heap;
    lock_t         lock;
    uint64_t       capacity;
    uint64_t       length;
    uint64_t       position;
    boolean_t      readonly;
    uint8_t*       data;
}buffer_internal_t;

buffer_t buffer_new_with_capacity(memory_heap_t* heap, uint64_t capacity) {
    buffer_internal_t* bi = memory_malloc_ext(heap, sizeof(buffer_internal_t), 0);

    if(bi == NULL) {
        return NULL;
    }

    bi->heap = heap;
    bi->lock = lock_create_with_heap(bi->heap);
    bi->capacity = capacity;
    bi->data = memory_malloc_ext(bi->heap, bi->capacity, 0);

    return (buffer_t)bi;
}

buffer_t buffer_encapsulate(uint8_t* data, uint64_t length) {
    buffer_internal_t* bi = memory_malloc_ext(NULL, sizeof(buffer_internal_t), 0);

    if(bi == NULL) {
        return NULL;
    }

    bi->lock = lock_create_with_heap(bi->heap);
    bi->capacity = length;
    bi->length = length;
    bi->readonly = true;
    bi->data = data;

    return (buffer_t)bi;
}

uint64_t buffer_get_length(buffer_t buffer) {
    if(!buffer) {
        return 0;
    }

    return ((buffer_internal_t*)buffer)->length;
};

boolean_t buffer_reset(buffer_t buffer) {
    if(!buffer) {
        return false;
    }

    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    lock_acquire(bi->lock);

    memory_memclean(bi->data, bi->length);

    bi->length = 0;
    bi->position = 0;

    lock_release(bi->lock);

    return true;
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

uint64_t buffer_get_position(buffer_t buffer) {
    if(!buffer) {
        return 0;
    }

    return ((buffer_internal_t*)buffer)->position;
};

buffer_t buffer_append_bytes(buffer_t buffer, uint8_t* data, uint64_t length) {
    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    if(bi->readonly) {
        return NULL;
    }

    lock_acquire(bi->lock);

    uint64_t new_cap = bi->capacity;

    while(bi->position + length > new_cap) {
        if(new_cap > (1 << 20)) {
            new_cap += 1 << 20;
        } else {
            new_cap *= 2;
        }
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

    memory_memcopy(data, bi->data + bi->position, length);

    bi->position += length;

    if(bi->length < bi->position) {
        bi->length = bi->position;
    }

    lock_release(bi->lock);

    return (buffer_t)bi;
}

buffer_t buffer_append_buffer(buffer_t buffer, buffer_t appenden) {
    buffer_internal_t* bi = (buffer_internal_t*)appenden;

    return buffer_append_bytes(buffer, bi->data, bi->length);
}

uint8_t* buffer_get_bytes(buffer_t buffer, uint64_t length) {
    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    if(length == 0) {
        return NULL;
    }

    lock_acquire(bi->lock);

    if(bi->position >= bi->length) {
        lock_release(bi->lock);

        return NULL;
    }

    uint8_t* res = memory_malloc_ext(bi->heap, length, 0);
    memory_memcopy(bi->data + bi->position, res, length);

    bi->position += length;

    lock_release(bi->lock);

    return res;
}

uint8_t* buffer_get_all_bytes(buffer_t buffer, uint64_t* length) {
    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    uint8_t* res = memory_malloc_ext(bi->heap, bi->length, 0);
    memory_memcopy(bi->data, res, bi->length);

    if(length) {
        *length = bi->length;
    }

    return res;
}


uint8_t  buffer_get_byte(buffer_t buffer) {
    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    lock_acquire(bi->lock);

    if(bi->position >= bi->length) {
        lock_release(bi->lock);

        return NULL;
    }
    uint8_t res = bi->data[bi->position];
    bi->position++;

    lock_release(bi->lock);

    return res;
}

uint8_t  buffer_peek_byte(buffer_t buffer) {
    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    lock_acquire(bi->lock);

    if(bi->position >= bi->length) {
        lock_release(bi->lock);

        return NULL;
    }
    uint8_t res = bi->data[bi->position];

    lock_release(bi->lock);

    return res;
}

uint8_t buffer_peek_buffer_at_position(buffer_t buffer, uint64_t position) {
    if(!buffer) {
        return 0;
    }

    return ((buffer_internal_t*)buffer)->data[position];
}


uint64_t buffer_remaining(buffer_t buffer) {
    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    lock_acquire(bi->lock);

    uint64_t res = bi->length - bi->position;

    lock_release(bi->lock);

    return res;
}


boolean_t   buffer_seek(buffer_t buffer, int64_t position, buffer_seek_direction_t direction) {
    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    lock_acquire(bi->lock);

    if(direction == BUFFER_SEEK_DIRECTION_START) {
        if(position < 0 || (uint64_t)position >= bi->capacity) {
            lock_release(bi->lock);

            return false;
        }

        bi->position = 0;
    }

    if(direction == BUFFER_SEEK_DIRECTION_END) {
        if (position > 0 || position + ((int64_t)bi->capacity) < 0) {
            lock_release(bi->lock);

            return false;
        }

        bi->position = bi->capacity;
    }

    if(direction == BUFFER_SEEK_DIRECTION_CURRENT && (bi->position + position > bi->capacity || ((int64_t)bi->position) + position < 0)) {
        lock_release(bi->lock);

        return false;
    }

    bi->position += position;

    if(bi->position > bi->length) {
        bi->length = bi->position;
    }

    lock_release(bi->lock);

    return true;
}

int8_t buffer_destroy(buffer_t buffer) {
    buffer_internal_t* bi = (buffer_internal_t*)buffer;

    lock_destroy(bi->lock);

    if(!bi->readonly) {
        memory_free_ext(bi->heap, bi->data);
    }

    memory_free_ext(bi->heap, bi);

    return 0;
}
