/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"

int32_t main(uint32_t argc, char_t** argv, char_t** en);

int32_t main(uint32_t argc, char_t** argv, char_t** en) {
    UNUSED(argc);
    UNUSED(argv);

    char_t* test = memory_malloc(100);
    memory_free(test);

    PRINTLOG(KERNEL, LOG_INFO, "deneme mesajı %i", 1234);

    printf("argc: %i\n", argc);

    for (uint32_t i = 0; i < argc; i++) {
        PRINTLOG(KERNEL, LOG_INFO, "argument: %i %s", i, argv[i]);
    }

    for(char_t** env = en; *env != 0; env++) {
        char* thisEnv = *env;
        PRINTLOG(KERNEL, LOG_INFO, "environment: %s", thisEnv);
    }

    print_success("TESTS PASSED");

    return 0;
}
