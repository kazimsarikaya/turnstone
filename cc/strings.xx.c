#include <strings.h>
#include <memory.h>
#include <utils.h>


size_t strlen(char_t* string) {
	if(string == NULL) {
		return 0;
	}
	size_t ret = 0;
	while(string[ret]) {
		ret++;
	}
	return ret;
}

uint8_t strcmp(char_t* string1, char_t* string2) {
	size_t len1 = strlen(string1);
	size_t len2 = strlen(string2);
	size_t minlen = MIN(len1, len2);
	for(size_t i = 0; i < minlen; i++) {
		if(string1[i] < string2[i]) {
			return -1;
		}
		if(string1[i] > string2[i]) {
			return 1;
		}
	}
	if(len1 == len2) {
		return 0;
	}
	return (minlen == len1) ? -1 : 1;
}

uint8_t strcpy(char_t* source, char_t* destination){
	if(source == NULL || destination == NULL) {
		return -1;
	}
	for(size_t i = 0; i < strlen(source); i++) {
		destination[i] = source[i];
	}
	return 0;
}

char_t* strrev(char_t* source) {
	size_t len = strlen(source);
	if(len == 0) {
		return NULL;
	}
	char_t* dest = memory_malloc(sizeof(char_t) * len);
	if(dest == NULL) {
		return NULL;
	}
	for(size_t i = 0; i < len; i++) {
		dest[i] = source[len - i - 1];
	}
	return dest;
}

number_t ato_base(char_t* source, number_t base) {
	number_t ret = 0;
	number_t p = 0;
	size_t l = strlen(source);
	for(size_t i = 1; i <= l; i++) {
		if(source[l - i] <= '9') {
			ret += ((number_t)(source[l - i] - 48)) * power(base, p);
		} else {
			ret += ((number_t)(source[l - i] - 55)) * power(base, p);
		}
		p++;
	}
	return ret;
}

char_t* ito_base(number_t number, number_t base){
	char_t buf[64];

	memory_memclean(buf, 64);

	if(ito_base_with_buffer(buf, number, base) != 0) {
		return NULL;
	}

	size_t len = strlen(buf);
	char_t* ret = memory_malloc(sizeof(char_t) * len + 1);

	if (ret == NULL) {
		return NULL;
	}

	strcpy(buf, ret);

	return ret;
}

char_t* strdup_at_heap(memory_heap_t* heap, char_t* src){
	if(src == NULL) {
		return NULL;
	}
	size_t l = strlen(src);
	char_t* res = memory_malloc_ext(heap, sizeof(char_t) * l + 1, 0x0);
	memory_memcopy(src, res, l);
	return res;
}
