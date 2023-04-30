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

    linkedlist_t list = linkedlist_create_sortedlist(integer_cmp);

    int64_t sign = 1;

    for(int64_t i = 0; i < 11; i++) {
        linkedlist_sortedlist_insert(list, (void*)(i * sign));
        sign *= -1;
    }
    printf("\n");

    iterator_t* iter = linkedlist_iterator_create(list);

    uint64_t pos = 0;

    while(iter->end_of_iterator(iter)) {
        int64_t i = (int64_t)iter->get_item(iter);
        printf("%lli: %lli\n", pos++, i);
        iter = iter->next(iter);
    }
    printf("\n");

    iter->destroy(iter);

    sign = 1;

    linkedlist_get_position(list, (void*)(4 * sign), &pos);

    printf("pos of %lli: %lli\n", 4 * sign, pos);

    linkedlist_get_position(list, (void*)(8 * sign), &pos);

    printf("pos of %lli: %lli\n", 8 * sign, pos);

    int64_t d = (int64_t)linkedlist_get_data_at_position(list, 5);
    printf("data at 5 is %lli\n", d);
    d = (int64_t)linkedlist_delete_at_position(list, 5);
    printf("old data at 5 is %lli\n", d);

    iter = linkedlist_iterator_create(list);

    pos = 0;

    while(iter->end_of_iterator(iter)) {
        int64_t i = (int64_t)iter->get_item(iter);
        printf("%lli: %lli\n", pos++, i);
        iter = iter->next(iter);
    }
    printf("\n");

    iter->destroy(iter);

    linkedlist_destroy(list);

    print_success("TESTS PASSED");

    return 0;
}
