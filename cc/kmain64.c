#include <types.h>
#include <helloworld.h>

uint8_t kmain64() {
	char_t* data = hello_world();
	data[0]='h';
	return 0;
}
