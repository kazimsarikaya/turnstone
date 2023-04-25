/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <hashmap.h>
#include <strings.h>

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

    print_success("TESTS PASSED");

    return 0;
}
