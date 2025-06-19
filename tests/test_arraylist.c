/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <list.h>

int32_t main(uint32_t argc, char_t** argv, char_t** en);

static int8_t int_comparator(const void* i, const void* j) {
    int64_t t_i = (int64_t)i;
    int64_t t_j = (int64_t)j;
    if(t_i < t_j) {
        return -1;
    }
    if(t_i > t_j) {
        return 1;
    }
    return 0;
}

int32_t main(uint32_t argc, char_t** argv, char_t** en) {
    UNUSED(argc);
    UNUSED(argv);
    UNUSED(en);

    list_t* list = list_create_arraylist();

    if(list == NULL) {
        print_error("Create arraylist: FAILED");
        return -1;
    }

    list_set_comparator(list, int_comparator);

    for(uint64_t i = 0; i < 10; i++){
        if(list_list_insert(list, (void*)i) == -1ULL) {
            print_error("Insert item: FAILED");
            list_destroy(list);
            return -1;
        }
    }

    print_success("list populated with size %llu", list_size(list));

    if(list_size(list) != 10) {
        print_error("Size of list is not 10");
        list_destroy(list);
        return -1;
    }

    for(int64_t i = 0; i < 10; i++) {
        int64_t item = (int64_t)list_get_data_at_position(list, i);
        if(item != i) {
            print_error("Item at position %llu is not %llu, but %llu", i, i, item);
            list_destroy(list);
            return -1;
        }
    }

    list_t* dup_list = list_duplicate_list(list);

    if(dup_list == NULL) {
        print_error("Duplicate list: FAILED");
        list_destroy(list);
        return -1;
    }

    if(list_size(dup_list) != 10) {
        print_error("Size of duplicated list is not 10");
        list_destroy(dup_list);
        list_destroy(list);
        return -1;
    }

    for(int64_t i = 0; i < 10; i++) {
        int64_t item = (int64_t)list_get_data_at_position(dup_list, i);
        if(item != i) {
            print_error("Item at position %llu in duplicated list is not %llu, but %llu", i, i, item);
            list_destroy(dup_list);
            list_destroy(list);
            return -1;
        }
    }

    size_t pos;
    if(list_get_position(dup_list, (void*)5, &pos) != 0) {
        print_error("Position of item 5 not found in duplicated list");
        list_destroy(dup_list);
        list_destroy(list);
        return -1;
    } else if(pos != 5) {
        print_error("Position of item 5 in duplicated list is not 5, but %llu", pos);
        list_destroy(dup_list);
        list_destroy(list);
        return -1;
    } else {
        print_success("Position of item 5 in duplicated list is %llu", pos);
    }

    iterator_t* iter = list_iterator_create(dup_list);

    if(iter == NULL) {
        print_error("Create iterator for duplicated list: FAILED");
        list_destroy(dup_list);
        list_destroy(list);
        return -1;
    }

    int64_t expected_item = 0;
    while(iter->end_of_iterator(iter) != 0) {
        const void* item = iter->get_item(iter);
        if(expected_item != (int64_t)item) {
            print_error("Item from iterator is not %lld, but %lld", expected_item, (int64_t)item);
            iter->destroy(iter);
            list_destroy(dup_list);
            list_destroy(list);
            return -1;
        }


        if(expected_item == 5){
            iter->delete_item(iter);
        }

        expected_item++;
        iter = iter->next(iter);
    }

    iter->destroy(iter);

    if(list_size(dup_list) != 9) {
        print_error("Size of duplicated list after deletion is not 9, but %llu", list_size(dup_list));
        list_destroy(dup_list);
        list_destroy(list);
        return -1;
    }

    iter = list_iterator_create(dup_list);

    if(iter == NULL) {
        print_error("Create iterator for duplicated list: FAILED");
        list_destroy(dup_list);
        list_destroy(list);
        return -1;
    }

    expected_item = 0;
    while(iter->end_of_iterator(iter) != 0) {
        const void* item = iter->get_item(iter);

        if(expected_item != (int64_t)item) {
            print_error("Item from iterator is not %lld, but %lld", expected_item, (int64_t)item);
            iter->destroy(iter);
            list_destroy(dup_list);
            list_destroy(list);
            return -1;
        }

        expected_item++;

        if(expected_item == 5) {
            expected_item++; // skip the deleted item
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    list_destroy(list);
    list_destroy(dup_list);

    print_success("TESTS PASSED");

    return 0;
}
