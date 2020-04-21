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
	uint32_t physical_address_part1 : 32; // bits 12-43
	uint8_t physical_address_part2 : 8;  // bits 44-51
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
uint8_t memory_simple_init();
void* memory_simple_kmalloc(size_t);
uint8_t memory_simple_kfree(void*);
void memory_simple_memset(void*, uint8_t, size_t);
#define memory_simple_memclean(addr, size) memory_simple_memset(addr, NULL, size)
void memory_simple_memcpy(uint8_t*, uint8_t*, size_t);

size_t memory_detect_map(memory_map_t**);
size_t memory_get_absolute_address(size_t);
size_t memory_get_relative_address(size_t);
uint8_t memory_build_page_table();

#if ___BITS == 16
#define MEMORY_PT_GET_P4_INDEX(u64) ((u64.part_high >> 7) & 0x1FF)
#define MEMORY_PT_GET_P3_INDEX(u64) (((u64.part_high & 0x7F) << 2) | ((u64.part_low >> 31) & 0x3))
#define MEMORY_PT_GET_P2_INDEX(u64) ((u64.part_low >> 21) & 0x1FF)
#define MEMORY_PT_GET_P1_INDEX(u64) ((u64.part_low >> 12) & 0x1FF)
#elif ___BITS == 64
#define MEMORY_PT_GET_P4_INDEX(u64) ((u64 >> 39) & 0x1FF)
#define MEMORY_PT_GET_P3_INDEX(u64) ((u64 >> 30) & 0x1FF)
#define MEMORY_PT_GET_P2_INDEX(u64) ((u64 >> 21) & 0x1FF)
#define MEMORY_PT_GET_P1_INDEX(u64) ((u64 >> 12) & 0x1FF)
#endif


#endif
