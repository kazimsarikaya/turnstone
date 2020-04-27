/**
 * @file memory.h
 * @brief memory functions
 */
#ifndef ___MEMORY_H
/*! prevent duplicate header error macro */
#define ___MEMORY_H 0

#include <types.h>

#ifndef NULL
/*! NULL value */
#define NULL 0
#endif

/*! while memory detection max memory entry count to search */
#define MEMORY_MMAP_MAX_ENTRY_COUNT 128

/**
 * @enum memory_map_type_t
 * @brief memory type enum
 */
typedef enum {
	MEMORY_MMAP_TYPE_USABLE = 1, ///< usable memory
	MEMORY_MMAP_TYPE_RESERVED = 2, ///< reserved memory for cpu usages
	MEMORY_MMAP_TYPE_ACPI = 3, ///< acpi memory
	MEMORY_MMAP_TYPE_ACPI_NVS = 4 ///< acpi nvs memory
}memory_map_type_t; ///< short hand for enum

/**
 * @struct memory_map_t
 * @brief memory map struct
 *
 * memory map is learned by bios interrupt. map is consist of memory holes.
 */
typedef struct {
	uint64_t base; ///< base address of memory
	uint64_t length; ///<memory length
	uint32_t type; ///< memory map type, see also \ref memory_map_type_t
	uint32_t acpi; ///< acpi extension never seen
} __attribute__((packed)) memory_map_t; ///< short hand for struct

/**
 * @struct memory_page_entry_t
 * @brief page entry struct
 *
 * each entry is 64bit long
 */
typedef struct {
	uint8_t present : 1; ///< bit 0 page present?
	uint8_t writable : 1; ///< bit 1 page can be writen?
	uint8_t user_accessible : 1; ///< bit 2 page can be accessable by user space
	uint8_t write_through_caching : 1; ///< bit 3 how cache will be writen
	uint8_t disable_cache : 1; ///< bit 4 for disable caching of page when 1
	uint8_t accessed : 1; ///< bit 5 page is accessed by cpu, cpu sets this bit
	uint8_t dirty : 1; ///< bit 6 page is writen, cpu sets this bits
	uint8_t hugepage : 1; ///< bit 7 hugepage flag for p3 (1g) and p2 (2m)
	uint8_t global : 1; ///< bit 8 page is global? for caching while page switches
	uint8_t os_avail01 : 1; ///< bit 9 is available for os
	uint8_t os_avail02 : 1; ///< bit 10 is available for os
	uint8_t os_avail03 : 1; ///< bit 11 is available for os
#if ___BITS == 16 || DOXYGEN
	uint32_t physical_address_part1 : 32; ///< bits 12-43 low 32 bits of physical address shifted by 12 (real mode)
	uint8_t physical_address_part2 : 8;  ///< bits 44-51 high 8 bits of physical adress for 40 bit length (real mode)
#endif
#if ___BITS == 64 || DOXYGEN
	uint64_t physical_address : 40; ///< bits 12-51 physical address 40 bits, shifted by 12 (long mode)
#endif
	uint8_t os_avail04 : 1; ///< bit 52 is available for os
	uint8_t os_avail05 : 1; ///< bit 53 is available for os
	uint8_t os_avail06 : 1; ///< bit 54 is available for os
	uint8_t os_avail07 : 1; ///< bit 55 is available for os
	uint8_t os_avail08 : 1; ///< bit 56 is available for os
	uint8_t os_avail09 : 1; ///< bit 57 is available for os
	uint8_t os_avail10 : 1; ///< bit 58 is available for os
	uint8_t os_avail11 : 1; ///< bit 59 is available for os
	uint8_t os_avail12 : 1; ///< bit 60 is available for os
	uint8_t os_avail13 : 1; ///< bit 61 is available for os
	uint8_t os_avail14 : 1; ///< bit 62 is available for os
	uint8_t no_execute : 1; ///< bit 63 prevents execution of page by kernel programs
} __attribute__((packed)) memory_page_entry_t;

/**
 * @struct memory_page_table_t
 * @brief page table struct
 *
 * in long mode a page table is consist of 512 page entries.
 * a page table is 4K page aligned.
 */
typedef struct {
	memory_page_entry_t pages[512]; ///< page table entries
} __attribute__((packed)) memory_page_table_t; ///< short hand for struct


/**
 * @struct memory_heap
 * @brief heap interface for all types
 */
typedef struct memory_heap {
	uint32_t header; ///< heap header custom values
	void* metadata; ///< internal heap metadata filled by heap implementation
	void* (*malloc)(struct memory_heap*, size_t, size_t); ///< malloc function of heap implementation
	uint8_t (* free)(struct memory_heap*, void*); ///< free function of heap implementation
} memory_heap_t; ///< short hand for struct

/**
 * @brief creates simple heap
 * @param[in]  start start address of heap
 * @param[in]  end   end address of heap
 * @return       heap
 */
memory_heap_t* memory_create_heap_simple(size_t start, size_t end);

/**
 * @brief sets default heap
 * @param[in]  heap the heap will be the default one
 * @return old default heap
 */
memory_heap_t* memory_set_default_heap(memory_heap_t* heap);

/**
 * @brief malloc memory
 * @param[in] heap  the heap used for malloc
 * @param[in] size  size of variable
 * @param[in] align address of variable aligned at.
 * @return address of variable
 *
 * if heap is NULL, variable allocated at default heap.
 */
void* memory_malloc_ext(struct memory_heap* heap, size_t size, size_t align);

/*! malloc with size s at default heap without aligned */
#define memory_malloc(s) memory_malloc_ext(NULL, s, 0x0)
/*! malloc with size s at default heap with aligned a */
#define memory_malloc_aligned(s, a) memory_malloc_ext(NULL, s, a)

