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

int32_t main(void);
int8_t  key_comparator(const void* i, const void* j);

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

int32_t main(void){
    size_t test_data[] = {25, 40,   29,   50,   39,   1,    58,    25,    92,  48,  5,   11,   33,   35,  1,   66,   14,   21,   71,  93,   98, 91
                          ,   20,   87,   46,   30,   29,   54,    49,    45,  17,  17,  31,   75,   12,  47,  47,   10,   38,   3,   59,   13, 5
                          ,   80,   77,   97,   1,    6,    79,    5,     19};

    int max_key_count = 8;

    index_t* idx = bplustree_create_index(max_key_count, key_comparator);

    if(idx == NULL) {
        print_error("b+ tree can not created");
        return -1;
    }

    size_t item_count = sizeof(test_data) / sizeof(size_t);

    printf("item count: %lx\n", item_count);

    for(size_t i = 0; i < item_count; i++) {
        item_t* item = memory_malloc(sizeof(item));

        if(item == NULL) {
            print_error("cannot create item");
            bplustree_destroy_index(idx);

            return -1;
        }

        item->key = test_data[i];
        item->data = (i << 32) | test_data[i];

        printf("try to insert item: %lx %lx\n", item->key, item->data);

        int res = idx->insert(idx, (void*)test_data[i], item, NULL);

        if(res == -1) {
            printf("failed item: %lx %lx\n", item->key, item->data);
            print_error("insert failed");

            return -1;
        } else if(res == -2) {
            print_error("tree mallformed");

            return -1;
        } else {
            printf("item inserted: %lx %lx\n", item->key, item->data);
        }
    }

    print_success("b+ tree builded. size of index: %llx", idx->size(idx));

    iterator_t* iter = idx->create_iterator(idx);

    printf("iterator created: %p\n", iter);

    size_t found_count = 0;

    while(iter->end_of_iterator(iter) != 0) {
        size_t k = (size_t)iter->get_extra_data(iter);
        item_t* d = (item_t*)iter->get_item(iter);

        if(k != NULL) {
            printf("k: %lx -> ", k);
        } else {
            printf("wtf key ->");
        }

        if(d != NULL) {
            printf("d: %lx , ", d->data);
            size_t order = d->data >> 32;
            size_t value = d->data & 0xFFFFFFFF;

            if(test_data[order] == value && k == value && k == d->key) {
                found_count++;
                test_data[order] |= 1ULL << 63;
            }
        } else {
            printf("wtf data , ");
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    printf("\niteration ended\n");

    if(found_count == item_count) {
        print_success("all items found");
    } else {
        print_error("missing items");
    }

    for(size_t i = 0; i < item_count; i++) {
        test_data[i] &= ~(1ULL << 63);
    }

    iter = idx->search(idx, (void*)5, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_LESS);

    printf("iterator created: %p\n", iter);

    while(iter->end_of_iterator(iter) != 0) {
        size_t k = (size_t)iter->get_extra_data(iter);
        item_t* d = (item_t*)iter->get_item(iter);

        if(k != NULL) {
            printf("k: %lx -> ", k);
        } else {
            printf("wtf key ->");
        }

        if(d != NULL) {
            printf("d: %lx , ", d->data);
        } else {
            printf("wtf data , ");
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    printf("\niteration ended\n");

    iter = idx->search(idx, (void*)5, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_LESSOREQUAL);

    printf("iterator created: %p\n", iter);

    while(iter->end_of_iterator(iter) != 0) {
        size_t k = (size_t)iter->get_extra_data(iter);
        item_t* d = (item_t*)iter->get_item(iter);

        if(k != NULL) {
            printf("k: %lx -> ", k);
        } else {
            printf("wtf key ->");
        }

        if(d != NULL) {
            printf("d: %lx , ", d->data);
        } else {
            printf("wtf data , ");
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    printf("\niteration ended\n");

    iter = idx->search(idx, (void*)5, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

    printf("iterator created: %p\n", iter);

    while(iter->end_of_iterator(iter) != 0) {
        size_t k = (size_t)iter->get_extra_data(iter);
        item_t* d = (item_t*)iter->get_item(iter);

        if(k != NULL) {
            printf("k: %lx -> ", k);
        } else {
            printf("wtf key ->");
        }

        if(d != NULL) {
            printf("d: %lx , ", d->data);
        } else {
            printf("wtf data , ");
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    printf("\niteration ended\n");

    iter = idx->search(idx, (void*)5, (void*)7, INDEXER_KEY_COMPARATOR_CRITERIA_BETWEEN);

    printf("iterator created: %p\n", iter);

    while(iter->end_of_iterator(iter) != 0) {
        size_t k = (size_t)iter->get_extra_data(iter);
        item_t* d = (item_t*)iter->get_item(iter);

        if(k != NULL) {
            printf("k: %lx -> ", k);
        } else {
            printf("wtf key ->");
        }

        if(d != NULL) {
            printf("d: %lx , ", d->data);
        } else {
            printf("wtf data , ");
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    printf("\niteration ended\n");

    iter = idx->search(idx, (void*)5, (void*)17, INDEXER_KEY_COMPARATOR_CRITERIA_BETWEEN);

    printf("iterator created: %p\n", iter);

    while(iter->end_of_iterator(iter) != 0) {
        size_t k = (size_t)iter->get_extra_data(iter);
        item_t* d = (item_t*)iter->get_item(iter);

        if(k != NULL) {
            printf("k: %lx -> ", k);
        } else {
            printf("wtf key ->");
        }

        if(d != NULL) {
            printf("d: %lx , ", d->data);
        } else {
            printf("wtf data , ");
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    printf("\niteration ended\n");

    iter = idx->search(idx, (void*)0x5c, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUALORGREATER);

    printf("iterator created: %p\n", iter);

    while(iter->end_of_iterator(iter) != 0) {
        size_t k = (size_t)iter->get_extra_data(iter);
        item_t* d = (item_t*)iter->get_item(iter);

        if(k != NULL) {
            printf("k: %lx -> ", k);
        } else {
            printf("wtf key ->");
        }

        if(d != NULL) {
            printf("d: %lx , ", d->data);
        } else {
            printf("wtf data , ");
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    printf("\niteration ended\n");

    iter = idx->search(idx, (void*)0x5c, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_GREATER);

    printf("iterator created: %p\n", iter);

    while(iter->end_of_iterator(iter) != 0) {
        size_t k = (size_t)iter->get_extra_data(iter);
        item_t* d = (item_t*)iter->get_item(iter);

        if(k != NULL) {
            printf("k: %lx -> ", k);
        } else {
            printf("wtf key ->");
        }

        if(d != NULL) {
            printf("d: %lx , ", d->data);
        } else {
            printf("wtf data , ");
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    printf("\niteration ended\n");

    iter = idx->search(idx, (void*)7, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

    printf("iterator created: %p\n", iter);

    while(iter->end_of_iterator(iter) != 0) {
        size_t k = (size_t)iter->get_extra_data(iter);
        item_t* d = (item_t*)iter->get_item(iter);

        if(k != NULL) {
            printf("k: %lx -> ", k);
        } else {
            printf("wtf key ->");
        }

        if(d != NULL) {
            printf("d: %lx , ", d->data);
        } else {
            printf("wtf data , ");
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    printf("\niteration ended\n");

    iter = idx->search(idx, (void*)0x1000, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_GREATER);

    printf("iterator created: %p\n", iter);

    while(iter->end_of_iterator(iter) != 0) {
        size_t k = (size_t)iter->get_extra_data(iter);
        item_t* d = (item_t*)iter->get_item(iter);

        if(k != NULL) {
            printf("k: %lx -> ", k);
        } else {
            printf("wtf key ->");
        }

        if(d != NULL) {
            printf("d: %lx , ", d->data);
        } else {
            printf("wtf data , ");
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    printf("\niteration ended\n");


    iter = idx->create_iterator(idx);

    printf("iterator created: %p\n", iter);

    while(iter->end_of_iterator(iter) != 0) {
        item_t* d = (item_t*)iter->get_item(iter);

        memory_free(d);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    bplustree_destroy_index(idx);

    print_success("all tests are passed");

    return 0;
}
