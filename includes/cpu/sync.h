/**
 * @file sync.h
 * @brief synchronization interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___CPU_SYNC_H
/*! prevent duplicate header error macro */
#define ___CPU_SYNC_H 0

#include <types.h>
#include <memory.h>

#define SYNC_LOCK_SIZE 0x20

typedef void* lock_t;

lock_t lock_create_with_heap_for_future(memory_heap_t* heap, boolean_t for_future);
int8_t lock_destroy(lock_t lock);

#define lock_create_with_heap(h) lock_create_with_heap_for_future(h, 0)
#define lock_create() lock_create_with_heap(NULL)
#define lock_create_for_future() lock_create_with_heap_for_future(NULL, 1)

void lock_acquire(lock_t lock);
void lock_release(lock_t lock);

typedef void* semaphore_t;

semaphore_t semaphore_create_with_heap(memory_heap_t* heap, uint64_t count);
int8_t semaphore_destroy(semaphore_t semaphore);

#define semaphore_create(c) semaphore_create_with_heap(NULL, c)

int8_t semaphore_acquire_with_count(semaphore_t semaphore, uint64_t count);
#define semaphore_acquire(s) semaphore_acquire_with_count(s, 1)

int8_t semaphore_release_with_count(semaphore_t semaphore, uint64_t count);
#define semaphore_release(s) semaphore_release_with_count(s, 1)

#endif
