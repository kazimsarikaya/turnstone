/**
 * @file memory_heap_hash.xx.c
 * @brief Turnstone OS memory heap with hash table implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <memory.h>
#include <systeminfo.h>
#include <cpu.h>
#include <logging.h>
#include <cpu/sync.h>
#include <linker.h>
#include <utils.h>
#if ___TESTMODE == 1
#include <valgrind.h>
#include <memcheck.h>
#endif
#if ___KERNELBUILD == 1
#include <backtrace.h>
#endif

MODULE("turnstone.lib.memory");


typedef struct memory_heap_hash_block_t {
    uint32_t  address;
    uint32_t  size;
    uint32_t  next;
    boolean_t is_free;
    uint8_t   reserved[3];
}__attribute__((packed)) memory_heap_hash_block_t;

_Static_assert(sizeof(memory_heap_hash_block_t) == 16, "memory_heap_hash_block_t size is not 16 bytes");

#define MEMORY_HEAP_HASH_FAST_CLASSES_COUNT 1025
#define MEMORY_HEAP_HASH_MAX_POOLS            16

typedef struct memory_heap_hash_fast_class_t {
    uint32_t head;
    uint32_t tail;
}__attribute__((packed)) memory_heap_hash_fast_class_t;

typedef struct memory_heap_hash_pool_t {
    uint64_t                      pool_base;
    uint32_t                      pool_size;
    uint32_t                      last_address;
    memory_heap_hash_fast_class_t fast_classes[MEMORY_HEAP_HASH_FAST_CLASSES_COUNT];
    uint32_t                      free_list;
    uint16_t                      pool_id;
    uint16_t                      segment_count;
    uint32_t                      segment_start;
    uint32_t                      segment_end;
}__attribute__((packed)) memory_heap_hash_pool_t;

typedef struct memory_heap_hash_metadata_t {
    uint64_t pools[MEMORY_HEAP_HASH_MAX_POOLS];
    uint16_t pool_count;
    uint16_t padding;
    uint32_t segment_size;
    uint32_t segment_block_count;
    uint32_t segment_hash_mask;
    uint64_t malloc_count;
    uint64_t free_count;
    uint64_t total_size;
    uint64_t free_size;
    uint64_t fast_hit;
    uint64_t header_count;
}__attribute__((packed)) memory_heap_hash_metadata_t;

static inline memory_heap_hash_pool_t* memory_heap_hash_pool_get(memory_heap_hash_metadata_t* metadata, uint16_t pool_id) {
    if(!metadata) {
        return NULL;
    }

    if(pool_id >= metadata->pool_count) {
        return NULL;
    }

    return (memory_heap_hash_pool_t*)metadata->pools[pool_id];
}


static inline memory_heap_hash_pool_t* memory_heap_hash_find_pool_by_address(memory_heap_hash_metadata_t * metadata, uint64_t address) {
    for(uint16_t i = 0; i < metadata->pool_count; i++) {
        memory_heap_hash_pool_t* pool = memory_heap_hash_pool_get(metadata, i);

        if(!pool) {
            continue;
        }

        uint32_t relative_address = address - pool->pool_base;

        if(pool->last_address <= relative_address && relative_address < pool->pool_base + pool->pool_size) {
            return pool;
        }
    }

    return NULL;
}

static inline memory_heap_hash_block_t* memory_heap_hash_pool_get_hash_block(memory_heap_hash_pool_t* pool, uint32_t block_address) {
    if(!pool) {
        return NULL;
    }
/*
    if(block_address < pool->segment_start || block_address >= pool->segment_end) {
        PRINTLOG(HEAP_HASH, LOG_ERROR, "block_address 0x%x is out of pool segment range 0x%x - 0x%x", block_address, pool->segment_start, pool->segment_end);
        return NULL;
    }
 */
    return (memory_heap_hash_block_t*)(pool->pool_base + block_address);
}

