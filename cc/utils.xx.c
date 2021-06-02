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

int8_t ito_base_with_buffer(char_t* buffer, number_t number, number_t base) {
	if(buffer == NULL) {
		return -1;
	}

	if(base < 2 || base > 36) {
		return NULL;
	}

	size_t len = 0;
	number_t temp = number;

	while(temp) {
		temp /= base;
		len++;
	}

	if(number == 0) {
		len = 1;
		buffer[0] = '0';
	}

	size_t i = 1;
	number_t r;

	while(number) {
		r = number % base;
		number /= base;

		if(r < 10) {
			buffer[len - i] = 48 + r;
		} else {
			buffer[len - i] = 55 + r;
		}

		i++;
	}

	buffer[len] = NULL;

	return 0;
}
