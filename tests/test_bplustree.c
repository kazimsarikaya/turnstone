/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <bplustree.h>

int8_t int_comparator(const void* i, const void* j){
    int* t_i = (int*)i;
    int* t_j = (int*)j;
    if(*t_i < *t_j) {
        return -1;
    }
    if(*t_i > *t_j) {
        return 1;
    }
    return 0;
}

int main(){
    setup_ram();
    int test_data[] = {25, 40,   29,   50,   39,   58,   92,    48,    11,  33,  35,  66,  14,  21, 71, 93, 98,  91
                       ,   20,   87,   46,   30,   54,   49,    45,    17,  31,  75,  12,  47,  10, 38, 3,  59,  13, 5
                       ,   80,   77,   97,   1,    6,    79,    19};//, 62, 43, 27, 84, 99, 86, 44, 65, 26, 57, 37, 36};
    //int test_data[] = {10, 3, 8, 1, 15, 23, 16, 2, 9, 17, 29, 5, 6, 35, 41};
    FILE* fp;
    int max_key_count = 8;


    index_t* idx = bplustree_create_index(max_key_count, int_comparator);
    if(idx == NULL) {
        print_error("b+ tree can not created");
        return -1;
    }
    printf("item count: %i\n", sizeof(test_data) / sizeof(int));

    for(size_t i = 0; i < sizeof(test_data) / sizeof(int); i++) {
        printf("try to insert: %i\n", test_data[i]);
        int res = idx->insert(idx, &test_data[i], &test_data[i]);
        if(res == -1) {
            printf("failed item: %i\n", test_data[i]);
            print_error("insert failed");
            return -1;
        } else if(res == -2) {
            print_error("tree mallformed");
            return -1;
        } else {
            printf("item inserted: %i\n", test_data[i]);
        }
    }
    print_success("b+ tree builded");

    iterator_t* iter = idx->create_iterator(idx);
    printf("iterator created: %p\n", iter);
    while(iter->end_of_iterator(iter) != 0) {
        int* k = (int*)iter->get_extra_data(iter);
        int* d = (int*)iter->get_item(iter);
        if(k != NULL) {
            printf("k: %i -> ", *k);
        } else {
            printf("wtf key ->");
        }
        if(d != NULL) {
            printf("d: %i , ", *d);
        } else {
            printf("wtf data , ");
        }
        iter = iter->next(iter);
    }
    printf("\niteration ended\n");
    iter->destroy(iter);
    printf("iterator destroyed\n");
    bplustree_destroy_index(idx);

    idx = bplustree_create_index(max_key_count, int_comparator);
    for(size_t i = 0; i < sizeof(test_data) / sizeof(int); i++) {
        idx->insert(idx, &test_data[i], &test_data[i]);
    }
    print_success("b+ tree builded for deletion test");

    int d_key = 101;
    int* deleted_data;
    printf("try to delete not existed key: %i\n", d_key );
    if(idx->delete(idx, &d_key, (void**)&deleted_data) != -1) {
        print_error("error at deletion");
    } else {
        print_success("not existed key deletion passed");
    }

    for(size_t i = 0; i < sizeof(test_data) / sizeof(int); i++) {
        d_key = test_data[i];
        printf("try to delete existed key: %i\n", d_key );
        int result = idx->delete(idx, &d_key, (void**)&deleted_data);
        if( result != 0) {
            printf("result: %i\n", result);
            print_error("error at deletion, key not found");
            break;
        } else if (*deleted_data != d_key) {
            print_error("error at deletion, returned data is different");
            break;
        } else {
            print_success("existed key deletion passed");
        }
        iter = idx->create_iterator(idx);
        while(iter->end_of_iterator(iter) != 0) {
            int* k = (int*)iter->get_extra_data(iter);
            int* d = (int*)iter->get_item(iter);
            if(k != NULL) {
                printf("k: %i -> ", *k);
            } else {
                printf("wtf key ->");
            }
            if(d != NULL) {
                printf("d: %i , ", *d);
            } else {
                printf("wtf data , ");
            }
            iter = iter->next(iter);
        }
        iter->destroy(iter);
        printf("\n");
    }

    bplustree_destroy_index(idx);

    print_success("all tests are passed");

    fp = fopen( "tmp/mem1.dump", "w" );
    fwrite(mem_area, 1, RAMSIZE, fp );

    fclose(fp);

    return 0;
}