static inline void* memory_heap_hash_try_alloc_from_fast_class(memory_heap_hash_metadata_t* metadata, uint32_t size) {
    uint32_t fast_class = size / 16;

    if(fast_class >= MEMORY_HEAP_HASH_FAST_CLASSES_COUNT) {
        return NULL;
    }

    for(uint16_t pool_id = 0; pool_id < metadata->pool_count; pool_id++) {
        memory_heap_hash_pool_t* pool = memory_heap_hash_pool_get(metadata, pool_id);

        if(pool->fast_classes[fast_class].head != 0) {
            memory_heap_hash_block_t* hash_block = memory_heap_hash_pool_get_hash_block(pool, pool->fast_classes[fast_class].head);

            if(!hash_block) {
                continue;
            }

            pool->fast_classes[fast_class].head = hash_block->next;

            if(pool->fast_classes[fast_class].head == 0) {
                pool->fast_classes[fast_class].tail = 0;
            }

            hash_block->next = 0;

            metadata->fast_hit++;
            metadata->malloc_count++;
            metadata->free_size -= hash_block->size;

            hash_block->is_free = false;

            PRINTLOG(HEAP_HASH, LOG_DEBUG, "fast malloced size 0x%x address 0x%llx", hash_block->size, pool->pool_base + hash_block->address);

#if ___TESTMODE == 1
            VALGRIND_MALLOCLIKE_BLOCK(pool->pool_base + hash_block->address, hash_block->size, 1, 1);
#endif
            return (void*)(pool->pool_base + hash_block->address);
        }

    }

    return NULL;
}

static inline void* memory_heap_hash_try_alloc_from_free_list(memory_heap_hash_metadata_t* metadata, uint64_t alignment, uint32_t size) {
    for(uint16_t pool_id = 0; pool_id < metadata->pool_count; pool_id++) {
        memory_heap_hash_pool_t* pool = memory_heap_hash_pool_get(metadata, pool_id);

        if(!pool) {
            continue;
        }

        uint32_t free_list = pool->free_list;
        memory_heap_hash_block_t* prev_free_list_node = NULL;

        // PRINTLOG(HEAP_HASH, LOG_TRACE, "try to allocate from free list");

        while(free_list) {
            // PRINTLOG(HEAP_HASH, LOG_TRACE, "try to allocate from free list 0x%x", free_list);
            memory_heap_hash_block_t* free_node = memory_heap_hash_pool_get_hash_block(pool, free_list);

            /*if(!free_node) {
                PRINTLOG(HEAP_HASH, LOG_ERROR, "free node 0x%x is out of pool segment range 0x%x - 0x%x", free_list, pool->segment_start, pool->segment_end);
                continue;
               }*/

            // PRINTLOG(HEAP_HASH, LOG_TRACE, "found free node 0x%x address 0x%x size 0x%x", free_list, free_node->address, free_node->size);

            if(free_node->size == size && !(free_node->address % alignment)) {
                // PRINTLOG(HEAP_HASH, LOG_TRACE, "found free node 0x%x address 0x%x size 0x%x", free_list, free_node->address, free_node->size);

                uint32_t next_free_list = free_node->next;

                free_node->next = 0;

                if(prev_free_list_node) {
                    prev_free_list_node->next = next_free_list;
                } else {
                    pool->free_list = next_free_list;
                }

                metadata->malloc_count++;

                metadata->free_size -= free_node->size;

                free_node->is_free = false;

                PRINTLOG(HEAP_HASH, LOG_DEBUG, "malloced from free list. size 0x%x alignment 0x%llx address 0x%llx", free_node->size, alignment, pool->pool_base + free_node->address);

#if ___TESTMODE == 1
                VALGRIND_MALLOCLIKE_BLOCK(pool->pool_base + free_node->address, free_node->size, 1, 1);
#endif
                return (void*)(pool->pool_base + free_node->address);
            }

            prev_free_list_node = free_node;
            free_list = free_node->next;
        }

    }

    return NULL;
}

