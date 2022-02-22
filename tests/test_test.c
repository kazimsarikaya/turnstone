/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"

uint32_t main(uint32_t argc, char_t** argv) {
	setup_ram();

	UNUSED(argc);
	UNUSED(argv);

	char_t* test = memory_malloc(100);
	memory_free(test);

	PRINTLOG(KERNEL, LOG_INFO, "deneme mesajı %i", 1234);

	print_success("TESTS PASSED");

	memory_heap_stat_t stat;

	memory_get_heap_stat(&stat);
	printf("mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

	return 0;
}
