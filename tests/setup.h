/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___SETUP_H
#define ___SETUP_H 0

#include <types.h>
#include <logging.h>
#include <memory.h>
#include "os_io.h"

#ifndef RAMSIZE
#define RAMSIZE 0x100000
#endif

#define REDCOLOR "\033[1;31m"
#define GREENCOLOR "\033[1;32m"
#define RESETCOLOR "\033[0m"

int printf(const char* format, ...);
int vprintf ( const char* format, va_list arg );

size_t video_printf(char_t* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int res = vprintf(fmt, args);
	va_end(args);

	return res;
}

uint8_t mem_area[RAMSIZE] = {0};
uint64_t __kheap_bottom = 0;
void* SYSTEM_INFO = NULL;
void* KERNEL_FRAME_ALLOCATOR = NULL;

void print_success(const char* msg){
	printf("%s%s%s%s", GREENCOLOR, msg, RESETCOLOR, "\r\n");
}

void print_error(const char* msg){
	printf("%s%s%s%s", REDCOLOR, msg, RESETCOLOR, "\r\n");
}

void cpu_hlt(){
}

#define PRINTLOG(M, L, msg, ...)  if(LOG_NEED_LOG(M, L)) { \
		if(LOG_LOCATION) { video_printf("%s:%i:%s:%s: " msg "\n", __FILE__, __LINE__, logging_module_names[M], logging_level_names[L], __VA_ARGS__); } \
		else {video_printf("%s:%s: " msg "\n", logging_module_names[M], logging_level_names[L], __VA_ARGS__); } }

typedef void frame_t;
typedef int8_t memory_paging_page_type_t;
typedef void memory_page_table_t;

int8_t memory_paging_add_va_for_frame_ext(memory_page_table_t* p4, uint64_t va_start, frame_t* frm, memory_paging_page_type_t type){
	UNUSED(p4);
	UNUSED(va_start);
	UNUSED(frm);
	UNUSED(type);
	return 0;
}
memory_heap_t* heap = NULL;

void setup_ram() {
	heap = memory_create_heap_simple((size_t)&mem_area[0], (size_t)&mem_area[RAMSIZE]);
	printf("%p\n", heap);
	memory_set_default_heap(heap);
}

void dump_ram(char_t* fname){
	FILE* fp = fopen( fname, "w" );
	fwrite(mem_area, 1, RAMSIZE, fp );

	fclose(fp);
}

void* task_get_current_task(){
	return NULL;
}

void* lock_create_with_heap(memory_heap_t* heap){
	UNUSED(heap);
	return NULL;
}

void* lock_create_with_heap_for_future(memory_heap_t* heap, boolean_t for_future){
	UNUSED(heap);
	UNUSED(for_future);
	return NULL;
}

int8_t lock_destroy(void* lock){
	UNUSED(lock);
	return 0;
}

void lock_acquire(void* lock){
	UNUSED(lock);
}

void lock_release(void* lock){
	UNUSED(lock);
}

#endif
