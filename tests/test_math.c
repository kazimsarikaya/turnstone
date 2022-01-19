/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <utils.h>
#include <math.h>

uint32_t main(uint32_t argc, char_t** argv) {
	setup_ram();

	UNUSED(argc);
	UNUSED(argv);

	printf("%i %i %i\n", sizeof(float128_t), sizeof(float64_t), sizeof(float32_t));

	printf("%024.020f\n", math_exp(2));
	float64_t val = math_log(123456789);

	printf("%024.020f %024.05f\n", val, math_exp(val) );

	printf("%020.010f\n", math_power(1.234, 3.76));

	printf("%030.025f %030.025f\n", math_log(2), math_log(10));

	printf("%010.05f\n", math_log2(16.0L));

	printf("%030.025f %030.025f\n", math_root(625, 4), math_root(12.45, 3.7));

	return 0;
}
