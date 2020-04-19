#include <utils.h>
#include <memory.h>

number_t power(number_t base, number_t p) {
	if (p == 0 ) {
		return 1;
	}
	number_t ret = 1;
	while(p) {
		if(p & 0x1) {
			p--;
			ret *= base;
		} else {
			p /= 2;
			base *= base;
		}
	}
	return ret;
}
