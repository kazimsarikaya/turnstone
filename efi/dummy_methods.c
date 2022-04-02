/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <utils.h>
#include <crc.h>
#include <random.h>

typedef void* task_t;
typedef void* lock_t;

size_t __kheap_bottom;
void* SYSTEM_INFO;
void* KERNEL_FRAME_ALLOCATOR = NULL;

task_t* task_get_current_task(){
	return NULL;
}

void task_yield() {
}
