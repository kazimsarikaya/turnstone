/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <quicksort.h>
#include <random.h>

int32_t main(uint32_t argc, char_t** argv, char_t** en);

static int8_t int32_comparator(const void* a, const void* b) {
    int32_t a_val = *(int32_t*)a;
    int32_t b_val = *(int32_t*)b;

    if (a_val < b_val) {
        return -1;
    } else if (a_val > b_val) {
        return 1;
    } else {
        return 0;
    }
}

int32_t main(uint32_t argc, char_t** argv, char_t** env) {
    UNUSED(argc);
    UNUSED(argv);
    UNUSED(env);

    int32_t** array = memory_malloc(sizeof(int32_t*) * 10);

    if (array == NULL) {
        print_error("Could not allocate memory for array\n");

        return -1;
    }

    for (uint32_t i = 0; i < 10; i++) {
        array[i] = memory_malloc(sizeof(int32_t));

        if (array[i] == NULL) {
            print_error("Could not allocate memory for array[%d]\n", i);

            return -1;
        }
    }

    int32_t try = 10000;

    while(try--) {
        for (uint32_t i = 0; i < 10; i++) {
            *array[i] = 10 + rand() % 0x3F;
            printf("%i ", *array[i]);
        }

        printf("\n");

        quicksort2_partial((void**)array, 0, 9, int32_comparator);

        int32_t last = -1;

        for (uint32_t i = 0; i < 10; i++) {
            printf("%i ", *array[i]);

            if (*array[i] < last) {
                print_error("Array not sorted\n");

                goto exit;
            }

            last = *array[i];
        }

        printf("\n");
    }

exit:
    for (uint32_t i = 0; i < 10; i++) {
        memory_free(array[i]);
    }

    memory_free(array);

    print_success("TESTS PASSED");

    return 0;
}
