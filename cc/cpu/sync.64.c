/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <cpu/sync.h>
#include <cpu/task.h>
#include <video.h>

typedef struct {
	memory_heap_t* heap;
	uint64_t lock_value;
	uint64_t owner_task_id;
	boolean_t for_future;
}lock_internal_t;

static int8_t sync_test_set_get(uint64_t* value, int8_t offset){
	int8_t res = 0;
	__asm__ __volatile__ ("lock bts %%rbx, (%%rax)\n" : "=@ccc" (res) : "a" (value), "b" (offset));
	return res;
}


lock_t lock_create_with_heap_for_future(memory_heap_t* heap, boolean_t for_future) {
	lock_internal_t* lock = memory_malloc_ext(heap, sizeof(lock_internal_t), 0x0);
	lock->heap = heap;
	lock->for_future = for_future;

	if(lock->for_future) {
		lock->lock_value = 1;
	}

	return lock;
}

int8_t lock_destroy(lock_t lock){
	lock_internal_t* li = (lock_internal_t*)lock;
	return memory_free_ext(li->heap, li);
}

void lock_acquire(lock_t lock) {
	lock_internal_t* li = (lock_internal_t*)lock;
	task_t* current_task = task_get_current_task();
	uint64_t current_task_id;

	if(current_task == NULL) {
		current_task_id = TASK_KERNEL_TASK_ID;
	} else {
		current_task_id = current_task->task_id;
	}

	if(li->for_future == 0 && li->owner_task_id == current_task_id) {
		return;
	}

	while(sync_test_set_get(&li->lock_value, 0)) {
		task_yield();
	}

	li->owner_task_id = current_task_id;
}

void lock_release(lock_t lock) {
	lock_internal_t* li = (lock_internal_t*)lock;

	if(li) {
		li->lock_value = 0;
		li->owner_task_id = 0;
	}
}

typedef struct {
	memory_heap_t* heap;
	lock_t lock;
	uint64_t initial_count;
	uint64_t current_count;
}semaphore_internal_t;

semaphore_t semaphore_create_with_heap(memory_heap_t* heap, uint64_t count){
	semaphore_internal_t* semaphore = memory_malloc_ext(heap, sizeof(semaphore_internal_t), 0x0);
	semaphore->heap = heap;
	semaphore->lock = lock_create_with_heap(heap);
	semaphore->initial_count = count;
	semaphore->current_count = count;

	return semaphore;
}

int8_t semaphore_destroy(semaphore_t semaphore){
	semaphore_internal_t* si = (semaphore_internal_t*)semaphore;
	lock_destroy(si->lock);

	return memory_free_ext(si->heap, si);
}

int8_t semaphore_acquire_with_count(semaphore_t semaphore, uint64_t count){
	semaphore_internal_t* si = (semaphore_internal_t*)semaphore;

	lock_acquire(si->lock);

	if(si->initial_count < count) {
		lock_release(si->lock);

		return -1;
	}

	while(1) {
		lock_acquire(si->lock);

		if(si->current_count >= count) {
			si->current_count -= count;
			lock_release(si->lock);

			return 0;
		}

		lock_release(si->lock);
		task_yield();
	}

	return 0;
}

int8_t semaphore_release_with_count(semaphore_t semaphore, uint64_t count){
	semaphore_internal_t* si = (semaphore_internal_t*)semaphore;

	lock_acquire(si->lock);

	if(si->current_count + count > si->initial_count) {
		lock_release(si->lock);

		return -1;
	}

	si->current_count += count;

	lock_release(si->lock);

	return 0;
}
