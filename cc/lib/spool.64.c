/**
 * @file spool.64.c
 * @brief Spool implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <spool.h>
#include <strings.h>
#include <logging.h>
#include <memory.h>

MODULE("turnstone.lib.spool");

struct spool_item_t {
    char_t* name;
    list_t* buffers;
};


list_t* spool_list = NULL;
memory_heap_t* spool_heap = NULL;

int8_t spool_init(size_t spool_size, uint64_t spool_start) {
    spool_heap = memory_create_heap_hash(spool_start, spool_start + spool_size);

    if(spool_heap == NULL) {
        return -1;
    }

    spool_list = list_create_queue_with_heap(spool_heap);

    if(spool_list == NULL) {
        return -1;
    }

    return 0;
}

memory_heap_t* spool_get_heap(void) {
    return spool_heap;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t spool_add(const char_t* name, size_t buf_cnt, ...) {

    spool_item_t* item = memory_malloc_ext(spool_heap, sizeof(spool_item_t), 0);

    if(item == NULL) {
        return -1;
    }

    item->name = strdup_at_heap(spool_heap, name);

    if(item->name == NULL) {
        memory_free_ext(spool_heap, item);
        return -1;
    }

    item->buffers = list_create_queue();

    if(item->buffers == NULL) {
        memory_free_ext(spool_heap, item->name);
        memory_free_ext(spool_heap, item);
        return -1;
    }

    va_list args;
    va_start(args, buf_cnt);

    for(size_t i = 0; i < buf_cnt; i++) {
        buffer_t* buf = va_arg(args, buffer_t*);

        if(buf == NULL) {
            memory_free_ext(spool_heap, item->name);
            list_destroy(item->buffers);
            memory_free_ext(spool_heap, item);
            va_end(args);
            return -1;
        }

        list_queue_push(item->buffers, buf);
    }

    va_end(args);

    list_queue_push(spool_list, item);

    return 0;
}
#pragma GCC diagnostic pop

list_t* spool_get_all(void) {
    return spool_list;
}

const char_t* spool_get_name(const spool_item_t* item) {
    return item->name;
}

size_t spool_get_buffer_count(const spool_item_t* item) {
    return list_size(item->buffers);
}

size_t spool_get_total_buffer_size(const spool_item_t* item) {
    size_t total_size = 0;

    for(size_t i = 0; i < list_size(item->buffers); i++) {
        const buffer_t* buf = list_get_data_at_position(item->buffers, i);

        total_size += buffer_get_length(buf);
    }

    return total_size;
}

const buffer_t* spool_get_buffer(const spool_item_t* item, size_t buf_idx) {
    return list_get_data_at_position(item->buffers, buf_idx);
}