/**
 * @brief frees memory
 * @param[in]  heap  the heap where the address is.
 * @param[in]  address address to free
 * @return  0 if successed.
 *
 * if heap is NULL, address will be freed at default heap
 */
uint8_t memory_free_ext(struct memory_heap* heap, void* address);
/*! frees memory addr at default heap */
#define memory_free(addr) memory_free_ext(NULL, addr)

/**
 * @brief sets memory with value
 * @param[in]  address the address to be setted.
 * @param[in]  value   the value
 * @param[in] size    repeat count
 * @return 0
 */
uint8_t memory_memset(void* address, uint8_t value, size_t size);
/*! calls memory_memset with data value as NULL */
#define memory_memclean(addr, s) memory_memset(addr, NULL, s)

/**
 * @brief copy source to destination with length bytes from source
 * @param[in]  source      source address
 * @param[in]  destination destination address
 * @param[in]  length      how many bytes will be copied
 * @return 0
 *
 * if destination is smaller then length a memory corruption will be happend
 */
uint8_t memory_memcopy(void* source, void* destination, size_t length);

/**
 * @brief compares first length bytes of mem1 with mem2
 * @param  mem1   first memory address
 * @param  mem2   second memory address
 * @param  length count of bytes for compare
 * @return       <0 if mem1>mem2, 0 if mem1=mem2, >0 if mem1>mem2
 */
int8_t memory_memcompare(void* mem1, void* mem2, size_t length);

/**
 * @brief detect memory map
 * @param[out]  map detected memory map
 * @return      entry count
 *
 * detects memory map with bios interrupt
 */
size_t memory_detect_map(memory_map_t** map);

/**
 * @brief converts relative address to absolute address
 * @param[in]  raddr relative address
 * @return       absolute address
 *
 * meaningful at real mode
 */
size_t memory_get_absolute_address(size_t raddr);
/**
 * @brief converts absolute address to relative address
 * @param[in]  aaddr absolute address
 * @return       relative address
 *
 * meaningful at real mode
 */
size_t memory_get_relative_address(size_t aaddr);

/**
 * @enum memory_paging_page_type_t
 * @brief page type enum.
 */
typedef enum {
	MEMORY_PAGING_PAGE_TYPE_4K, ///< 4k page
	MEMORY_PAGING_PAGE_TYPE_2M, ///< 2m page aka hugepage
	MEMORY_PAGING_PAGE_TYPE_1G ///< 1g page aka big hugepage
} memory_paging_page_type_t; ///< short hand for enum

/**
 * @brief build default page table
 * @param[in]  heap the heap where pages are to be in.
 * @return      p4 page table
 *
 * if heap is NULL page tables will be created at default heap
 */
memory_page_table_t* memory_paging_build_table_ext(memory_heap_t* heap);
/*! creates page table at default heap (mallocs are at default heap) */
#define memory_paging_build_table() memory_paging_build_table_ext(NULL)

/**
 * @brief switches p4 page table
 * @param[in]  new_table new p4 table
 * @return        old p4 table
 *
 * change value of cr3 register.
 * if new_table is NULL returns only current table
 */
memory_page_table_t* memory_paging_switch_table(const memory_page_table_t* new_table);
/*! return p4 table */
#define memory_paging_get_table() memory_paging_switch_table(NULL)

/**
 * @brief creates virtual address frame mapping with adding page
 * @param  heap            the heap where variables will be created in
 * @param  p4              p4 page table
 * @param  virtual_address virtual start address of page
 * @param  frame_adress    frame address of page links
 * @param  type            page type, see also \ref memory_paging_page_type_t
 * @return  0 if successed
 *
 * if heap is NULL, the pages created in default heap
 */
uint8_t memory_paging_add_page_ext(memory_heap_t* heap, memory_page_table_t* p4,
                                   uint64_t virtual_address, uint64_t frame_adress,
                                   memory_paging_page_type_t type);
/*! add virtual address va to pt page table with frame address fa and page type t uses default heap for mallocs */
#define memory_paging_add_page(pt, va, fa, t)  memory_paging_add_page_ext(NULL, pt, va, fa, t)

#if ___BITS == 16 || DOXYGEN
/*! gets p4 index of virtual address at real mode*/
#define MEMORY_PT_GET_P4_INDEX(u64) ((u64.part_high >> 7) & 0x1FF)
/*! gets p3 index of virtual address at real mode  */
#define MEMORY_PT_GET_P3_INDEX(u64) (((u64.part_high & 0x7F) << 2) | ((u64.part_low >> 31) & 0x3))
/*! gets p2 index of virtual address at real mode */
#define MEMORY_PT_GET_P2_INDEX(u64) ((u64.part_low >> 21) & 0x1FF)
/*! gets p1 index of virtual address at real mode */
#define MEMORY_PT_GET_P1_INDEX(u64) ((u64.part_low >> 12) & 0x1FF)
#elif ___BITS == 64 || DOXYGEN
/*! gets p4 index of virtual address at long mode */
#define MEMORY_PT_GET_P4_INDEX(u64) ((u64 >> 39) & 0x1FF)
/*! gets p3 index of virtual address at long mode */
#define MEMORY_PT_GET_P3_INDEX(u64) ((u64 >> 30) & 0x1FF)
/*! gets p2 index of virtual address at long mode */
#define MEMORY_PT_GET_P2_INDEX(u64) ((u64 >> 21) & 0x1FF)
/*! gets p1 index of virtual address at long mode */
#define MEMORY_PT_GET_P1_INDEX(u64) ((u64 >> 12) & 0x1FF)
#endif


#endif
