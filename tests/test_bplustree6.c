/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <linkedlist.h>
#include <utils.h>
#include <xxhash.h>
#include <strings.h>
#include <bplustree.h>


typedef struct tosdb_memtable_secondary_index_item_t {
    uint64_t secondary_key_hash;
    uint64_t secondary_key_length;
    uint64_t id;
    uint8_t  data[];
}__attribute__((packed, aligned(8))) tosdb_memtable_secondary_index_item_t;

int32_t main(uint32_t argc, char_t** argv);
int8_t  key_cmp(const void* item1, const void* item2);

int8_t key_cmp(const void* i1, const void* i2) {
    const tosdb_memtable_secondary_index_item_t* ti1 = (tosdb_memtable_secondary_index_item_t*)i1;
    const tosdb_memtable_secondary_index_item_t* ti2 = (tosdb_memtable_secondary_index_item_t*)i2;

    if(ti1->secondary_key_hash < ti2->secondary_key_hash) {
        return -1;
    }

    if(ti1->secondary_key_hash > ti2->secondary_key_hash) {
        return 1;
    }

    if(!ti1->secondary_key_length && !ti2->secondary_key_length) {
        return 0;
    }

    uint64_t min = MIN(ti1->secondary_key_length, ti2->secondary_key_length);

    int8_t res = memory_memcompare(ti1->data, ti2->data, min);

    if(min && res != 0) {
        return res;
    }

    if(ti1->secondary_key_length < ti2->secondary_key_length) {
        return -1;
    }

    if(ti1->secondary_key_length > ti2->secondary_key_length) {
        return 1;
    }

    return 0;
}

int32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    index_t* idx = bplustree_create_index_with_unique(16, key_cmp, false);

    for(int64_t i = 1; i < 100; i++) {
        char_t* s_i = itoa(i % 17);
        char_t* item = strcat("item ", s_i);
        memory_free(s_i);

        int64_t ii_size = sizeof(tosdb_memtable_secondary_index_item_t) + strlen(item);
        tosdb_memtable_secondary_index_item_t* ii = memory_malloc(ii_size);

        if(!ii) {
            break;
        }

        ii->secondary_key_length = strlen(item);
        ii->secondary_key_hash = xxhash64_hash(item, ii->secondary_key_length);
        memory_memcopy(item, ii->data, ii->secondary_key_length);
        ii->id = i;

        memory_free(item);

        idx->insert(idx, ii, ii, NULL);

    }


    iterator_t* iter = idx->create_iterator(idx);

    printf("!!! ls %lli\n", idx->size(idx));

    uint64_t prev_hash = 0;

    while(iter->end_of_iterator(iter)) {
        const tosdb_memtable_secondary_index_item_t* ii = iter->get_item(iter);

        if(prev_hash > ii->secondary_key_hash) {
            print_error("order mismatch");
        }

        prev_hash = ii->secondary_key_hash;

        printf("!!! %03lli 0x%llx %lli ", ii->id, ii->secondary_key_hash, ii->secondary_key_length);

        for(uint64_t i = 0; i < ii->secondary_key_length; i++) {
            printf("%c", ii->data[i]);
        }
        printf("\n");

        memory_free((void*)ii);

        iter = iter->next(iter);
    }
    printf("\n");

    iter->destroy(iter);


    bplustree_destroy_index(idx);

    print_success("TESTS PASSED");

    return 0;
}
