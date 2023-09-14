/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <utils.h>
#include <crc.h>
#include <random.h>
#include <memory.h>
#include <hashmap.h>
#include <bloomfilter.h>
#include <math.h>
#include <cache.h>
#include <bplustree.h>
#include <rbtree.h>
#include <binarysearch.h>

MODULE("turnstone.efi");

#ifdef ___EFIBUILD

typedef void* task_t;
typedef void* lock_t;

size_t __kheap_bottom;
void* KERNEL_FRAME_ALLOCATOR = NULL;
lock_t video_lock = NULL;
boolean_t KERNEL_PANIC_DISABLE_LOCKS = false;
typedef void * future_t;

void*    task_get_current_task(void);
void     task_yield(void);
void     backtrace(void);
uint64_t task_get_id(void);
int8_t   apic_get_local_apic_id(void);
future_t future_create_with_heap_and_data(memory_heap_t* heap, lock_t lock, void* data);
void*    future_get_data_and_destroy(future_t fut);

uint64_t task_get_id(void){
    return 0;
}

void* task_get_current_task(void){
    return NULL;
}

void task_yield(void) {
}

void backtrace(void) {
}

int8_t apic_get_local_apic_id(void) {
    return 0;
}

future_t future_create_with_heap_and_data(memory_heap_t* heap, lock_t lock, void* data) {
    UNUSED(heap);
    UNUSED(lock);

    if(data) {
        return data;
    }

    return (void*)0xdeadbeaf;
}

void* future_get_data_and_destroy(future_t fut) {
    if(!fut) {
        return NULL;
    }

    if(((uint64_t)fut) == 0xdeadbeaf) {
        return NULL;
    }

    return fut;
}


#endif
