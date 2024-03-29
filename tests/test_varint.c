/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <utils.h>
#include <varint.h>

int32_t main(uint32_t argc, char_t** argv);

int32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);


    const int64_t num1 = 0x123FC67ULL;
    int8_t s1 = 0, s2 = 0;

    uint8_t* res1 = varint_encode(num1, &s1);

    int64_t num2 = varint_decode(res1, &s2);

    if( num1 == num2 && s1 == s2) {
        print_success("TESTS PASSED");
    } else {
        printf("0x%llx 0x%llx %i %i ", num1, num2, s1, s2);

        for(int8_t i = 0; i < s1; i++) {
            printf("0x%x ", *(res1 + i));
        }

        printf("\n");
        print_error("TESTS FAILED");
    }

    memory_free(res1);

    return 0;
}
