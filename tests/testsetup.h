#ifndef ___TESTSETUP_H
#define ___TESTSETUP_H 0

#include <types.h>
#include <memory.h>
#include <systeminfo.h>

#define RAMSIZE 0x100000

int printf(const char* format, ...);

uint8_t mem_area[RAMSIZE] = {0};
uint64_t __kheap_bottom = 0;
system_info_t* SYSTEM_INFO = NULL;

void print_red() {
	printf("\033[0;31m");
}

void print_green() {
	printf("\033[0;32m");
}

void print_reset() {
	printf("\033[0m");
}

void print_success(const char* msg){
	print_green();
	printf("%s\n", msg);
	print_reset();
}

void print_error(const char* msg){
	print_red();
	printf("%s\n", msg);
	print_reset();
}

void setup_ram() {
	memory_heap_t* heap = memory_create_heap_simple((size_t)&mem_area[0], (size_t)&mem_area[RAMSIZE - 1]);
	memory_set_default_heap(heap);
}

#endif
