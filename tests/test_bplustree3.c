/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <bplustree.h>

typedef struct {
    uint64_t key;
    uint64_t data;
}item_t;

int8_t key_comparator(const void* i, const void* j){
    size_t t_i = (size_t)i;
    size_t t_j = (size_t)j;

    if(t_i < t_j) {
        return -1;
    }

    if(t_i > t_j) {
        return 1;
    }

    return 0;
}

uint32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);


    size_t test_key[] =  {1,  4,  5,  8,
                          1,  1,  6,  7,
                          1,  3,  5,  9,
                          2,  2,  1, 10 };

    size_t test_data[] = {25, 40, 29, 50,
                          39,  1, 58, 25,
                          92, 48,  5, 11,
                          33, 35,  1, 66 };

    int max_key_count = 8;

    index_t* idx = bplustree_create_index_with_unique(max_key_count, key_comparator, true);

    if(idx == NULL) {
        print_error("b+ tree can not created");
        return -1;
    }


    size_t item_count = sizeof(test_data) / sizeof(size_t);

    printf("item count: %lx\n", item_count);

    for(size_t i = 0; i < item_count; i++) {
        item_t* item = memory_malloc(sizeof(item));

        item->key = test_key[i];
        item->data = (i << 16) | test_data[i];

        printf("try to insert item: %lx %lx\n", item->key, item->data);

        item_t* removed_item = NULL;

        int res = idx->insert(idx, (void*)test_key[i], item, (void**)&removed_item);

        if(res == -1) {
            printf("failed item: %lx %lx\n", item->key, item->data);
            print_error("insert failed");

            return -1;
        } else if(res == -2) {
            print_error("tree mallformed");

            return -1;
        } else {
            printf("item inserted: %lx %lx\n", item->key, item->data);

            if(removed_item) {
                printf("---> old item removed: %lx %lx\n", removed_item->key, removed_item->data);
                memory_free(removed_item);
            }
        }
    }

    print_success("b+ tree builded");


    iterator_t* iter = idx->create_iterator(idx);

    while(iter->end_of_iterator(iter) != 0) {
        item_t* d = (item_t*)iter->get_item(iter);

        printf("%llx: %05llx\n", d->key, d->data);

        memory_free(d);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    bplustree_destroy_index(idx);

    print_success("all tests are passed");

    return 0;
}