static inline memory_heap_hash_block_t* memory_heap_hash_pool_new_hash_block(memory_heap_hash_metadata_t* metadata, memory_heap_hash_pool_t* pool, uint32_t address, uint32_t size) {
    uint32_t hash = (address >> 4) & metadata->segment_hash_mask;

    uint8_t* segment_offset = (uint8_t*)(pool->pool_base + pool->segment_start);

    memory_heap_hash_block_t* blocks = (memory_heap_hash_block_t*)segment_offset;

    uint32_t segment_idx = 0;

    while(segment_idx < pool->segment_count) {
        if(blocks[hash].address == 0) {
            blocks[hash].address = address;
            blocks[hash].size = size;

            metadata->header_count++;

            return &blocks[hash];
        }

        segment_idx++;

        segment_offset += metadata->segment_size;
        blocks = (memory_heap_hash_block_t*)segment_offset;
    }

    if(pool->last_address > pool->segment_end + metadata->segment_size) {
        pool->segment_end += metadata->segment_size;
        pool->segment_count++;

        blocks[hash].address = address;
        blocks[hash].size = size;

        metadata->header_count++;

        metadata->free_size -= metadata->segment_size;

        return &blocks[hash];
    }

    return NULL;
}

static inline memory_heap_hash_block_t* memory_heap_hash_pool_search_hash_block(memory_heap_hash_metadata_t* metadata, memory_heap_hash_pool_t* pool, uint32_t address) {
    uint32_t hash = (address >> 4) & metadata->segment_hash_mask;

    uint8_t* segment_offset = (uint8_t*)(pool->pool_base + pool->segment_start);

    memory_heap_hash_block_t* blocks = (memory_heap_hash_block_t*)segment_offset;

    uint32_t segment_idx = 0;

    while(segment_idx < pool->segment_count) {
        if(blocks[hash].address == address) {
            return &blocks[hash];
        }

        segment_idx++;

        segment_offset += metadata->segment_size;
        blocks = (memory_heap_hash_block_t*)segment_offset;
    }

    return NULL;
}

void*  memory_heap_hash_malloc_ext(memory_heap_t* heap, uint64_t size, uint64_t alignment);
int8_t memory_heap_hash_free(memory_heap_t* heap, void* ptr);
void   memory_heap_hash_stat(memory_heap_t* heap, memory_heap_stat_t* stat);


static inline void memory_heap_hash_pool_insert_sorted_at_free_list(memory_heap_hash_pool_t* pool, memory_heap_hash_block_t* hash_block) {
    uint32_t hash_block_address = (uint32_t)((uint64_t)hash_block - pool->pool_base);

    if(!pool->free_list) {
        pool->free_list = hash_block_address;

        return;
    }

    // PRINTLOG(HEAP_HASH, LOG_TRACE, "free list start %x hash_block_address %x", pool->free_list, hash_block_address);

    memory_heap_hash_block_t* free_list_node = memory_heap_hash_pool_get_hash_block(pool, pool->free_list);


    /*if(!free_list_node) {
        pool->free_list = hash_block_address;

        return;
       }*/

    memory_heap_hash_block_t* prev_free_list_node = NULL;

    while(free_list_node) {
        if(hash_block->size <= free_list_node->size) {
            break;
        }

        prev_free_list_node = free_list_node;
        // PRINTLOG(HEAP_HASH, LOG_TRACE, "next free list node %x", free_list_node->next);

        if(!free_list_node->next) {
            free_list_node = NULL;

            break;
        }

        free_list_node = memory_heap_hash_pool_get_hash_block(pool, free_list_node->next);
    }

    hash_block->next = free_list_node ? (uint32_t)((uint64_t)free_list_node - pool->pool_base) : 0;
    // PRINTLOG(HEAP_HASH, LOG_TRACE, "hash block next %x", hash_block->next);

    if(prev_free_list_node) {
        prev_free_list_node->next = hash_block_address;
    } else {
        pool->free_list = hash_block_address;
    }

    // PRINTLOG(HEAP_HASH, LOG_TRACE, "inserted hash block 0x%x at free list 0x%x", hash_block_address, pool->free_list);
}

