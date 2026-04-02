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
#include <zpack.h>
#include <gzip.h>
#include <compression.h>
#include <quicksort.h>
#include <assert.h>

/*! module name */
MODULE("turnstone.efi");

boolean_t windowmanager_is_initialized(void);
boolean_t windowmanager_is_initialized(void) {
    return false;
}

typedef struct efi_frame_allocator_context_t {
    uint64_t max_memory_address;
} efi_frame_allocator_context_t;

/**
 * brief allocates a frame from the efi boot services.
 * @param[in] self frame allocator to use. (ignored)
 * @param[in] count number of frames to allocate.
 * @param[in] fa_type type of frame allocation to use. (ignored)
 * @param[out] fs frame to allocate.
 * @param[out] alloc_list_size size of allocation list. (ignored)
 * @return 0 on success. error code otherwise.
 */
static int8_t efi_frame_allocate_frame_by_count(struct frame_allocator_t* self, uint32_t proximity_domain, uint64_t count, frame_allocation_type_t fa_type, frame_t** fs, uint64_t* alloc_list_size) {
    UNUSED(proximity_domain);
    UNUSED(fa_type);
    UNUSED(alloc_list_size);

    efi_frame_allocator_context_t* ctx = (efi_frame_allocator_context_t*)self->context;

    uint64_t frame_address = ctx->max_memory_address;
    uint64_t old_count     = count;

    if(count % 0x200) {
        count += 0x200;
    }

    efi_status_t res = BS->allocate_pages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_DATA, count, &frame_address);

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
    (*fs)->frame_count   = count;

    return EFI_SUCCESS;
}

efi_status_t efi_frame_allocator_init(uint64_t max_memory_address) {
    frame_allocator_t* frame_allocator = memory_malloc(sizeof(frame_allocator_t));

    if(!frame_allocator) {
        return EFI_OUT_OF_RESOURCES;
    }

    efi_frame_allocator_context_t* ctx = memory_malloc(sizeof(efi_frame_allocator_context_t));

    if(!ctx) {
        memory_free(frame_allocator);
        return EFI_OUT_OF_RESOURCES;
    }

    ctx->max_memory_address = max_memory_address;

    frame_allocator->context                 = ctx;
    frame_allocator->allocate_frame_by_count = efi_frame_allocate_frame_by_count;

    frame_set_allocator(frame_allocator);

    return EFI_SUCCESS;
}

/*! day count of each month */
static const int32_t time_days_of_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

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

static time_t efi_current_time_ns = 0;

EFIAPI static void efi_timer_cb(efi_event_t event, void* context) {
    UNUSED(event);
    UNUSED(context);

    efi_current_time_ns += 100000000ULL; // 100ms
}

static void efi_set_current_time_ns(void) {
    efi_time_t time                   = {0};
    efi_time_capabilities_t time_caps = {0};

    if(RS->get_time(&time, &time_caps) != EFI_SUCCESS) {
        return;
    }

    time_t res = 0;

    res  = time.second;
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

    efi_current_time_ns = res;
}

static void efi_timer_init(void) {
    efi_set_current_time_ns();
    efi_event_t event = {0};

    efi_status_t res = BS->create_event(EFI_EVT_TIMER | EFI_EVT_NOTIFY_SIGNAL, EFI_TPL_NOTIFY, efi_timer_cb, NULL, &event);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot create event: 0x%llx", res);
        return;
    }

    res = BS->set_timer(event, EFI_TIMER_PERIODIC, 100000000ULL / 100); // 100ms period is in 100ns units

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot set timer: 0x%llx", res);
        return;
    }

    PRINTLOG(EFI, LOG_INFO, "timer initialized. current time: %llu", efi_current_time_ns);
}

time_t time_ns(time_t* t) {
    UNUSED(t);

    if(efi_current_time_ns == 0) {
        efi_timer_init();
    }

    return efi_current_time_ns;
}
