/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"

uint32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    char_t* test = memory_malloc(100);
    memory_free(test);

    PRINTLOG(KERNEL, LOG_INFO, "deneme mesajı %i", 1234);

    print_success("TESTS PASSED");

    return 0;
}