void* memory_heap_hash_malloc_ext(memory_heap_t* heap, uint64_t size, uint64_t alignment) {
    if(!heap) {
        return NULL;
    }

    if(size == 0) {
        return NULL;
    }

    PRINTLOG(HEAP_HASH, LOG_DEBUG, "malloc size 0x%llx alignment 0x%llx", size, alignment);

    memory_heap_hash_metadata_t* metadata = heap->metadata;

    size += 16 - (size % 16) - 1;

    if(alignment < 16) {
        alignment = 16;
    }

    // PRINTLOG(HEAP_HASH, LOG_TRACE, "fixed malloc size 0x%llx alignment 0x%llx", size, alignment);

    // look at fast classes if alignment is 16
    if(alignment == 16) {
        void* fast_class_ptr = memory_heap_hash_try_alloc_from_fast_class(metadata, size);

        if(fast_class_ptr) {
            return fast_class_ptr;
        }
    }

    // look at free list
    void* free_list_ptr = memory_heap_hash_try_alloc_from_free_list(metadata, alignment, size);

    if(free_list_ptr) {
        return free_list_ptr;
    }

    // look at end of heap

    for(uint16_t pool_id = 0; pool_id < metadata->pool_count; pool_id++) {
        memory_heap_hash_pool_t* pool = memory_heap_hash_pool_get(metadata, pool_id);

        if(pool->last_address < size) {
            // PRINTLOG(HEAP_HASH, LOG_TRACE, "pool %d last address 0x%x is less than size 0x%llx", pool_id, pool->last_address, size);

            continue;
        }

        uint32_t last_address = pool->last_address - size;

        if(last_address <= pool->segment_end) {
            // PRINTLOG(HEAP_HASH, LOG_TRACE, "pool %d last address 0x%x is less than segment end 0x%x", pool_id, last_address, pool->segment_end);

            continue;
        }

        if(alignment > 16) {
            uint32_t back = last_address % alignment + pool->pool_base % alignment;

            if(back > last_address) {
                // PRINTLOG(HEAP_HASH, LOG_TRACE, "pool %d last address 0x%x is less than alignment 0x%llx", pool_id, last_address, alignment);

                continue;
            }

            last_address -= last_address % alignment + pool->pool_base % alignment;
        }

        if(last_address <= pool->segment_end) {
            // PRINTLOG(HEAP_HASH, LOG_TRACE, "pool %d last address 0x%x is less than segment end 0x%x", pool_id, last_address, pool->segment_end);

            continue;
        }

        // PRINTLOG(HEAP_HASH, LOG_TRACE, "try to allocate from end of heap with address 0x%x", last_address);

        uint32_t remaining_size = pool->last_address - (last_address + size);

        if(remaining_size) {
            uint32_t remaining_address = last_address + size;

            // PRINTLOG(HEAP_HASH, LOG_TRACE, "remaining found. splitting address 0x%x remaining 0x%x", remaining_address, remaining_size);

            uint64_t real_remaining_address = pool->pool_base + remaining_address;

            uint8_t* flag = (uint8_t*)real_remaining_address;

            remaining_size -= 1;
            remaining_address += 1;

            memory_heap_hash_block_t* remaining_node = memory_heap_hash_pool_new_hash_block(metadata, pool, remaining_address, remaining_size);

            if(!remaining_node) {
                continue;
            }

            uint32_t block_address = (uint32_t)((uint64_t)remaining_node - pool->pool_base);

            *flag = 0x55;

#if ___TESTMODE == 1
            VALGRIND_MAKE_MEM_NOACCESS(flag, 1);
#endif

            uint32_t fast_class = remaining_size >> 4;

            if(fast_class < MEMORY_HEAP_HASH_FAST_CLASSES_COUNT) {
                if(pool->fast_classes[fast_class].head == 0) {
                    pool->fast_classes[fast_class].head = block_address;
                    pool->fast_classes[fast_class].tail = block_address;
                } else {
                    memory_heap_hash_block_t* fast_class_node = memory_heap_hash_pool_get_hash_block(pool, pool->fast_classes[fast_class].tail);

                    if(fast_class_node) {
                        fast_class_node->next = block_address;
                        pool->fast_classes[fast_class].tail = block_address;
                    }

                }
            } else {
                memory_heap_hash_pool_insert_sorted_at_free_list(pool, remaining_node);
            }
        }


        uint8_t* flag = (uint8_t*)(pool->pool_base + last_address - 1);

        if(!memory_heap_hash_pool_new_hash_block(metadata, pool, last_address, size)) {
            continue;
        }

        pool->last_address = last_address - 1;

        metadata->malloc_count++;
        metadata->free_size -= size + 1;

        *flag = 0x55;

        PRINTLOG(HEAP_HASH, LOG_DEBUG, "malloced from end of heap. size 0x%llx alignment 0x%llx address 0x%llx", size, alignment, pool->pool_base + last_address);

#if ___TESTMODE == 1
        VALGRIND_MALLOCLIKE_BLOCK(flag + 1, size, 1, 1);
        VALGRIND_MAKE_MEM_NOACCESS(flag, 1);
#endif

        return (void*)(pool->pool_base + last_address);

    }


    PRINTLOG(HEAP_HASH, LOG_ERROR, "out of memory");
    memory_heap_stat_t stat;
    memory_heap_hash_stat(heap, &stat);

    PRINTLOG(HEAP_HASH, LOG_ERROR, "memory stat ts 0x%llx fs 0x%llx mc 0x%llx fc 0x%llx diff 0x%llx", stat.total_size, stat.free_size, stat.malloc_count, stat.free_count, stat.malloc_count - stat.free_count);

    return NULL;
}


