#include "setup.h"

int main(){
	setup_ram();
	FILE* fp;


	fp = fopen( "tmp/mem0.dump", "w" );
	fwrite(mem_area, 1, RAMSIZE, fp );
	fclose(fp);

	char* data1 = memory_malloc(sizeof(char) * 31);
	if(data1 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	printf("data1: %p\n", data1);
	memory_memcopy("data1", data1, 5);
	print_success("data1 ok");


	char* data2 = memory_malloc(sizeof(char) * 117);
	if(data2 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	printf("data2: %p\n", data2);
	memory_memcopy("data2", data2, 5);
	print_success("data2 ok");

	// memory_free(data1);
	// memory_free(data2);
	// fp = fopen( "tmp/mem1.dump", "w" );
	// fwrite(mem_area, 1, RAMSIZE, fp );
	// fclose(fp);
	// return 0;

	char* data3 = memory_malloc(sizeof(char) * 87);
	if(data3 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	printf("data3: %p\n", data3);
	memory_memcopy("data3", data3, 5);
	print_success("data3 ok");

	memory_free(data2);

	char* data4 = memory_malloc(sizeof(char) * 29);
	if(data4 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	printf("data4: %p\n", data4);
	memory_memcopy("data4", data4, 5);
	print_success("data4 ok");

	fp = fopen( "tmp/mem1.dump", "w" );
	fwrite(mem_area, 1, RAMSIZE, fp );
	fclose(fp);

	memory_free(data1);
	memory_free(data3);
	memory_free(data4);

	fp = fopen( "tmp/mem2.dump", "w" );
	fwrite(mem_area, 1, RAMSIZE, fp );
	fclose(fp);

	print_success("OK");

	printf("testing aligned malloc\n");

	uint8_t* items[10];

	for(int i = 0; i < 10; i++) {
		items[i] = memory_malloc_ext(NULL, 0x1000, 0x1000);
		printf("0x%08lx\n", items[i] );
		memory_free_ext(NULL, items[i]);
	}


	for(int i = 0; i < 10; i++) {
		items[i] = memory_malloc_ext(NULL, 0x1000, 0x100);
		printf("0x%08lx\n", items[i] );
		memory_free_ext(NULL, items[i]);
	}

	fp = fopen( "tmp/mem3.dump", "w" );
	fwrite(mem_area, 1, RAMSIZE, fp );
	fclose(fp);
	return 0;
}
