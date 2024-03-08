/**
 * @file sync.64.c
 * @brief Synchronization primitives.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <cpu/sync.h>
#include <cpu/task.h>
#include <apic.h>
#include <logging.h>

MODULE("turnstone.kernel.cpu.sync");

typedef struct lock_t {
    memory_heap_t*    heap;
    volatile uint64_t lock_value;
    uint64_t          owner_task_id;
    uint64_t          owner_cpu_id;
    boolean_t         for_future;
}lock_t;

void video_text_print(const char* str);

boolean_t KERNEL_PANIC_DISABLE_LOCKS = false;

typedef uint32_t (*lock_get_local_apic_id_getter_f)(void);
typedef task_t   * (*lock_current_task_getter_f)(void);
typedef void     (*lock_task_yielder_f)(void);

lock_get_local_apic_id_getter_f lock_get_local_apic_id_getter = NULL;
lock_current_task_getter_f lock_get_current_task_getter = NULL;
lock_task_yielder_f lock_task_yielder = NULL;

void future_task_wait_toggler(uint64_t task_id);

static uint32_t lock_get_local_apic_id(void) {
    if(lock_get_local_apic_id_getter) {
        return lock_get_local_apic_id_getter();
    }

    return NULL;
}

static task_t* lock_get_current_task(void) {
    if(lock_get_current_task_getter) {
        return lock_get_current_task_getter();
    }

    return 0;
}

static void lock_task_yield(void) {
    if(lock_task_yielder) {
        lock_task_yielder();
    }
}


static inline int8_t sync_test_set_get(volatile uint64_t* value, uint64_t offset){
    int8_t res = 0;
    __asm__ __volatile__ ("lock bts %[offset], %[value]\n" : "=@ccc" (res), [value] "+m" (*value), [offset] "+r" (offset) : : "memory");
    return res;
}

lock_t* lock_create_with_heap_for_future(memory_heap_t* heap, boolean_t for_future, uint64_t task_id) {
    lock_t* lock = memory_malloc_ext(heap, sizeof(lock_t), 0x0);

    if(lock == NULL) {
        return NULL;
    }

    lock->heap = heap;
    lock->for_future = for_future;

    if(lock->for_future) {
        lock->owner_task_id = task_id;
        lock->lock_value = 1;
    }

    return lock;
}

int8_t lock_destroy(lock_t* lock){
    return memory_free_ext(lock->heap, lock);
}

void lock_acquire(lock_t* lock) {
    if(lock == NULL) {
        return;
    }

    if(KERNEL_PANIC_DISABLE_LOCKS) {
        return;
    }

    task_t* current_task = lock_get_current_task();
    uint64_t current_task_id;

    uint64_t current_cpu_id = lock_get_local_apic_id() + 1; // add one for preventing bsp cpu id 0

    if(current_task == NULL) {
        current_task_id = TASK_KERNEL_TASK_ID;
    } else {
        current_task_id = current_task->task_id;
    }

    if(lock->lock_value && lock->for_future == 0 && lock->owner_cpu_id == current_cpu_id && lock->owner_task_id == current_task_id) {
        return;
    }

    while(sync_test_set_get(&lock->lock_value, 0)) {
        if(current_cpu_id == 1) {
            if(lock->for_future) {
                // future_task_wait_toggler(lock->owner_task_id);
            }

            lock_task_yield();
        } else {
            asm volatile ("pause");
        }
    }

    if(!lock->for_future) {
        lock->owner_task_id = current_task_id;
    }

    lock->owner_cpu_id = current_cpu_id;
}

void lock_release(lock_t* lock) {
    if(lock == NULL) {
        return;
    }

    if(lock) {
        if(lock->for_future) {
            // future_task_wait_toggler(lock->owner_task_id);
        }

        lock->owner_task_id = 0;
        lock->owner_cpu_id = 0;
        lock->lock_value = 0;
    }
}

typedef struct semaphore_t {
    memory_heap_t* heap;
    lock_t*        lock;
    uint64_t       initial_count;
    uint64_t       current_count;
}semaphore_t;

semaphore_t* semaphore_create_with_heap(memory_heap_t* heap, uint64_t count){
    semaphore_t* semaphore = memory_malloc_ext(heap, sizeof(semaphore_t), 0x0);

    if(semaphore == NULL) {
        return NULL;
    }

    semaphore->heap = heap;
    semaphore->lock = lock_create_with_heap(heap);
    semaphore->initial_count = count;
    semaphore->current_count = count;

    return semaphore;
}

int8_t semaphore_destroy(semaphore_t* semaphore){
    lock_destroy(semaphore->lock);

    return memory_free_ext(semaphore->heap, semaphore);
}

int8_t semaphore_acquire_with_count(semaphore_t* semaphore, uint64_t count){
    if(semaphore == NULL) {
        return -1;
    }

    lock_acquire(semaphore->lock);

    if(semaphore->initial_count < count) {
        lock_release(semaphore->lock);

        return -1;
    }

    while(true) {
        lock_acquire(semaphore->lock);

        if(semaphore->current_count >= count) {
            semaphore->current_count -= count;
            lock_release(semaphore->lock);

            return 0;
        }

        lock_release(semaphore->lock);
        lock_task_yielder();
    }

    return 0;
}

int8_t semaphore_release_with_count(semaphore_t* semaphore, uint64_t count){
    if(semaphore == NULL) {
        return -1;
    }

    lock_acquire(semaphore->lock);

    if(semaphore->current_count + count > semaphore->initial_count) {
        lock_release(semaphore->lock);

        return -1;
    }

    semaphore->current_count += count;

    lock_release(semaphore->lock);

    return 0;
}
