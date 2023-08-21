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
#include <strings.h>
#include <utils.h>

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

//int                               printf(const char* format, ...);
int                               vprintf ( const char* format, va_list arg );
size_t                            video_printf(const char_t* fmt, ...);
void                              print_success(const char* msg);
void                              print_error(const char* msg);
void                              cpu_hlt(void);
int8_t                            setup_ram2(void);
void                              remove_ram2(void);
void __attribute__((constructor)) start_ram(void);
void __attribute__((destructor))  stop_ram(void);

extern int64_t errno;

size_t video_printf(const char_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = vprintf(fmt, args);
    va_end(args);

    return (size_t)res;
}

void print_success(const char* msg){
    printf("%s%s%s%s", GREENCOLOR, msg, RESETCOLOR, "\r\n");
}

void print_error(const char* msg){
    printf("%s%s%s%s", REDCOLOR, msg, RESETCOLOR, "\r\n");
}

void cpu_hlt(void) {
}

#define PRINTLOG(M, L, msg, ...)  if(LOG_NEED_LOG(M, L)) { \
            if(LOG_LOCATION) { video_printf("%s:%i:%s:%s: " msg "\n", __FILE__, __LINE__, logging_module_names[M], logging_level_names[L], ## __VA_ARGS__); } \
            else {video_printf("%s:%s: " msg "\n", logging_module_names[M], logging_level_names[L], ## __VA_ARGS__); } }

memory_heap_t* d_heap = NULL;

int8_t setup_ram2(void) {

    if(mmap_size < 4096) {
        print_error("invalid ram size min should be 4k");
        return -1;
    }
/*
    mem_backend = tmpfile();

    printf("error: %i\n", errno);

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

    printf("mmap res %p 0x%llx\n", mmap_res, mem_backend_fd);

    if(mmap_res != (void*)mmmap_address) {
        print_error("cannot mmap ram tmpfile");
        printf("error: %i\n", errno);
        fclose(mem_backend);
        mem_backend = NULL;
        return -3;
    }

    logging_module_levels[HEAP_HASH] = LOG_INFO;
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
        printf("mem stats:\n\tmalloc count: 0x%lx\n\tfree count: 0x%lx\n\ttotal space: 0x%lx\n\tfree space: 0x%lx\n\tdiff: 0x%lx\n\tfast hit: 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size, stat.fast_hit);

        if(stat.malloc_count != stat.free_count) {
            print_error("memory leak detected");
        } else {
            print_success("no memeory leak detected");
        }
    }

    if(mem_backend) {
        munmap((void*)mmmap_address, mmap_size);
        fclose(mem_backend);
    }
}

void __attribute__((constructor)) start_ram(void) {
    int8_t res = setup_ram2();

    if(res) {
        exit(res);
    }
}

void __attribute__((destructor)) stop_ram(void) {
    remove_ram2();
}


#ifdef ___TESTMODE

uint64_t __kheap_bottom = 0;

void* SYSTEM_INFO;

void* KERNEL_FRAME_ALLOCATOR = NULL;

typedef void   * frame_t;
typedef int8_t memory_paging_page_type_t;
typedef void   * memory_page_table_t;
typedef void   * future_t;

int8_t   memory_paging_add_va_for_frame_ext(memory_page_table_t* p4, uint64_t va_start, frame_t* frm, memory_paging_page_type_t type);
void     dump_ram(char_t* fname);
void*    task_get_current_task(void);
void*    lock_create_with_heap(memory_heap_t* heap);
int8_t   lock_destroy(void* lock);
void     lock_acquire(void* lock);
void     lock_release(void* lock);
void*    lock_create_with_heap_for_future(memory_heap_t* heap, boolean_t for_future);
future_t future_create_with_heap_and_data(memory_heap_t* heap, lock_t lock, void* data);
void*    future_get_data_and_destroy(future_t fut);

int8_t memory_paging_add_va_for_frame_ext(memory_page_table_t* p4, uint64_t va_start, frame_t* frm, memory_paging_page_type_t type){
    UNUSED(p4);
    UNUSED(va_start);
    UNUSED(frm);
    UNUSED(type);
    return 0;
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

#endif

#endif
