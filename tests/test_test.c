#include "setup.h"

uint32_t main(uint32_t argc, char_t** argv) {
	setup_ram();

	UNUSED(argc);
	UNUSED(argv);

	char_t* test = memory_malloc(100);
	memory_free(test);

	return 0;
}
