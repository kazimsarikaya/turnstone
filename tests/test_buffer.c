/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <buffer.h>
#include <strings.h>
#include <utils.h>

uint32_t main(uint32_t argc, char_t** argv) {
	setup_ram();

	UNUSED(argc);
	UNUSED(argv);

	buffer_t buf = buffer_new_with_capacity(NULL, 5);

	buffer_append_byte(buf, 'H');

	uint64_t l = 0;
	char_t* res = (char_t*)buffer_get_bytes(buf, &l);

	if(l != 1 || res[0] != 'H') {
		print_error("append byte failed");
		buffer_destroy(buf);
		memory_free(res);

		return -1;
	}

	memory_free(res);

	buffer_append_bytes(buf, (uint8_t*)"ello World\0", strlen("ello World") + 1);

	res = (char_t*)buffer_get_bytes(buf, &l);

	if(l != strlen(res) + 1 || strcmp(res, "Hello World") != 0) {
		print_error("append bytes failed");
		buffer_destroy(buf);
		memory_free(res);

		return -1;
	}

	memory_free(res);

	buffer_destroy(buf);

	memory_heap_stat_t stat;

	memory_get_heap_stat(&stat);
	printf("mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);


	print_success("TESTS PASSED");

	return 0;
}
