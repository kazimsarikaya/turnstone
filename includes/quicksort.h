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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function pointer type for comparing two elements during quicksort.
 *
 * This comparator function should return:
 * - A negative value if `a` is less than `b`.
 * - Zero if `a` is equal to `b`.
 * - A positive value if `a` is greater than `b`.
 *
 * @param a Pointer to the first element.
 * @param b Pointer to the second element.
 * @return An integer indicating the relative order of `a` and `b`.
 */
typedef int8_t (*quicksort_comparator_f)(const void* a, const void* b);

/**
 * @brief Function pointer type for swapping two elements during quicksort.
 *
 * This function should swap the contents of the memory locations pointed to by `a` and `b`,
 * considering `item_size` bytes for each element.
 *
 * @param a Pointer to the first element to swap.
 * @param b Pointer to the second element to swap.
 * @param item_size The size in bytes of each element.
 */
typedef void (*quicksort_swap_f)(void* a, void* b, uint64_t item_size);

/**
 * @brief Sorts a partial section of an array using the Quicksort algorithm.
 *
 * This function implements the Quicksort algorithm to sort elements within a
 * specified range of an array. It requires a comparator function to determine
 * the order of elements and a swap function to exchange elements.
 *
 * @param array Pointer to the beginning of the array to be sorted.
 * @param start The starting index (inclusive) of the section to sort.
 * @param end The ending index (inclusive) of the section to sort.
 * @param item_size The size in bytes of each element in the array.
 * @param comparator A function pointer to compare two elements.
 * @param swap A function pointer to swap two elements.
 */
void quicksort_partial(void* array, uint64_t start, uint64_t end, uint64_t item_size, quicksort_comparator_f comparator, quicksort_swap_f swap);

/**
 * @brief Sorts an entire array using the Quicksort algorithm.
 *
 * This is an inline convenience function that calls `quicksort_partial`
 * to sort the entire array from index 0 to `size - 1`.
 *
 * @param array Pointer to the beginning of the array to be sorted.
 * @param size The total number of elements in the array.
 * @param item_size The size in bytes of each element in the array.
 * @param comparator A function pointer to compare two elements.
 * @param swap A function pointer to swap two elements.
 */
static inline void quicksort(void* array, uint64_t size, uint64_t item_size, quicksort_comparator_f comparator, quicksort_swap_f swap)
{
    quicksort_partial(array, 0, size - 1, item_size, comparator, swap);
}

/**
 * @brief Sorts a partial section of an array of pointers using the Quicksort algorithm.
 *
 * This variant of Quicksort is designed for arrays where each element is a pointer
 * to the actual data. The `item_size` is implicitly `sizeof(void*)`.
 *
 * @param array Pointer to the beginning of the array of pointers to be sorted.
 * @param start The starting index (inclusive) of the section to sort.
 * @param end The ending index (inclusive) of the section to sort.
 * @param comparator A function pointer to compare two elements (which are `void*`).
 */
void quicksort2_partial(void** array, uint64_t start, uint64_t end, quicksort_comparator_f comparator);

/**
 * @brief Sorts an entire array of pointers using the Quicksort algorithm.
 *
 * This is an inline convenience function that calls `quicksort2_partial`
 * to sort the entire array of pointers from index 0 to `size - 1`.
 *
 * @param array Pointer to the beginning of the array of pointers to be sorted.
 * @param size The total number of elements (pointers) in the array.
 * @param comparator A function pointer to compare two elements (which are `void*`).
 */
static inline void quicksort2(void** array, uint64_t size, quicksort_comparator_f comparator)
{
    quicksort2_partial(array, 0, size - 1, comparator);
}

#ifdef __cplusplus
}
#endif

#endif
