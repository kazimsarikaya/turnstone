#include "setup.h"


int32_t main(int32_t argc, char** argv) {
	setup_ram();
	UNUSED(argc);
	UNUSED(argv);
	print_success("OK");
	return 0;
}
