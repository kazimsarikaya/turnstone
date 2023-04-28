/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE 0x800000
#include "setup.h"
#include <set.h>
#include <bplustree.h>

int32_t main(uint32_t argc, char_t** argv);

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

    print_success("TESTS PASSED");

    return 0;
}
