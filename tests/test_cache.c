/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <cache.h>
#include <hashmap.h>
#include <linkedlist.h>
#include <strings.h>
#include <utils.h>

int32_t   main(uint32_t argc, char_t** argv);
boolean_t test_item_key_destroyer(const void* key, const void* item);

boolean_t test_item_key_destroyer(const void* key, const void* item) {
    UNUSED(key);

    memory_free((void*)item);

    return true;
}

int32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    cache_config_t cc = {0};
    cc.hard_limit = 5;
    cc.soft_limit = 2;
    cc.policy = CACHE_POLICY_COUNT;
    cc.item_key_destroyer = test_item_key_destroyer;


    cache_t* cache = cache_new(&cc);

    cache_put_by_count(cache, (void*)1, strdup("elma"));
    cache_put_by_count(cache, (void*)2, strdup("armut"));
    cache_put_by_count(cache, (void*)3, strdup("kiraz"));
    cache_put_by_count(cache, (void*)4, strdup("vişne"));
    cache_put_by_count(cache, (void*)5, strdup("kayısı"));
    cache_put_by_count(cache, (void*)6, strdup("çilek"));

    if(strcmp("çilek", cache_get(cache, (void*)6)) != 0) {
        print_error("key 6 not found");
        cache_destroy(cache);

        return -1;
    }

    if(strcmp("armut", cache_get(cache, (void*)2)) != 0) {
        print_error("key 2 not found");
        cache_destroy(cache);

        return -1;
    }

    if(strcmp("elma", cache_get(cache, (void*)1)) == 0) {
        print_error("key 1 found");
        cache_destroy(cache);

        return -1;
    }

    if(strcmp("kiraz", cache_get(cache, (void*)3)) != 0) {
        print_error("key 3 not found");
        cache_destroy(cache);

        return -1;
    }

    if(strcmp("vişne", cache_get(cache, (void*)4)) != 0) {
        print_error("key 4 not found");
        cache_destroy(cache);

        return -1;
    }

    if(strcmp("kayısı", cache_get(cache, (void*)5)) != 0) {
        print_error("key 5 not found");
        cache_destroy(cache);

        return -1;
    }

    cache_destroy(cache);

    print_success("TESTS PASSED");

    return 0;
}