int8_t memory_heap_hash_free(memory_heap_t* heap, void* ptr) {
    if(!heap || !ptr) {

        return -1;
    }

    memory_heap_hash_metadata_t* metadata = heap->metadata;

    uint64_t req_address = (uint64_t)ptr;

    memory_heap_hash_pool_t* pool = memory_heap_hash_find_pool_by_address(metadata, req_address);

    if(!pool) {
        PRINTLOG(HEAP_HASH, LOG_WARNING, "address %p out of heap range. heap task 0x%llx", ptr, heap->task_id);

#if ___KERNELBUILD == 1
        backtrace();
#endif

        return -1;
    }

    uint32_t rel_address = (uint32_t)(req_address - pool->pool_base);

    PRINTLOG(HEAP_HASH, LOG_DEBUG, "freeing address 0x%x(0x%llx)", rel_address, req_address);

    memory_heap_hash_block_t* hash_block = memory_heap_hash_pool_search_hash_block(metadata, pool, rel_address);

    if(!hash_block) {
        PRINTLOG(HEAP_HASH, LOG_WARNING, "address %p not found. heap task 0x%llx", ptr, heap->task_id);

#if ___KERNELBUILD == 1
        backtrace();
#endif

        return -1;
    }

    if(hash_block->is_free) {
        PRINTLOG(HEAP_HASH, LOG_WARNING, "address %p is already freed. heap task 0x%llx", ptr, heap->task_id);

#if ___KERNELBUILD == 1
        backtrace();
#endif

        return -1;
    }

    // PRINTLOG(HEAP_HASH, LOG_TRACE, "found block with address 0x%x(0x%llx) size 0x%x", hash_block->address, pool->pool_base + hash_block->address, hash_block->size);

    uint64_t real_address = req_address;
    uint32_t real_size = hash_block->size;

    // PRINTLOG(HEAP_HASH, LOG_TRACE, "real address 0x%llx size 0x%x end 0x%llx", real_address, real_size, real_address + real_size);


    memory_memclean((void*)real_address, real_size);

    uint32_t fast_class = real_size / 16;

    hash_block->is_free = true;

    if(fast_class < MEMORY_HEAP_HASH_FAST_CLASSES_COUNT) {
        uint32_t rel_block_address = (uint32_t)((uint64_t)hash_block - pool->pool_base);

        if(pool->fast_classes[fast_class].head == 0) {
            pool->fast_classes[fast_class].head = rel_block_address;
            pool->fast_classes[fast_class].tail = rel_block_address;
        } else {
            memory_heap_hash_block_t* fast_class_node = memory_heap_hash_pool_get_hash_block(pool, pool->fast_classes[fast_class].tail);

            if(fast_class_node) {
                fast_class_node->next = rel_block_address;
                pool->fast_classes[fast_class].tail = rel_block_address;
            }
        }

        // PRINTLOG(HEAP_HASH, LOG_TRACE, "0x%x inserted into to fast class %d", rel_address, fast_class);
    } else {
        memory_heap_hash_pool_insert_sorted_at_free_list(pool, hash_block);

        // PRINTLOG(HEAP_HASH, LOG_TRACE, "0x%x inserted into to free list", rel_address);
    }

    metadata->free_size += real_size;
    metadata->free_count++;


    PRINTLOG(HEAP_HASH, LOG_DEBUG, "freed %p", (void*)req_address);

#if ___TESTMODE == 1
    VALGRIND_FREELIKE_BLOCK(real_address, 1);
#endif

    return 0;
}

