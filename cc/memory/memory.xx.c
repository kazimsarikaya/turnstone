/**
 *
 * @file memory.xx.c
 * @brief main memory interface and functions implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <types.h>
#include <memory.h>
#include <cpu/task.h>
#include <cpu/sync.h>

MODULE("turnstone.lib.memory");

/*! default heap variable */
memory_heap_t* memory_heap_default = NULL;

memory_heap_t* memory_set_default_heap(memory_heap_t* heap) {
    memory_heap_t* res = memory_heap_default;
    memory_heap_default = heap;
    return res;
}

memory_heap_t* memory_get_heap(memory_heap_t* heap) {
    if(heap != NULL) {
        return heap;
    }

    task_t* current_task = task_get_current_task();

    if(current_task != NULL && current_task->heap != NULL) {
        return current_task->heap;
    }

    return memory_heap_default;
}

void* memory_malloc_ext(memory_heap_t* heap, size_t size, size_t align){
    if(size == 0) {
        return NULL;
    }

    if(size % 8) {
        size = size + (8 - (size % 8));
    }

    void* res = NULL;

    if(heap == NULL) {
        task_t* current_task = task_get_current_task();
        if(current_task != NULL && current_task->heap != NULL) {
            lock_acquire(current_task->heap->lock);
            res = current_task->heap->malloc(current_task->heap, size, align);
            lock_release(current_task->heap->lock);
        }

        if(!res) {
            lock_acquire(memory_heap_default->lock);
            res = memory_heap_default->malloc(memory_heap_default, size, align);
            lock_release(memory_heap_default->lock);
        }

    }else {
        lock_acquire(heap->lock);
        res = heap->malloc(heap, size, align);
        lock_release(heap->lock);
    }

    if(res != NULL) {
        if(align && ((size_t)res % align)) {
            memory_free_ext(heap, res); // recover
            res = NULL;
        }
    }

    return res;
}

int8_t memory_free_ext(memory_heap_t* heap, void* address){
    int8_t res = -1;

    if(heap == NULL) {
        task_t* current_task = task_get_current_task();
        if(current_task != NULL && current_task->heap != NULL) {
            lock_acquire(current_task->heap->lock);
            res = current_task->heap->free(current_task->heap, address);
            lock_release(current_task->heap->lock);
        }

        if(res == -1) {
            lock_acquire(memory_heap_default->lock);

            res = memory_heap_default->free(memory_heap_default, address);
            lock_release(memory_heap_default->lock);
        }

    }else {
        lock_acquire(heap->lock);
        res = heap->free(heap, address);
        lock_release(heap->lock);
    }

    return res;
}

void memory_get_heap_stat_ext(memory_heap_t* heap, memory_heap_stat_t* stat){
    if(heap == NULL) {
        task_t* current_task = task_get_current_task();
        if(current_task != NULL && current_task->heap != NULL) {
            current_task->heap->stat(current_task->heap, stat);

            return;
        }

        memory_heap_default->stat(memory_heap_default, stat);
    } else {
        heap->stat(heap, stat);
    }
}

int8_t memory_memset(void* address, uint8_t value, size_t size){
    if(address == NULL) {
        return -1;
    }

    uint8_t* t_addr = (uint8_t*)address;

    size_t max_regsize = sizeof(size_t);

    if(size <= (max_regsize * 32)) {
        for(size_t i = 0; i < size; i++) {
            *t_addr = value;
            t_addr++;
        }
        return 0;
    }

    size_t start = (size_t)address;

    size_t rem = start % max_regsize;
    if(rem != 0) {
        rem = max_regsize - rem;

        for(size_t i = 0; i < rem; i++) {
            *t_addr++ = value;
            size--;
        }
    }

    size_t* st_addr = (size_t*)t_addr;

    size_t rep = size / max_regsize;

    size_t pad = 0;

    if(value) {
        pad = value;
        for(uint8_t i = 0; i < max_regsize - 1; i++) {
            pad = (pad << 8) | value;
        }
    }

    for(size_t i = 0; i < rep; i++) {
        *st_addr = pad;
        st_addr++;
    }

    rem = size % max_regsize;

    if(rem > 0) {
        t_addr = (uint8_t*)st_addr;

        for(size_t i = 0; i < rem; i++) {
            *t_addr = value;
            t_addr++;
        }
    }

    return 0;
}

