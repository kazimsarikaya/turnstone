/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <utils.h>
#include <math.h>

uint32_t main(uint32_t argc, char_t** argv) {

    UNUSED(argc);
    UNUSED(argv);

    printf("%li %li %li\n", sizeof(float128_t), sizeof(float64_t), sizeof(float32_t));

    printf("%024.020f\n", math_exp(2));
    float64_t val = math_log(123456789);

    printf("%024.020f %024.05f\n", val, math_exp(val) );

    printf("%020.010f\n", math_power(1.234, 3.76));

    printf("%030.025f %030.025f\n", math_log(2), math_log(10));

    printf("%010.05f\n", math_log2(16.0L));

    printf("%030.025f %030.025f\n", math_root(625, 4), math_root(12.45, 3.7));

    if(math_floor(3.5) != 3) {
        print_error("math floor error");
    }

    if(math_ceil(3.5) != 4) {
        print_error("math ceil error");
    }

    if(math_sin(90) != 1 || math_sin(180) != 0) {
        print_error("error at sin");
        printf("%01.016f %01.08f\n", math_sin(90), math_sin(180));
    }

    return 0;
}
