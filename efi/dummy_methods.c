/**
 * @file dummy_methods.c
 * @brief dummy methods for efi.
 * @details this file contains methods that are not required for efi however are required for linking.
 *
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

/*! module name */
MODULE("turnstone.efi");

/*! dummy task_t type for efi */
typedef void * task_t;

/*! dummy lock_t type for efi */
typedef void * lock_t;

/*! global video lock for efi */
lock_t video_lock = NULL;

/*! global kernel panic lock for efi */
boolean_t KERNEL_PANIC_DISABLE_LOCKS = false;

/*! dummy future_t type for efi */
typedef void * future_t;

/*! windowmanager initialized flag global variable */
boolean_t windowmanager_initialized = false;

/**
 * @brief dummy method for efi for getting the current task.
 * @details this method is not required for efi however is required for linking.
 * @return NULL
 */
void* task_get_current_task(void);

/**
 * @brief dummy method for efi for yielding the current task.
 * @details this method is not required for efi however is required for linking.
 */
void task_yield(void);

/**
 * @brief dummy method for efi for backtracing.
 * @details this method is not required for efi however is required for linking.
 */
void backtrace(void);

/**
 * @brief dummy method for efi for getting the current task id.
 * @details this method is not required for efi however is required for linking.
 * @return 0
 */
uint64_t task_get_id(void);

/**
 * @brief dummy method for efi for getting local apic id.
 * @details this method is not required for efi however is required for linking.
 * @return 0
 */
int8_t apic_get_local_apic_id(void);

/**
 * @brief dummy method for efi for creating a future.
 * @details this method is not required for efi however is required for linking. if data is not NULL then it is returned. otherwise 0xdeadbeaf is returned.
 * @param heap heap to allocate future on. (ignored)
 * @param lock lock to use for future. (ignored)
 * @param data data to store in future.
 * @return data if data is not NULL. otherwise 0xdeadbeaf.
 */
future_t future_create_with_heap_and_data(memory_heap_t* heap, lock_t lock, void* data);

/**
 * @brief dummy method for efi for getting data from future and destroying it.
 * @details this method is not required for efi however is required for linking. if fut is 0xdeadbeaf then NULL is returned. otherwise fut is returned. fut value may be a data pointer. see future_create_with_heap_and_data.
 * @param fut future to get data from.
 * @return fut if fut is not 0xdeadbeaf. otherwise NULL.
 */
void* future_get_data_and_destroy(future_t fut);

/**
 * @brief dummy method for efi for getting input buffer.
 * @details this method is not required for efi however is required for linking. NULL is returned.
 * @return NULL
 */
buffer_t* task_get_input_buffer(void);

/**
 * @brief dummy method for efi for getting output buffer.
 * @details this method is not required for efi however is required for linking. NULL is returned.
 * @return NULL
 */
buffer_t* task_get_output_buffer(void);

/**
 * @brief dummy method for efi for getting error buffer.
 * @details this method is not required for efi however is required for linking. NULL is returned.
 * @return NULL
 */
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

/*! efi boot services global variable */
extern efi_boot_services_t* BS;

/**
 * brief allocates a frame from the efi boot services.
 * @param[in] self frame allocator to use. (ignored)
 * @param[in] count number of frames to allocate.
 * @param[in] fa_type type of frame allocation to use. (ignored)
 * @param[out] fs frame to allocate.
 * @param[out] alloc_list_size size of allocation list. (ignored)
 * @return 0 on success. error code otherwise.
 */
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

/*! efi runtime services global variable */
extern efi_runtime_services_t* RS;

/*! day count of each month */
const int32_t time_days_of_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/**
 * @brief check if a year is leap year
 * @param[in] year the year to check
 * @return true if the year is leap year
 */
static inline boolean_t time_is_leap(int64_t year) {
    return year % 400 == 0 || (year % 4 == 0  && year % 100 != 0);
}

/*! start year of timestamp */
#define TIME_TIMESTAMP_START_YEAR  1970
/*! seconds of a minute */
#define TIME_SECONDS_OF_MINUTE       60
/*! seconds of a hour */
#define TIME_SECONDS_OF_HOUR       3600
/*! seconds of a day */
#define TIME_SECONDS_OF_DAY       86400
/*! seconds of a month */
#define TIME_SECONDS_OF_MONTH   2629743
/*! seconds of a year */
#define TIME_SECONDS_OF_YEAR   31556926
/*! days of a leap year */
#define TIME_DAYS_AT_YEAR           365
/*! days of a leap year */
#define TIME_DAYS_AT_LEAP_YEAR      366

time_t time_ns(time_t* t) {
    UNUSED(t);

    efi_time_t time = {0};
    efi_time_capabilities_t time_caps = {0};

    if(RS->get_time(&time, &time_caps) != EFI_SUCCESS) {
        return 0;
    }

    time_t res = 0;

    res = time.second;
    res += time.minute * TIME_SECONDS_OF_MINUTE;
    res += time.hour * TIME_SECONDS_OF_HOUR;

    int64_t days = time.day - 1;

    for(int16_t i = 0; i < time.month - 1; i++) {
        days += time_days_of_month[i];
    }

    int64_t year = time.year;

    if(time_is_leap(year) && time.month > 2) {
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

    res += days * TIME_SECONDS_OF_DAY;

    res *= 1000000000ULL; // ns

    res += time.nano_second;

    return res;
}
