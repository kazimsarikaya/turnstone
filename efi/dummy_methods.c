/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <utils.h>
#include <crc.h>
#include <random.h>

#ifdef ___EFIBUILD

typedef void* task_t;
typedef void* lock_t;

size_t __kheap_bottom;
void* SYSTEM_INFO;
void* KERNEL_FRAME_ALLOCATOR = NULL;
lock_t video_lock = NULL;

void*    task_get_current_task(void);
void     task_yield(void);
void     backtrace(void);
uint64_t task_get_id(void);
int8_t   apic_get_local_apic_id(void);

uint64_t task_get_id(void){
    return 0;
}

void* task_get_current_task(void){
    return NULL;
}

void task_yield(void) {
}

void backtrace(void) {
}

int8_t apic_get_local_apic_id(void) {
    return 0;
}

#endif
