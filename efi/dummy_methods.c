/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <setup.h>
#include <types.h>
#include <utils.h>
#include <crc.h>
#include <random.h>
#include <memory.h>
#include <hashmap.h>
#include <bloomfilter.h>
#include <math.h>
#include <cache.h>
#include <bplustree.h>
#include <rbtree.h>
#include <binarysearch.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <efi.h>
#include <stdbufs.h>
#include <deflate.h>
#include <compression.h>
#include <quicksort.h>

MODULE("turnstone.efi");

#ifdef ___EFIBUILD

typedef void* task_t;
typedef void* lock_t;

size_t __kheap_bottom;
lock_t video_lock = NULL;
boolean_t KERNEL_PANIC_DISABLE_LOCKS = false;
typedef void * future_t;
boolean_t windowmanager_initialized = false;

void*    task_get_current_task(void);
void     task_yield(void);
void     backtrace(void);
uint64_t task_get_id(void);
int8_t   apic_get_local_apic_id(void);
future_t future_create_with_heap_and_data(memory_heap_t* heap, lock_t lock, void* data);
void*    future_get_data_and_destroy(future_t fut);

buffer_t* task_get_input_buffer(void);
buffer_t* task_get_output_buffer(void);
buffer_t* task_get_error_buffer(void);

uint64_t task_get_id(void){
    return 0;
}

void* task_get_current_task(void){
    return NULL;
}

void task_yield(void) {
}

buffer_t* task_get_input_buffer(void) {
    return NULL;
}

buffer_t* task_get_output_buffer(void) {
    return NULL;
}

buffer_t* task_get_error_buffer(void) {
    return NULL;
}

void backtrace(void) {
}

int8_t apic_get_local_apic_id(void) {
    return 0;
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

extern efi_boot_services_t* BS;

int8_t efi_frame_allocate_frame_by_count(struct frame_allocator_t* self, uint64_t count, frame_allocation_type_t fa_type, frame_t** fs, uint64_t* alloc_list_size);


int8_t efi_frame_allocate_frame_by_count(struct frame_allocator_t* self, uint64_t count, frame_allocation_type_t fa_type, frame_t** fs, uint64_t* alloc_list_size) {
    UNUSED(self);
    UNUSED(fa_type);
    UNUSED(alloc_list_size);

    uint64_t frame_address = 0;
    uint64_t old_count = count;

    if(count % 0x200) {
        count += 0x200;
    }

    efi_status_t res = BS->allocate_pages(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_DATA, count, &frame_address);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate frame");

        return -1;
    }

    if(old_count % 0x200) {
        uint64_t old_frame_address = frame_address;

        if(frame_address % MEMORY_PAGING_PAGE_LENGTH_2M) {
            uint64_t diff = MEMORY_PAGING_PAGE_LENGTH_2M - (frame_address % MEMORY_PAGING_PAGE_LENGTH_2M);
            frame_address += diff;

            BS->free_pages(old_frame_address, diff / FRAME_SIZE);
        }

    }

    *fs = memory_malloc(sizeof(frame_t));

    if(!*fs) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate frame");

        return -1;
    }

    (*fs)->frame_address = frame_address;
    (*fs)->frame_count = count;

    (*fs)->frame_address = frame_address;
    (*fs)->frame_count = count;

    return EFI_SUCCESS;
}


efi_status_t efi_frame_allocator_init(void) {
    frame_allocator_t* frame_allocator = memory_malloc(sizeof(frame_allocator_t));

    if(!frame_allocator) {
        return EFI_OUT_OF_RESOURCES;
    }

    KERNEL_FRAME_ALLOCATOR = frame_allocator;

    frame_allocator->allocate_frame_by_count = efi_frame_allocate_frame_by_count;

    return EFI_SUCCESS;
}

extern efi_runtime_services_t* RS;

const int32_t time_days_of_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static inline boolean_t time_is_leap(int64_t year) {
    return year % 400 == 0 || (year % 4 == 0  && year % 100 != 0);
}

time_t time_ns(time_t* tloc) {
    UNUSED(tloc);
#define TIME_TIMESTAMP_START_YEAR  1970
#define TIME_SECONDS_OF_MINUTE       60
#define TIME_SECONDS_OF_HOUR       3600
#define TIME_SECONDS_OF_DAY       86400
#define TIME_SECONDS_OF_MONTH   2629743
#define TIME_SECONDS_OF_YEAR   31556926
#define TIME_DAYS_AT_YEAR           365
#define TIME_DAYS_AT_LEAP_YEAR      366

    efi_time_t time = {0};
    efi_time_capabilities_t time_caps = {0};

    if(RS->get_time(&time, &time_caps) != EFI_SUCCESS) {
        return 0;
    }

    time_t t = 0;

    t = time.second;
    t += time.minute * TIME_SECONDS_OF_MINUTE;
    t += time.hour * TIME_SECONDS_OF_HOUR;

    int64_t days = time.day - 1;

    for(int16_t i = 0; i < time.month - 1; i++) {
        days += time_days_of_month[i];
    }

    int64_t year = time.year;

    if(time_is_leap(year)) {
        days++;
    }

    year--;

    while(year >= TIME_TIMESTAMP_START_YEAR) {
        if(time_is_leap(year)) {
            days += TIME_DAYS_AT_LEAP_YEAR;
        } else {
            days += TIME_DAYS_AT_YEAR;
        }

        year--;
    }

    t += days * TIME_SECONDS_OF_DAY;

    t *= 1000000000ULL; // ns

    t += time.nano_second;

    return t;
}

#endif
