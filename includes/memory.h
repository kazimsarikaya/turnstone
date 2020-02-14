#ifndef ___MEMORY_H
#define ___MEMORY_H 0

#include <types.h>

#ifndef NULL
#define NULL 0
#endif

uint8_t init_simple_memory();
void *simple_kmalloc(size_t);
uint8_t simple_kfree(void*);
void simple_memset(void*,uint8_t,size_t);
#define simple_memclean(addr,size) simple_memset(addr,NULL,size)

#endif
