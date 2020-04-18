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

#if ___BITS == 16
typedef struct physical_address {
	uint32_t part1;
	uint8_t part2;
} __attribute__((packed)) physical_address_t;
#endif

typedef struct page_entry {
	uint8_t present : 1; //bit 0
	uint8_t writable : 1; //bit 1
	uint8_t user_accessible : 1; //bit 2
	uint8_t write_through_caching : 1; //bit 3
	uint8_t disable_cache : 1; //bit 4
	uint8_t accessed : 1; //bit 5
	uint8_t dirty : 1; //bit 6
	uint8_t hugepage : 1; //bit 7
	uint8_t global : 1; //bit 8
	uint8_t os_avail01 : 1; //bit 9
	uint8_t os_avail02 : 1; //bit 10
	uint8_t os_avail03 : 1; //bit 11
#if ___BITS == 16
	//physical_address_t physical_address; // bits 12-51
	uint32_t physical_address_part1 : 32; // bits 12-43
	uint8_t physical_address_part2 : 8; // bits 44-51
#elif ___BITS == 64
	uint64_t physical_address : 40; // bits 12-51
#endif
	uint8_t os_avail04 : 1; //bit 52
	uint8_t os_avail05 : 1; //bit 53
	uint8_t os_avail06 : 1; //bit 54
	uint8_t os_avail07 : 1; //bit 55
	uint8_t os_avail08 : 1; //bit 56
	uint8_t os_avail09 : 1; //bit 57
	uint8_t os_avail10 : 1; //bit 58
	uint8_t os_avail11 : 1; //bit 59
	uint8_t os_avail12 : 1; //bit 60
	uint8_t os_avail13 : 1; //bit 61
	uint8_t os_avail14 : 1; //bit 62
	uint8_t no_execute : 1; //bit 63
} __attribute__((packed)) page_entry_t;

typedef struct page_table {
	page_entry_t pages[512];
} __attribute__((packed)) page_table_t;

extern uint8_t check_longmode();
uint8_t init_simple_memory();
void *simple_kmalloc(size_t);
uint8_t simple_kfree(void*);
void simple_memset(void*,uint8_t,size_t);
#define simple_memclean(addr,size) simple_memset(addr,NULL,size)
void simple_memcpy(uint8_t*,uint8_t*,size_t);

size_t detect_memory(memory_map_t**);
size_t get_absolute_address(uint32_t);
uint8_t memory_build_page_table();

#endif
