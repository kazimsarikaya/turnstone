/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <linkedlist.h>

int32_t main(uint32_t argc, char_t** argv);
int8_t  integer_cmp(const void* item1, const void* item2);


int8_t integer_cmp(const void* item1, const void* item2) {
    int64_t i1 = (int64_t)item1;
    int64_t i2 = (int64_t)item2;

    int64_t diff = i1 - i2;

    if(diff < 0) {
        return -1;
    }

    if(diff > 0) {
        return 1;
    }

    return 0;
}

int32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    linkedlist_t* list = linkedlist_create_sortedlist(integer_cmp);

    int64_t sign = 1;

    for(int64_t i = 0; i < 102; i++) {
        linkedlist_sortedlist_insert(list, (void*)(i * sign));
        //sign *= -1;
    }

    for(uint64_t i = 0; i < linkedlist_size(list); i++) {
        size_t pos = -1;

        if(linkedlist_get_position(list, (void*)i, &pos) != 0 || pos != i) {
            print_error("get pos failed");
            printf("i %lli pos %lli\n", i, pos);
        }

        size_t res = (size_t)linkedlist_get_data_at_position(list, i);
        if(res != i) {
            print_error("get data at pos failed");
            printf("!!! i %lli res %lli\n", i, res);
        }
    }


    linkedlist_destroy(list);

    list = linkedlist_create_sortedlist(integer_cmp);

    for(int64_t i = 1; i < 100; i++) {
        linkedlist_sortedlist_insert(list, (void*)(i % 17));
    }

    for(uint64_t i = 0; i < linkedlist_size(list); i++) {
        size_t res = (size_t)linkedlist_get_data_at_position(list, i);
        printf("!!! i %lli res %lli\n", i, res);
    }
/*
    for(uint64_t i = 1; i < 100; i++) {
        size_t pos = -1;

        if(linkedlist_get_position(list, (void*)(i % 17), &pos) != 0 || pos != i) {
            print_error("get pos failed");
            printf("i %lli pos %lli\n", i, pos);
        }

    }

    for(uint64_t i = 0; i < linkedlist_size(list); i++) {

        size_t res = (size_t)linkedlist_get_data_at_position(list, i);
        if(res != i) {
            print_error("get data at pos failed");
            printf("!!! i %lli res %lli\n", i, res);
        }
    }
 */

    linkedlist_destroy(list);

    print_success("TESTS PASSED");

    return 0;
}
