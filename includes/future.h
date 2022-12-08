/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___FUTURE_H
#define ___FUTURE_H 0

#include <types.h>
#include <memory.h>
#include <cpu/sync.h>

typedef void * future_t;

future_t future_create_with_heap_and_data(memory_heap_t* heap, lock_t lock, void* data);
#define future_create(l) future_create_with_heap_and_data(NULL, l, NULL)
#define future_create_with_data(l, d) future_create_with_heap_and_data(NULL, l, d)

void* future_get_data_and_destroy(future_t future);

#endif
