/**
 * @file quicksort.64.c
 * @brief 64-bit quicksort implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <quicksort.h>

MODULE("turnstone.lib");

void quicksort_partial(void* array, uint64_t start, uint64_t end, uint64_t item_size, quicksort_comparator_f comparator, quicksort_swap_f swap) {
    uint64_t i = start;
    uint64_t j = end;
    uint64_t pivot = start;
    uint8_t* array_ptr = (uint8_t*)array;

    while (i <= j) {
        while (comparator(array_ptr + (i * item_size), array_ptr + (pivot * item_size)) < 0) {
            i++;
        }

        while (comparator(array_ptr + (j * item_size), array_ptr + (pivot * item_size)) > 0) {
            if (j == 0) {
                break;
            }

            j--;
        }

        if (i < j) {
            swap(array_ptr + (i * item_size), array_ptr + (j * item_size), item_size);
            i++;

            if (j == 0) {
                break;
            }

            j--;
        } else if (i == j) {
            i++;

            if (j == 0) {
                break;
            }

            j--;

            break;
        }
    }

    if (start < j) {
        quicksort_partial(array, start, j, item_size, comparator, swap);
    }

    if (i < end) {
        quicksort_partial(array, i, end, item_size, comparator, swap);
    }
}

#include <stdbufs.h>

void quicksort2_partial(void** array, uint64_t start, uint64_t end, quicksort_comparator_f comparator) {
    uint64_t i = start;
    uint64_t j = end;
    uint64_t pivot = start;

    while (i <= j) {
        while (comparator(array[i], array[pivot]) < 0) {
            i++;
        }

        while (comparator(array[j], array[pivot]) > 0) {
            if (j == 0) {
                break;
            }

            j--;
        }

        if (i < j) {
            void* tmp = array[i];
            array[i] = array[j];
            array[j] = tmp;
            i++;

            if (j == 0) {
                break;
            }

            j--;
        } else if (i == j) {
            i++;

            if (j == 0) {
                break;
            }

            j--;

            break;
        }
    }

    if (start < j) {
        quicksort2_partial(array, start, j, comparator);
    }

    if (i < end) {
        quicksort2_partial(array, i, end, comparator);
    }
}