int8_t memory_memcopy(const void* source, const void* destination, size_t size){
    if((!source && !destination) || !size) {
        return 0;
    }

    if(source == NULL || destination == NULL) {
        return -1;
    }

    uint8_t* s_addr = (uint8_t*)source;
    uint8_t* t_addr = (uint8_t*)destination;

    size_t max_regsize = sizeof(size_t);

    if(size <= (max_regsize * 2)) {
        for(size_t i = 0; i < size; i++) {
            *t_addr = *s_addr;
            t_addr++;
            s_addr++;
        }
        return 0;
    }

    size_t start = (size_t)source;

    size_t rem = start % max_regsize;

    if(rem != 0) {
        rem = max_regsize - rem;

        for(size_t i = 0; i < rem; i++) {
            *t_addr = *s_addr;
            t_addr++;
            s_addr++;
            size--;
        }
    }

    size_t* st_addr = (size_t*)t_addr;
    size_t* ss_addr = (size_t*)s_addr;

    size_t rep = size / max_regsize;

    for(size_t i = 0; i < rep; i++) {
        *st_addr = *ss_addr;
        st_addr++;
        ss_addr++;
    }

    rem = size % max_regsize;

    if(rem > 0) {
        t_addr = (uint8_t*)st_addr;
        s_addr = (uint8_t*)ss_addr;

        for(size_t i = 0; i < rem; i++) {
            *t_addr = *s_addr;
            t_addr++;
            s_addr++;
        }
    }

    return 0;
}

int8_t memory_memcompare(const void* mem1, const void* mem2, size_t size) {
    if(!size && ((!mem1 && !mem2) || (mem1 && mem2))) {
        return 0;
    }

    if(!mem1 && mem2) {
        return -1;
    }

    if(mem1 && !mem2) {
        return 1;
    }

    if(size && !mem1 && !mem2) {
        return 0;
    }

    size_t q_size = size / sizeof(size_t);
    size_t rem = size % sizeof(size_t);

    size_t* s_mem1 = (size_t*)mem1;
    size_t* s_mem2 = (size_t*)mem2;

    for(size_t i = 0; i < q_size; i++) {
        if(s_mem1[i] != s_mem2[i]) {
            uint8_t* t_mem1 = (uint8_t*)&s_mem1[i];
            uint8_t* t_mem2 = (uint8_t*)&s_mem2[i];

            for(size_t j = 0; j < sizeof(size_t); j++) {
                if(t_mem1[j] < t_mem2[j]) {
                    return -1;
                } else if(t_mem1[j] > t_mem2[j]) {
                    return 1;
                }
            }
        }
    }

    size_t jump = sizeof(size_t) * q_size;

    uint8_t* mem1_t = (uint8_t*)mem1 + jump;
    uint8_t* mem2_t = (uint8_t*)mem2 + jump;

    for(size_t i = 0; i < rem; i++) {
        if(mem1_t[i] < mem2_t[i]) {
            return -1;
        } else if(mem1_t[i] > mem2_t[i]) {
            return 1;
        }
    }

    return 0;
}

int8_t memory_memclean(void* address, size_t size) {
    if(!address || !size) {
        return 0;
    }

    uint64_t addr = (uint64_t)address;
    uint8_t* t_addr = (uint8_t*)address;

    if(addr % 16) {

        size_t rem = addr % 16;
        size -= rem;

        if(rem >= 8) {
            uint64_t* t2_addr = (uint64_t*)t_addr;
            *t2_addr++ = 0;
            t_addr = (uint8_t*)t2_addr;
            rem -= 8;
        }

        if(rem >= 4) {
            uint32_t* t2_addr = (uint32_t*)t_addr;
            *t2_addr++ = 0;
            t_addr = (uint8_t*)t2_addr;
            rem -= 4;
        }

        if(rem >= 2) {
            uint16_t* t2_addr = (uint16_t*)t_addr;
            *t2_addr++ = 0;
            t_addr = (uint8_t*)t2_addr;
            rem -= 2;
        }

        if(rem) {
            *t_addr++ = 0;
        }
    }

    uint128_t* t128_addr = (uint128_t*)t_addr;

    size_t q_size = size / sizeof(uint128_t);

    for(size_t i = 0; i < q_size; i++) {
        *t128_addr++ = 0;
    }

    size_t rem = size % sizeof(uint128_t);

    if(rem) {
        t_addr = (uint8_t*)t128_addr;

        if(rem >= 8) {
            uint64_t* t2_addr = (uint64_t*)t_addr;
            *t2_addr++ = 0;
            t_addr = (uint8_t*)t2_addr;
            rem -= 8;
        }

        if(rem >= 4) {
            uint32_t* t2_addr = (uint32_t*)t_addr;
            *t2_addr++ = 0;
            t_addr = (uint8_t*)t2_addr;
            rem -= 4;
        }

        if(rem >= 2) {
            uint16_t* t2_addr = (uint16_t*)t_addr;
            *t2_addr++ = 0;
            t_addr = (uint8_t*)t2_addr;
            rem -= 2;
        }

        if(rem) {
            *t_addr++ = 0;
        }
    }

    return 0;
}

