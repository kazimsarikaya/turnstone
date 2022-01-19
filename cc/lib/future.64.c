/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <future.h>
typedef struct {
	memory_heap_t* heap;
	lock_t lock;
	void* data;
} future_internal_t;

future_t future_create_with_heap_and_data(memory_heap_t* heap, lock_t lock, void* data) {
	future_internal_t* fi = memory_malloc_ext(heap, sizeof(future_internal_t), 0);

	fi->heap = heap;
	fi->lock = lock;
	fi->data = data;

	return fi;
}

void* future_get_data_and_destroy(future_t future) {
	future_internal_t* fi = (future_internal_t*)future;

	lock_acquire(fi->lock);

	void* data = fi->data;

	lock_destroy(fi->lock);

	memory_free_ext(fi->heap, fi);

	return data;
}
