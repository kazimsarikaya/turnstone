/**
 * @file pipeline.64.c
 * @brief Pipeline stream
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <pipeline.h>
#include <cpu/sync.h>

MODULE("turnstone.lib.pipeline");


typedef struct pipeline_t {
    memory_heap_t* heap;
    uint64_t       capacity;
    uint64_t       read_index;
    uint64_t       write_index;
    lock_t*        lock;
    semaphore_t*   read_semaphore;
    semaphore_t*   write_semaphore;
    uint8_t*       buffer;
} pipeline_t;

pipeline_t* pipeline_create_with_heap(memory_heap_t* heap, uint64_t capacity) {
    heap = memory_get_heap(heap);

    if(capacity == 0) {
        return NULL;
    }

    pipeline_t* pipeline = memory_malloc_ext(heap, sizeof(pipeline_t), 0x0);

    if(!pipeline) {
        return NULL;
    }

    pipeline->heap = heap;
    pipeline->capacity = capacity;
    pipeline->read_index = 0;
    pipeline->write_index = 0;
    pipeline->buffer = memory_malloc_ext(heap, capacity, 0x80);
    if(!pipeline->buffer) {
        memory_free_ext(heap, pipeline);
        return NULL;
    }

    pipeline->lock = lock_create_with_heap(heap);
    if(!pipeline->lock) {
        memory_free_ext(heap, pipeline->buffer);
        memory_free_ext(heap, pipeline);
        return NULL;
    }

    pipeline->read_semaphore = semaphore_create_with_heap_and_check_initial_count(heap, 0, false);
    if(!pipeline->read_semaphore) {
        lock_destroy(pipeline->lock);
        memory_free_ext(heap, pipeline->buffer);
        memory_free_ext(heap, pipeline);
        return NULL;
    }

    pipeline->write_semaphore = semaphore_create_with_heap_and_check_initial_count(heap, capacity, false);
    if(!pipeline->write_semaphore) {
        semaphore_destroy(pipeline->read_semaphore);
        lock_destroy(pipeline->lock);
        memory_free_ext(heap, pipeline->buffer);
        memory_free_ext(heap, pipeline);
        return NULL;
    }

    return pipeline;
}

int8_t pipeline_clear(pipeline_t* pipeline) {
    if(!pipeline) {
        return -1;
    }

    lock_acquire(pipeline->lock);

    pipeline->read_index = 0;
    pipeline->write_index = 0;
    memory_memclean(pipeline->buffer, pipeline->capacity);

    semaphore_reset(pipeline->read_semaphore);
    semaphore_reset(pipeline->write_semaphore);

    lock_release(pipeline->lock);

    return 0;
}

int8_t pipeline_destroy(pipeline_t* pipeline){
    if(!pipeline) {
        return -1;
    }

    if(pipeline->buffer) {
        memory_free_ext(pipeline->heap, pipeline->buffer);
    }

    lock_destroy(pipeline->lock);

    semaphore_destroy(pipeline->read_semaphore);
    semaphore_destroy(pipeline->write_semaphore);

    memory_free_ext(pipeline->heap, pipeline);

    return 0;
}

uint64_t pipeline_write(pipeline_t* pipeline, uint64_t size, const uint8_t* data) {
    if(!pipeline || !data || size == 0) {
        return -1;
    }

    lock_acquire(pipeline->lock);

    uint64_t available_space = pipeline_available_space(pipeline);

    if(size > available_space) {
        size = available_space;
    }

    if(size == 0) {
        lock_release(pipeline->lock);
        return 0;
    }

    semaphore_acquire_with_count(pipeline->write_semaphore, size);

    uint64_t first_chunk_size = pipeline->capacity - pipeline->write_index;
    if(first_chunk_size > size) {
        first_chunk_size = size;
    }

    memory_memcopy(data, &pipeline->buffer[pipeline->write_index], first_chunk_size);

    if(size > first_chunk_size) {
        memory_memcopy(&data[first_chunk_size], &pipeline->buffer[0], size - first_chunk_size);
    }

    pipeline->write_index = (pipeline->write_index + size) % pipeline->capacity;

    semaphore_release_with_count(pipeline->read_semaphore, size);

    lock_release(pipeline->lock);

    return size;
}

uint64_t pipeline_write_blocked(pipeline_t* pipeline, uint64_t size, const uint8_t* data) {
    if(!pipeline || !data || size == 0) {
        return -1;
    }

    lock_acquire(pipeline->lock);

    semaphore_acquire_with_count(pipeline->write_semaphore, size);

    uint64_t first_chunk_size = pipeline->capacity - pipeline->write_index;
    if(first_chunk_size > size) {
        first_chunk_size = size;
    }

    memory_memcopy(data, &pipeline->buffer[pipeline->write_index], first_chunk_size);

    if(size > first_chunk_size) {
        memory_memcopy(&data[first_chunk_size], &pipeline->buffer[0], size - first_chunk_size);
    }

    pipeline->write_index = (pipeline->write_index + size) % pipeline->capacity;

    semaphore_release_with_count(pipeline->read_semaphore, size);

    lock_release(pipeline->lock);

    return size;
}

uint64_t pipeline_read(pipeline_t* pipeline, uint64_t size, uint8_t* data) {
    if(!pipeline || !data || size == 0) {
        return -1;
    }

    lock_acquire(pipeline->lock);

    uint64_t available_data = pipeline_available_data(pipeline);

    if(size > available_data) {
        size = available_data;
    }

    if(size == 0) {
        lock_release(pipeline->lock);
        return 0;
    }

    semaphore_acquire_with_count(pipeline->read_semaphore, size);

    uint64_t first_chunk_size = pipeline->capacity - pipeline->read_index;

    if(first_chunk_size > size) {
        first_chunk_size = size;
    }

    memory_memcopy(&pipeline->buffer[pipeline->read_index], data, first_chunk_size);

    if(size > first_chunk_size) {
        memory_memcopy(&pipeline->buffer[0], &data[first_chunk_size], size - first_chunk_size);
    }

    pipeline->read_index = (pipeline->read_index + size) % pipeline->capacity;

    semaphore_release_with_count(pipeline->write_semaphore, size);

    lock_release(pipeline->lock);

    return size;
}

uint64_t pipeline_read_blocked(pipeline_t* pipeline, uint64_t size, uint8_t* data) {
    if(!pipeline || !data || size == 0) {
        return -1;
    }

    lock_acquire(pipeline->lock);

    semaphore_acquire_with_count(pipeline->read_semaphore, size);

    uint64_t first_chunk_size = pipeline->capacity - pipeline->read_index;

    if(first_chunk_size > size) {
        first_chunk_size = size;
    }

    memory_memcopy(&pipeline->buffer[pipeline->read_index], data, first_chunk_size);

    if(size > first_chunk_size) {
        memory_memcopy(&pipeline->buffer[0], &data[first_chunk_size], size - first_chunk_size);
    }

    pipeline->read_index = (pipeline->read_index + size) % pipeline->capacity;

    semaphore_release_with_count(pipeline->write_semaphore, size);

    lock_release(pipeline->lock);

    return size;
}

uint64_t pipeline_available_data(pipeline_t* pipeline) {
    if(!pipeline) {
        return 0;
    }

    lock_acquire(pipeline->lock);

    uint64_t w = pipeline->write_index % pipeline->capacity;
    uint64_t r = pipeline->read_index % pipeline->capacity;

    uint64_t available_data = 0;

    if (w >= r) {
        available_data = w - r;
    } else {
        available_data = pipeline->capacity - (r - w);
    }

    lock_release(pipeline->lock);

    return available_data;
}

uint64_t pipeline_available_space(pipeline_t* pipeline) {
    if (!pipeline) {
        return 0;
    }

    lock_acquire(pipeline->lock);

    uint64_t w = pipeline->write_index % pipeline->capacity;
    uint64_t r = pipeline->read_index % pipeline->capacity;

    uint64_t available_space;

    if (w >= r) {
        available_space = pipeline->capacity - (w - r);
    } else {
        available_space = r - w;
    }

    lock_release(pipeline->lock);

    return available_space;
}

uint64_t pipeline_capacity(pipeline_t* pipeline) {
    if(!pipeline) {
        return 0;
    }

    return pipeline->capacity;

}
