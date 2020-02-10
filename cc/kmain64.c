#include <helloworld.h>

int kmain64() {
	char * data = hello_world();
	data[0]='h';
	return 0;
}
