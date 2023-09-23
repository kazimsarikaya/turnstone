/**
 * @file quicksort.h
 * @brief quicksort interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___QUICKSORT_H
#define ___QUICKSORT_H

#include <types.h>


typedef int8_t (*quicksort_comparator_f)(void* a, void* b);
typedef void   (*quicksort_swap_f)(void* a, void* b, uint64_t item_size);

void quicksort_partial(void* array, uint64_t start, uint64_t end, uint64_t item_size, quicksort_comparator_f comparator, quicksort_swap_f swap);

static inline void quicksort(void* array, uint64_t size, uint64_t item_size, quicksort_comparator_f comparator, quicksort_swap_f swap)
{
    quicksort_partial(array, 0, size - 1, item_size, comparator, swap);
}

void quicksort2_partial(void** array, uint64_t start, uint64_t end, quicksort_comparator_f comparator);

static inline void quicksort2(void** array, uint64_t size, quicksort_comparator_f comparator)
{
    quicksort2_partial(array, 0, size - 1, comparator);
}


#endif
