/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE (128 << 20)
#include "setup.h"
#include <bplustree.h>

uint32_t main(uint32_t argc, char_t** argv);
int8_t   key_comparator(const void* i, const void* j);

int8_t key_comparator(const void* i, const void* j){
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

uint32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    index_t* idx = bplustree_create_index_with_unique(128, key_comparator, true);

    if(idx == NULL) {
        print_error("b+ tree can not created");
        return -1;
    }

    int64_t sign = 1;
    for(int64_t i = 0; i < 128 * 1024; i++) {
        idx->insert(idx, (void*)(i * sign), (void*)i, NULL);
        sign *= -1;
    }

    if(!idx->contains(idx, (void*)123456)) {
        print_error("key not found");
    }

    bplustree_destroy_index(idx);

    print_success("all tests are passed");

    return 0;
}
