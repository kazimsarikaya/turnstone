/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE 0x800000
#include "setup.h"
#include <set.h>
#include <bplustree.h>
#include <strings.h>
#include <utils.h>

int32_t   main(uint32_t argc, char_t** argv);
boolean_t str_free(void* item);

boolean_t str_free(void* item) {
    memory_free(item);
    return true;
}

int32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    set_t* s = set_integer();

    for(int64_t i = 1; i < 10000; i++) {
        if(!set_append(s, (void*)i)) {
            print_error("error");
            printf("!!! %lli\n", i);
            break;
        }
    }

    set_destroy(s);

    s = set_string();

    for(int64_t i = 1; i < 100; i++) {
        char_t* s_i = itoa(i % 17);
        char_t* item = strcat("item ", s_i);
        memory_free(s_i);

        if(!set_append(s, item)) {
            memory_free(item);
        }
    }

    set_destroy_with_callback(s, str_free);

    print_success("TESTS PASSED");

    return 0;
}
