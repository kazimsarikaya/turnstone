#include <stdio.h>
#include <stdlib.h>


int main(int argc, char **argv) {
	if (argc < 4) {
		printf("Error: not enoug arguments\n");
		return -1;
	}
	int item = 1;
	char * file_name = argv[item++];
	FILE * fp_kernel = fopen(file_name, "r+");
	if(fp_kernel == NULL) {
		printf("Error: Kernel not found\n");
		return -1;
	}
	fseek(fp_kernel, 0, SEEK_END);
	long size = ftell(fp_kernel);
	if(size < 1024) {
		fclose(fp_kernel);
		printf("Error: kernel size incorrect\n");
		return -1;
	}
	fseek(fp_kernel, 0x0310, SEEK_SET);

	char unused[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	char slottype = 1;
	int slot = 0;
	long slot_start = 0;
	long slot_end = 0;
	for(; item <argc; ) {
		file_name = argv[item++];
		FILE * fp_file = fopen(file_name, "ab+");
		if(fp_file == NULL) {
			printf("Error: File not found %s\n", file_name);
			return -1;
		}
		fseek(fp_file, 0, SEEK_END);
		size = ftell(fp_file);
		fclose(fp_file);
		if(size <= 0 || (size%512) != 0) {
			printf("Error: file size incorrect %s\n", file_name);
			return -1;
		}
		size = size/512;
		slot_end = slot_start + size -1;
		printf("File: %s, Slot: %i, Type: %i, Start: %li, End: %li \n", file_name, slot, slottype, slot_start, slot_end);
		unused[0] = slottype;
		fwrite(unused, 1, 8, fp_kernel);
		fwrite(&slot_start, 8, 1, fp_kernel);
		fwrite(&slot_end, 8, 1, fp_kernel);
		slot++;
		slottype++;
		slot_start = slot_end +1;
	}
	fclose(fp_kernel);
	return 0;
}