void memory_heap_hash_stat(memory_heap_t* heap, memory_heap_stat_t* stat) {
    if(!heap || !stat) {
        return;
    }

    memory_heap_hash_metadata_t* metadata = (memory_heap_hash_metadata_t*)heap->metadata;

    stat->malloc_count = metadata->malloc_count;
    stat->free_count = metadata->free_count;
    stat->total_size = metadata->total_size;
    stat->free_size = metadata->free_size;
    stat->fast_hit = metadata->fast_hit;
    stat->header_count = metadata->header_count;
}

memory_heap_t* memory_create_heap_hash(uint64_t start, uint64_t end) {
    size_t heap_start = 0, heap_end = 0;

    if(start == 0 || end == 0) {
        program_header_t* kernel = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;

        heap_start = kernel->program_heap_virtual_address;
        heap_end = heap_start + kernel->program_heap_size;
    } else {
        heap_start = start;
        heap_end = end;
    }

    PRINTLOG(HEAP_HASH, LOG_DEBUG, "heap boundaries 0x%llx 0x%llx", heap_start, heap_end);

    uint64_t heap_size = heap_end - heap_start;

    memory_memclean((void*)heap_start, heap_size);

    memory_heap_t* heap = (memory_heap_t*)heap_start;
    heap->header = 0xaa55aa55;

    uint64_t lock_start = heap_start + sizeof(memory_heap_t);

    if(lock_start % 0x20) {
        lock_start += 0x20 - (lock_start % 0x20);
    }

    memory_heap_t** lock_heap = (memory_heap_t**)lock_start;
    *lock_heap = heap;
    heap->lock = (lock_t)lock_heap;

    uint64_t metadata_start = lock_start + SYNC_LOCK_SIZE;

    if(metadata_start % 0x20) {
        metadata_start += 0x20 - (metadata_start % 0x20);
    }

    memory_heap_hash_metadata_t* metadata = (memory_heap_hash_metadata_t*)metadata_start;
    metadata->pool_count = 1;

    uint64_t pool_start = metadata_start + sizeof(memory_heap_hash_metadata_t);

    if(pool_start % 0x20) {
        pool_start += 0x20 - (pool_start % 0x20);
    }

    metadata->pools[0] = pool_start;

    memory_heap_hash_pool_t* pool = (memory_heap_hash_pool_t*)pool_start;
    pool->pool_base = pool_start;
    pool->pool_size = heap_size - (pool_start - heap_start);
    pool->last_address = pool->pool_size - 1;

    metadata->segment_size = heap_size / 1024;

    int8_t msb = bit_most_significant(metadata->segment_size - 1);
    metadata->segment_size = 1 << (msb + 1);
    metadata->segment_block_count = metadata->segment_size / sizeof(memory_heap_hash_block_t);

    if(metadata->segment_block_count < 256) {
        metadata->segment_size = 0x1000;
        metadata->segment_block_count = 256;
    }

    PRINTLOG(HEAP_HASH, LOG_DEBUG, "segment size %i segment block count %i", metadata->segment_size, metadata->segment_block_count);

    metadata->segment_hash_mask = metadata->segment_block_count - 1;

    uint32_t segment_start = (pool_start - heap_start) + sizeof(memory_heap_hash_pool_t);

    if(segment_start % 0x1000) {
        segment_start += 0x1000 - (segment_start % 0x1000);
    }

    pool->segment_start = segment_start - (pool_start - heap_start);
    pool->segment_end = segment_start;

    metadata->total_size = heap_size;


    uint8_t* heap_data = (uint8_t*)pool_start;
    heap_data[pool->last_address] = 0x55;

    heap->metadata = metadata;

    metadata->free_size = heap_size - segment_start - 1;

    heap->malloc = memory_heap_hash_malloc_ext;
    heap->free = memory_heap_hash_free;
    heap->stat = memory_heap_hash_stat;

    return heap;
}

