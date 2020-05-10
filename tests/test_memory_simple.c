#include "setup.h"

int main(){
	setup_ram();
	FILE* fp;

	char* data1 = memory_malloc(sizeof(char) * 31);
	if(data1 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	memory_memcopy("data1", data1, 5);
	// if(memory_free(data) != 0) {
	// 	print_error("Cannot free");
	// 	return -1;
	// }
	char* data2 = memory_malloc(sizeof(char) * 117);
	if(data2 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	memory_memcopy("data2", data2, 5);
	char* data3 = memory_malloc(sizeof(char) * 87);
	if(data3 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	memory_memcopy("data3", data3, 5);

	memory_free(data2);

	char* data4 = memory_malloc(sizeof(char) * 29);
	if(data4 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	memory_memcopy("data4", data4, 5);

	fp = fopen( "tmp/mem1.dump", "w" );
	fwrite(mem_area, 1, RAMSIZE, fp );
	fclose(fp);

	memory_free(data1);
	memory_free(data3);
	memory_free(data4);

	print_success("OK");

	fp = fopen( "tmp/mem2.dump", "w" );
	fwrite(mem_area, 1, RAMSIZE, fp );
	fclose(fp);
	return 0;
}
