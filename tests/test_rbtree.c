/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE (128 << 20)
#include "setup.h"
#include <rbtree.h>
#include <strings.h>
#include <list.h>

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

    index_t* idx = rbtree_create_index(integer_cmp);

    idx->insert(idx, (void*)1, "elma", NULL);
    idx->insert(idx, (void*)2, "armut", NULL);
    idx->insert(idx, (void*)3, "kiraz", NULL);
    idx->insert(idx, (void*)4, "karpuz", NULL);
    idx->insert(idx, (void*)5, "çilek", NULL);
    idx->insert(idx, (void*)6, "vişne", NULL);

    char_t* old = NULL;

    idx->insert(idx, (void*)1, "ayva", (void**)&old);

    uint64_t size = idx->size(idx);
    if(size != 6) {
        print_error("inserts failed");
        printf("*** size %lli\n", size);
    }

    if(strcmp("elma", old) != 0) {
        print_error("insert cannot get old data");
    }

    idx->delete(idx, (void*)4, (void**)&old);

    size = idx->size(idx);
    if(size != 5) {
        print_error("delete failed");
        printf("*** size %lli\n", size);
    }

    if(strcmp("karpuz", old) != 0) {
        print_error("delete cannot get old data");
    }

    int64_t sign = 1;

    for(int64_t i = 0; i <  128 * 1024; i++) {
        idx->insert(idx, (void*)(i * sign), (void*)i, NULL);
        // sign *= -1;
    }

    if(!idx->contains(idx, (void*)123456)) {
        print_error("contains failed");
    }

    iterator_t* iter;

    iter = idx->search(idx, (void*)90, (void*)150, INDEXER_KEY_COMPARATOR_CRITERIA_BETWEEN);

    while(iter->end_of_iterator(iter) != 0) {
        int64_t i = (int64_t)iter->get_item(iter);

        printf("%lli ", i);

        iter->next(iter);
    }

    printf("\n");

    iter->destroy(iter);

    rbtree_destroy_index(idx);

    print_success("TESTS PASSED");

    return 0;
}
