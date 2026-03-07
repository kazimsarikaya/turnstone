/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <list.h>
#include <random.h>

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

    for(uint64_t i = 0; i < 10; i++) {
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
    while(!iter->end_of_iterator(iter)) {
        const void* item = iter->get_item(iter);
        if(expected_item != (int64_t)item) {
            print_error("Item from iterator is not %lld, but %lld", expected_item, (int64_t)item);
            iter->destroy(iter);
            list_destroy(dup_list);
            list_destroy(list);
            return -1;
        }


        if(expected_item == 5) {
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
    while(!iter->end_of_iterator(iter)) {
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

    list = list_create_arraylist();

    if(list == NULL) {
        print_error("Create arraylist: FAILED");
        return -1;
    }

    list_queue_push(list, (void*)10);
    list_queue_push(list, (void*)20);

    int64_t val = (int64_t)list_queue_pop(list);;
    if(val != 10) {
        print_error("Queue pop expected 10 but got %lld", val);
        list_destroy(list);
        return -1;
    }

    val = (int64_t)list_queue_pop(list);;
    if(val != 20) {
        print_error("Queue pop expected 20 but got %lld", val);
        list_destroy(list);
        return -1;
    }

    list_destroy(list);

    print_success("Queue TESTS PASSED");

    list = list_create_arraylist();

    if(list == NULL) {
        print_error("Create arraylist: FAILED");
        return -1;
    }

    list_stack_push(list, (void*)30);
    list_stack_push(list, (void*)40);

    val = (int64_t)list_stack_pop(list);;
    if(val != 40) {
        print_error("Stack pop expected 40 but got %lld", val);
        list_destroy(list);
        return -1;
    }

    val = (int64_t)list_stack_pop(list);;
    if(val != 30) {
        print_error("Stack pop expected 30 but got %lld", val);
        list_destroy(list);
        return -1;
    }

    list_destroy(list);

    print_success("Stack TESTS PASSED");

    list = list_create_arraylist();

    if(list == NULL) {
        print_error("Create arraylist: FAILED");
        return -1;
    }

    list_insert_at_position(list, (void*)100, 0); // head
    list_insert_at_position(list, (void*)200, 1); // tail
    list_insert_at_position(list, (void*)150, 1); // middle

    val = (int64_t)list_get_data_at_position(list, 0);
    if(val != 100) {
        print_error("Expected 100 at position 0 but got %lld", val);
        list_destroy(list);
        return -1;
    }

    val = (int64_t)list_get_data_at_position(list, 1);
    if(val != 150) {
        print_error("Expected 150 at position 1 but got %lld", val);
        list_destroy(list);
        return -1;
    }

    val = (int64_t)list_get_data_at_position(list, 2);
    if(val != 200) {
        print_error("Expected 200 at position 2 but got %lld", val);
        list_destroy(list);
        return -1;
    }

    list_destroy(list);

    print_success("Insert at position TESTS PASSED");

    list = list_create_arraylist();

    if(list == NULL) {
        print_error("Create arraylist: FAILED");
        return -1;
    }

    list_set_comparator(list, int_comparator);


    for(int64_t i = 0; i < 100; i++) {
        list_sortedlist_insert(list, (void*)((rand64() % 200) + 200));
    }

    if(list_size(list) != 100) {
        print_error("Sorted list size is not 1000, but %llu", list_size(list));
        list_destroy(list);
        return -1;
    }

    int64_t last_val = -1;
    for(uint64_t i = 0; i < list_size(list); i++) {
        int64_t item = (int64_t)list_get_data_at_position(list, i);
        if(item < last_val) {
            print_error("List is not sorted at position %llu: %lld < %lld", i, item, last_val);
            list_destroy(list);
            return -1;
        }
        last_val = item;
    }

    print_success("Sorted list verified with size %llu", list_size(list));

    iterator_t* s_iter = list_iterator_create(list);

    if(s_iter == NULL) {
        print_error("Create iterator for sorted list: FAILED");
        list_destroy(list);
        return -1;
    }

    last_val = -1;
    while(!s_iter->end_of_iterator(s_iter)) {
        const void* item = s_iter->get_item(s_iter);
        val = (int64_t)item;
        if(val < last_val) {
            print_error("List is not sorted in iterator: %lld < %lld", val, last_val);
            s_iter->destroy(s_iter);
            list_destroy(list);
            return -1;
        }
        last_val = val;
        s_iter   = s_iter->next(s_iter);
    }

    s_iter->destroy(s_iter);

    print_success("Sorted list iterator verified with size %llu", list_size(list));


    list_set_capacity(list, 1000);

    last_val = -1;
    for(uint64_t i = 0; i < list_size(list); i++) {
        int64_t item = (int64_t)list_get_data_at_position(list, i);
        if(item < last_val) {
            print_error("List is not sorted at position %llu after capacity increase: %lld < %lld", i, item, last_val);
            list_destroy(list);
            return -1;
        }
        last_val = item;
    }

    print_success("Sorted list verified after capacity increase with size %llu", list_size(list));

    for(int64_t i = 0; i < 500; i++) {
        list_sortedlist_insert(list, (void*)(rand64() % 500));
    }

    last_val = -1;
    for(uint64_t i = 0; i < list_size(list); i++) {
        int64_t item = (int64_t)list_get_data_at_position(list, i);
        if(item < last_val) {
            print_error("List is not sorted at position %llu after capacity increase: %lld < %lld", i, item, last_val);
            list_destroy(list);
            return -1;
        }
        last_val = item;
    }

    print_success("Sorted list verified after more inserts with size %llu", list_size(list));

    s_iter = list_iterator_create(list);

    if(s_iter == NULL) {
        print_error("Create iterator for sorted list: FAILED");
        list_destroy(list);
        return -1;
    }

    last_val = -1;
    while(!s_iter->end_of_iterator(s_iter)) {
        const void* item = s_iter->delete_item(s_iter);
        val = (int64_t)item;
        if(val < last_val) {
            print_error("List is not sorted in iterator after more inserts: %lld < %lld", val, last_val);
            s_iter->destroy(s_iter);
            list_destroy(list);
            return -1;
        }
        last_val = val;
        s_iter   = s_iter->next(s_iter);
    }

    s_iter->destroy(s_iter);

    if(list_size(list) != 0) {
        print_error("Sorted list is not empty after deleting all with iterator, but size %llu", list_size(list));
        list_destroy(list);
        return -1;
    }

    print_success("Sorted list iterator verified after deleting with size %llu", list_size(list));

    list_destroy(list);

    print_success("Sorted insert TESTS PASSED");


    print_success("TESTS PASSED");

    return 0;
}
