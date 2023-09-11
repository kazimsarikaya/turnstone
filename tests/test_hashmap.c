/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <hashmap.h>
#include <strings.h>
#include <xxhash.h>

int32_t main(uint32_t argc, char_t** argv);

int32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    hashmap_t* hm = hashmap_new(10);

    hashmap_put(hm, (void*)1, "elma");
    hashmap_put(hm, (void*)11, "armut");
    hashmap_put(hm, (void*)21, "ayva");

    if(strcmp("elma", hashmap_get(hm, (void*)1)) != 0) {
        printf("!! %s\n", hashmap_get(hm, (void*)1));
        print_error("cannot get key 1");
        hashmap_destroy(hm);

        return -1;
    }

    if(strcmp("armut", hashmap_get(hm, (void*)11)) != 0) {
        print_error("cannot get key 11");
        hashmap_destroy(hm);

        return -1;
    }

    if(strcmp("ayva", hashmap_get(hm, (void*)21)) != 0) {
        print_error("cannot get key 21");
        hashmap_destroy(hm);

        return -1;
    }

    hashmap_put(hm, (void*)11, "kiraz");

    if(strcmp("kiraz", hashmap_get(hm, (void*)11)) != 0) {
        print_error("cannot get key 11");
        hashmap_destroy(hm);

        return -1;
    }

    hashmap_delete(hm, (void*)21);

    if(hashmap_get(hm, (void*)21) != NULL) {
        print_error("cannot delete key 21");
        hashmap_destroy(hm);

        return -1;
    }

    hashmap_put(hm, (void*)0, "çilek");

    if(strcmp("çilek", hashmap_get(hm, (void*)0)) != 0) {
        print_error("cannot get key 0");
        hashmap_destroy(hm);

        return -1;
    }

    hashmap_destroy(hm);

    hm = hashmap_string(128);

    hashmap_put(hm, "key", "value");

    if(strcmp("value", hashmap_get(hm, "key")) != 0) {
        print_error("cannot get key");
        hashmap_destroy(hm);

        return -1;
    }

    boolean_t pass = false;

    iterator_t* iter = hashmap_iterator_create(hm);

    while(iter->end_of_iterator(iter) != 0) {
        const char_t* v = iter->get_item(iter);

        printf("!!! %s\n", v);

        if(strcmp("value", v) == 0) {
            pass = true;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    hashmap_destroy(hm);

    hm = hashmap_integer(10);

    for(uint64_t i = 0; i < 100; i++) {
        hashmap_put(hm, (void*)i, (void*)i);
    }

    pass = true;

    for(uint64_t i = 0; i < 100; i++) {
        if(hashmap_get(hm, (void*)i) != (void*)i) {
            pass = false;
        }
    }

    hashmap_destroy(hm);

    if(!pass) {
        print_error("TESTS FAILED");
    } else {
        print_success("TESTS PASSED");
    }

    return 0;
}
