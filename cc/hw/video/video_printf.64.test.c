/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tests.h>
#include <logging.h>

TEST_FUNC(video, printf1, printf) {
    UNUSED(test_no);

    printf("signed print test: 150=%li\n", 150 );
    printf("signed print test: -150=%i\n", -150 );
    printf("signed print test: -150=%09i\n", -150 );
    printf("unsigned print test %lu %li\n", -2UL, -2UL);

    printf("128 bit tests\n");
    printf("sizes: int128 %i unit128 %i float32 %i float64 %i float128 %i\n",
           sizeof(int128_t), sizeof(uint128_t),
           sizeof(float32_t), sizeof(float64_t), sizeof(float128_t) );

    float32_t f1 = 15;
    float32_t f2 = 2;
    float32_t f3 = f1 / f2;
    printf("div res: %lf\n", f3);

    printf("printf test for floats: %lf %lf %.3lf\n", -123.4567891234, -123.456, -123.4567891234);

    printf("i32 %i\n", 1 );

    return 0;
}
