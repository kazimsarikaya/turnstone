#include <types.h>
#include <utils.h>
#include <crc.h>
#include <random.h>

typedef void* task_t;
typedef void* lock_t;

size_t __kheap_bottom;
void* SYSTEM_INFO;

task_t* task_get_current_task(){
	return NULL;
}


void lock_acquire(lock_t lock){
	UNUSED(lock);
}
void lock_release(lock_t lock){
	UNUSED(lock);
}

int32_t rand(){
	return 0;
}
