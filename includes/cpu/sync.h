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

#ifdef __cplusplus
extern "C" {
#endif

/*! memory size for lock*/
#define SYNC_LOCK_SIZE 0x28

/*! lock type */
typedef struct lock_t lock_t;

/**
 * @brief creates lock
 * @param[in] heap heap for lock
 * @param[in] for_future is lock for future
 * @return lock
 */
lock_t* lock_create_with_heap_for_future(memory_heap_t* heap, boolean_t for_future, uint64_t task_id);

/**
 * @brief destroys lock
 * @param[in] lock lock to destroy
 * @return 0 if succeed
 */
int8_t lock_destroy(lock_t* lock);

/**
 * @brief macro for creating lock with heap
 * @param[in] h heap
 */
#define lock_create_with_heap(h) lock_create_with_heap_for_future(h, 0, 0)

/**
 * @brief macro for creating lock with default heap
 */
#define lock_create() lock_create_with_heap(NULL)

/**
 * @brief macro for creating future lock with default heap
 */
#define lock_create_for_future(tid) lock_create_with_heap_for_future(NULL, true, tid)

/**
 * @brief acquires lock
 * @param[in] lock lock to acquire
 */
void lock_acquire(lock_t* lock);

/**
 * @brief relaases lock
 * @param[in] lock lock to release
 */
void lock_release(lock_t* lock);

/*! semaphore type*/
typedef struct semaphore_t semaphore_t;

/**
 * @brief create semaphore
 * @param[in] heap heap where semaphore resides
 * @param[in] count semaphore count value
 * @return semaphore
 */
semaphore_t* semaphore_create_with_heap(memory_heap_t* heap, uint64_t count);

/**
 * @brief destroys semaphore
 * @param[in] semaphore semaphore to destroy
 */
int8_t semaphore_destroy(semaphore_t* semaphore);

/**
 * @brief creats sempahore at default heap with count
 * @param[in] c count
 */
#define semaphore_create(c) semaphore_create_with_heap(NULL, c)

/**
 * @brief acquires slots at semaphore
 * @param[in] semaphore semaphore to acquire
 * @param[in] count semaphore decrease value
 * @return 0 if succeed, or waits
 */
int8_t semaphore_acquire_with_count(semaphore_t* semaphore, uint64_t count);

/**
 * @brief acquires one slot at semaphore
 * @param[in] s semaphore to acquire
 */
#define semaphore_acquire(s) semaphore_acquire_with_count(s, 1)

/**
 * @brief releases slots at semaphore
 * @param[in] semaphore semaphore to release
 * @param[in] count semaphore increase value
 * @return 0 if succeed
 */
int8_t semaphore_release_with_count(semaphore_t* semaphore, uint64_t count);

/**
 * @brief releases one slot at semaphore
 * @param[in] s semaphore to release
 */
#define semaphore_release(s) semaphore_release_with_count(s, 1)

#ifdef __cplusplus
}
#endif

#endif
