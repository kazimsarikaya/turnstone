#ifndef ___MEMORY_H
#define ___MEMORY_H 0

#include <types.h>

#ifndef NULL
#define NULL 0
#endif

#ifndef MMAP_MAX_ENTRY_COUNT
#define MMAP_MAX_ENTRY_COUNT 128
#endif

typedef struct memory_map {
	uint64_t base;
	uint64_t length;
	uint32_t type;
	uint32_t acpi;
} __attribute__((packed)) memory_map_t;

uint8_t init_simple_memory();
void *simple_kmalloc(size_t);
uint8_t simple_kfree(void*);
void simple_memset(void*,uint8_t,size_t);
#define simple_memclean(addr,size) simple_memset(addr,NULL,size)

size_t detect_memory(memory_map_t**);

#endif
