#ifndef ___SYSTEMINFO_H
#define ___SYSTEMINFO_H 0

#include <types.h>
#include <memory.h>

typedef struct system_info {
	memory_map_t* mmap;
#if ___BITS == 16
	uint32_t mem64bitalign;
#endif
	uint32_t mmap_entry_count;
}__attribute__((packed)) system_info_t;

extern system_info_t* SYSTEM_INFO;

#endif
