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
#include <utils.h>
#include <random.h>

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
boolean_t windowmanager_initialized = false;

size_t                            video_printf(const char_t* fmt, ...);
void                              video_print(const char_t* fmt);
void                              print_success(const char* msg, ...);
void                              print_error(const char* msg, ...);
void                              cpu_hlt(void);
int8_t                            setup_ram2(void);
void                              remove_ram2(void);
void __attribute__((constructor)) start_ram(void);
void __attribute__((destructor))  stop_ram(void);

void print_success(const char* msg, ...){
    va_list args;
    va_start(args, msg);
    printf("%s", GREENCOLOR);
    vprintf(msg, args);
    printf("%s%s", RESETCOLOR, "\r\n");
    va_end(args);
}

void print_error(const char* msg, ...){
    va_list args;
    va_start(args, msg);
    printf("%s", REDCOLOR);
    vprintf(msg, args);
    printf("%s%s", RESETCOLOR, "\r\n");
    va_end(args);
}

void video_print(const char_t* msg) {
    printf("%s", msg);
}

buffer_t* default_buffer = NULL;

buffer_t* buffer_get_io_buffer(uint64_t buffer_io_id) {
    UNUSED(buffer_io_id);

    if(default_buffer == NULL) {
        default_buffer = buffer_new();
    }

    return default_buffer;
}

memory_heap_t* d_heap = NULL;

int8_t setup_ram2(void) {

    if(mmap_size < 4096) {
        print_error("invalid ram size min should be 4k");
        return -1;
    }
/*
    mem_backend = tmpfile();

    if(mem_backend == NULL) {
        print_error("cannot create ram tmpfile");
        return -2;
    }

    fseek(mem_backend, mmap_size - 1, SEEK_SET);

    int8_t zero = 0;

    fwrite(&zero, 1, 1, mem_backend);
    fseek(mem_backend, 0, SEEK_SET);

    mem_backend_fd  = fileno(mem_backend);
 */
    void* mmap_res = mmap((void*)mmmap_address, mmap_size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    printf("mmap res %p size 0x%llx\n", mmap_res, mmap_size);

    if(mmap_res != (void*)mmmap_address) {
        print_error("cannot mmap ram tmpfile");
        //fclose(mem_backend);
        mem_backend = NULL;
        return -3;
    }

    d_heap = memory_create_heap_hash(mmmap_address, mmmap_address + mmap_size);

    if(d_heap == NULL) {
        print_error("cannot setup heap");
        return -4;
    }

    memory_set_default_heap(d_heap);

    return 0;
}

void remove_ram2(void) {
    if(d_heap) {
        memory_heap_stat_t stat;
        memory_get_heap_stat(&stat);
        printf("mem stats:\n\tmalloc count: 0x%llx\n\tfree count: 0x%llx\n\ttotal space: 0x%llx\n\tfree space: 0x%llx\n\tdiff: 0x%llx\n\tfast hit: 0x%llx\n\theader count: 0x%llx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size, stat.fast_hit, stat.header_count);

        if(stat.malloc_count != stat.free_count) {
            print_error("memory leak detected");
        } else {
            print_success("no memeory leak detected");
        }
    }

    if(mem_backend) {
        munmap((void*)mmmap_address, mmap_size);
        //fclose(mem_backend);
    }
}

void __attribute__((constructor)) start_ram(void) {
    int8_t res = setup_ram2();

    if(res) {
        exit(res);
    }

    uint64_t seed = time_ns(NULL);

    srand(seed);
}

void __attribute__((destructor)) stop_ram(void) {
    buffer_destroy(default_buffer);

    remove_ram2();
}

#ifdef ___TESTMODE
typedef void * future_t;

void*    lock_create_with_heap(memory_heap_t* heap);
int8_t   lock_destroy(void* lock);
void     lock_acquire(void* lock);
void     lock_release(void* lock);
void*    lock_create_with_heap_for_future(memory_heap_t* heap, boolean_t for_future);
void     dump_ram(char_t* fname);
void     cpu_sti(void);
void     apic_eoi(void);
void     task_current_task_sleep(uint64_t when_tick);
time_t   rtc_get_time(void);
void*    task_get_current_task(void);
void     task_switch_task(void);
future_t future_create_with_heap_and_data(memory_heap_t* heap, lock_t lock, void* data);
void*    future_get_data_and_destroy(future_t fut);

void* SYSTEM_INFO;
void* KERNEL_FRAME_ALLOCATOR = NULL;
uint64_t __kheap_bottom = 0;

size_t video_printf(const char_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = vprintf(fmt, args);
    va_end(args);

    return (size_t)res;
}

void task_switch_task(void){
}

void cpu_sti(void){

}

void  apic_eoi(void){
}

void task_current_task_sleep(uint64_t when_tick) {
    UNUSED(when_tick);
}

time_t rtc_get_time(void){
    return time(NULL);
}

void* task_get_current_task(void){
    return NULL;
}

void* lock_create_with_heap(memory_heap_t* heap){
    UNUSED(heap);
    return (void*)0xdeadbeaf;
}

void* lock_create_with_heap_for_future(memory_heap_t* heap, boolean_t for_future){
    UNUSED(heap);
    UNUSED(for_future);
    return (void*)0xdeadbeaf;
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

future_t future_create_with_heap_and_data(memory_heap_t* heap, lock_t lock, void* data) {
    UNUSED(heap);
    UNUSED(lock);

    if(data) {
        return data;
    }

    return (void*)0xdeadbeaf;
}

void* future_get_data_and_destroy(future_t fut) {
    if(!fut) {
        return NULL;
    }

    if(((uint64_t)fut) == 0xdeadbeaf) {
        return NULL;
    }

    return fut;
}

struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
} ts;
struct timespec clock_gettime(int, struct timespec* ts);
time_t time_ns(time_t* t) {
    UNUSED(t);
    clock_gettime(1, &ts);
    return 1000000000ULL * ts.tv_sec + ts.tv_nsec;
}

#endif

#endif
