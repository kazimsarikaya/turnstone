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
#include <time.h>

#ifndef RAMSIZE
#define RAMSIZE 0x100000
#endif

#define REDCOLOR "\033[1;31m"
#define GREENCOLOR "\033[1;32m"
#define RESETCOLOR "\033[0m"

FILE* mem_backend = NULL;
int32_t mem_backend_fd = 0;
uint64_t mmmap_address = 4ULL << 30;
uint64_t mmap_size = RAMSIZE;

int printf(const char* format, ...);

void* SYSTEM_INFO = NULL;
void* KERNEL_FRAME_ALLOCATOR = NULL;

size_t video_printf(char_t* fmt, va_list args) {
    return printf(fmt, args);
}

void task_switch_task(){
}

void cpu_sti(){

}

void  apic_eoi(){
}

time_t rtc_get_time(){
    return time(NULL);
}

uint8_t mem_area[RAMSIZE] = {0};
uint64_t __kheap_bottom = 0;

void print_success(const char* msg){
    printf("%s%s%s%s", GREENCOLOR, msg, RESETCOLOR, "\r\n");
}

void print_error(const char* msg){
    printf("%s%s%s%s", REDCOLOR, msg, RESETCOLOR, "\r\n");
}

memory_heap_t* heap = NULL;

int8_t setup_ram2() {

    mem_backend = tmpfile();

    fseek(mem_backend, mmap_size - 1, SEEK_SET);

    int8_t zero = 0;

    fwrite(&zero, 1, 1, mem_backend);
    fseek(mem_backend, 0, SEEK_SET);

    mem_backend_fd  = fileno(mem_backend);

    void* mmap_res = mmap((void*)mmmap_address, mmap_size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE, mem_backend_fd, 0);

    printf("mmap res %p\n", mmap_res);

    if(mmap_res != (void*)mmmap_address) {
        fclose(mem_backend);
        return -2;
    }

    heap = memory_create_heap_simple(mmmap_address, mmmap_address + mmap_size);
    printf("%p\n", heap);
    memory_set_default_heap(heap);

    return 0;
}

void remove_ram2() {
    munmap((void*)mmmap_address, mmap_size);
    fclose(mem_backend);
}

int32_t __attribute__((constructor)) start_ram() {
    setup_ram2();
    print_success("hello world");
    return 0;
}

int32_t __attribute__((destructor)) stop_ram() {
    remove_ram2();
    print_success("bye world");
    return 0;
}

void dump_ram(char_t* fname){
    FILE* fp = fopen( fname, "w" );
    fwrite(mem_area, 1, RAMSIZE, fp );

    fclose(fp);
}

void* task_get_current_task(){
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
